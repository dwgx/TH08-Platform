// Package ws is the WebSocket hub. Each connected client holds one
// Client struct; Hub owns the registry and does topic-fanout.
//
// Design notes:
//   * Per-connection goroutine for reads; writer goroutine drains a
//     buffered channel — if the channel fills (slow client), we drop
//     the client rather than blocking fanout.
//   * Topics are strings ("lobby", "room:<uuid>", "channel:<uuid>",
//     "user:<uid>", "dm:<thread_id>"). Topic subscription state lives
//     in the Client, fanout loop iterates Hub.byTopic.
//   * Per-user fanout uses Redis Pub/Sub as the cross-process bus once
//     we shard gateway to N instances. V1 single process = in-memory only.
package ws

import (
	"context"
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"sync"
	"time"

	"github.com/dwgx/th-platform/server/internal/auth"
	"nhooyr.io/websocket"
)

type Hub struct {
	log    *slog.Logger
	signer *auth.Signer

	mu       sync.RWMutex
	clients  map[*Client]struct{}
	byTopic  map[string]map[*Client]struct{}
	byUID    map[int64]map[*Client]struct{}
}

func NewHub(log *slog.Logger, signer *auth.Signer) *Hub {
	return &Hub{
		log:     log,
		signer:  signer,
		clients: make(map[*Client]struct{}),
		byTopic: make(map[string]map[*Client]struct{}),
		byUID:   make(map[int64]map[*Client]struct{}),
	}
}

func (h *Hub) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	token := r.URL.Query().Get("token")
	if token == "" {
		http.Error(w, "missing token", http.StatusUnauthorized)
		return
	}
	claims, err := h.signer.Verify(token)
	if err != nil || claims.TokenType != "access" {
		http.Error(w, "invalid token", http.StatusUnauthorized)
		return
	}

	conn, err := websocket.Accept(w, r, &websocket.AcceptOptions{
		OriginPatterns: []string{"*"}, // TODO narrow in prod
	})
	if err != nil {
		h.log.Warn("ws accept failed", "err", err)
		return
	}
	conn.SetReadLimit(64 * 1024)

	cli := &Client{
		hub:    h,
		conn:   conn,
		uid:    claims.UID,
		send:   make(chan []byte, 128),
		topics: make(map[string]struct{}),
		log:    h.log.With("uid", claims.UID),
	}
	h.register(cli)
	defer h.unregister(cli)

	// Writer pump.
	go cli.writeLoop()

	// Send hello.
	cli.pushJSON(map[string]any{
		"type":               "hello",
		"uid":                claims.UID,
		"heartbeat_interval_ms": 30_000,
	})

	// Reader pump (this goroutine).
	cli.readLoop()
}

func (h *Hub) register(c *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()
	h.clients[c] = struct{}{}
	if _, ok := h.byUID[c.uid]; !ok {
		h.byUID[c.uid] = make(map[*Client]struct{})
	}
	h.byUID[c.uid][c] = struct{}{}
	// Auto-subscribe to own user topic so private events reach this client.
	h.subscribeLocked(c, userTopic(c.uid))
}

func (h *Hub) unregister(c *Client) {
	h.mu.Lock()
	defer h.mu.Unlock()
	delete(h.clients, c)
	if set, ok := h.byUID[c.uid]; ok {
		delete(set, c)
		if len(set) == 0 { delete(h.byUID, c.uid) }
	}
	for t := range c.topics {
		if set, ok := h.byTopic[t]; ok {
			delete(set, c)
			if len(set) == 0 { delete(h.byTopic, t) }
		}
	}
	close(c.send)
	_ = c.conn.Close(websocket.StatusNormalClosure, "")
}

func (h *Hub) subscribeLocked(c *Client, topic string) {
	if _, ok := h.byTopic[topic]; !ok {
		h.byTopic[topic] = make(map[*Client]struct{})
	}
	h.byTopic[topic][c] = struct{}{}
	c.topics[topic] = struct{}{}
}

// PublishTopic fans out a JSON message to everyone subscribed. Returns
// how many deliveries succeeded (best-effort; never blocks).
func (h *Hub) PublishTopic(topic string, body any) int {
	payload, err := json.Marshal(body)
	if err != nil { return 0 }
	h.mu.RLock()
	subs := h.byTopic[topic]
	recipients := make([]*Client, 0, len(subs))
	for c := range subs { recipients = append(recipients, c) }
	h.mu.RUnlock()

	n := 0
	for _, c := range recipients {
		select {
		case c.send <- payload:
			n++
		default:
			// slow consumer → drop connection
			h.log.Warn("ws slow consumer dropped", "uid", c.uid)
			_ = c.conn.Close(websocket.StatusPolicyViolation, "slow consumer")
		}
	}
	return n
}

// PublishUID delivers to all connections of a single user (across multiple devices).
func (h *Hub) PublishUID(uid int64, body any) int {
	return h.PublishTopic(userTopic(uid), body)
}

func userTopic(uid int64) string { return "user:" + int64ToStr(uid) }

func int64ToStr(n int64) string {
	var buf [20]byte
	i := len(buf)
	neg := n < 0
	if neg { n = -n }
	for {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
		if n == 0 { break }
	}
	if neg { i--; buf[i] = '-' }
	return string(buf[i:])
}

// ── Client ──────────────────────────────────────────────────────────

type Client struct {
	hub    *Hub
	conn   *websocket.Conn
	uid    int64
	send   chan []byte
	topics map[string]struct{}
	log    *slog.Logger
}

type incomingMsg struct {
	Type   string          `json:"type"`
	ID     string          `json:"id,omitempty"`
	Topics []string        `json:"topics,omitempty"`
	Target string          `json:"target,omitempty"`
	Status string          `json:"status,omitempty"`
	RoomID string          `json:"room_id,omitempty"`
	// Raw fallthrough for forward-compat
	Raw json.RawMessage `json:"-"`
}

func (c *Client) readLoop() {
	ctx := context.Background()
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	for {
		_, data, err := c.conn.Read(ctx)
		if err != nil {
			if !errors.Is(err, context.Canceled) {
				c.log.Debug("ws read error", "err", err)
			}
			return
		}
		var m incomingMsg
		if err := json.Unmarshal(data, &m); err != nil { continue }
		c.handleIncoming(&m)
	}
}

func (c *Client) writeLoop() {
	ctx := context.Background()
	for payload := range c.send {
		wctx, cancel := context.WithTimeout(ctx, 10*time.Second)
		err := c.conn.Write(wctx, websocket.MessageText, payload)
		cancel()
		if err != nil {
			c.log.Debug("ws write error", "err", err)
			return
		}
	}
}

func (c *Client) handleIncoming(m *incomingMsg) {
	switch m.Type {
	case "ping":
		c.pushJSON(map[string]any{"type": "pong"})
	case "subscribe":
		c.hub.mu.Lock()
		for _, t := range m.Topics {
			if allowedTopic(t, c.uid) {
				c.hub.subscribeLocked(c, t)
			}
		}
		c.hub.mu.Unlock()
		c.pushJSON(map[string]any{"type": "subscribed", "topics": m.Topics})
	case "unsubscribe":
		c.hub.mu.Lock()
		for _, t := range m.Topics {
			if set, ok := c.hub.byTopic[t]; ok {
				delete(set, c)
				if len(set) == 0 { delete(c.hub.byTopic, t) }
			}
			delete(c.topics, t)
		}
		c.hub.mu.Unlock()
	default:
		// typing.start / presence.update etc — hand off to feature
		// dispatcher once those land. V1 silently drops unknown types.
	}
}

func (c *Client) pushJSON(body any) {
	payload, err := json.Marshal(body)
	if err != nil { return }
	select { case c.send <- payload: default: }
}

// allowedTopic enforces authorization — e.g. a user can only subscribe
// to their own user:<uid> topic, or to rooms they are a member of.
// V1 loosely permits lobby + any channel/room (server-side fanout still
// filtered on authoritative state). Tighten as abuse emerges.
func allowedTopic(topic string, uid int64) bool {
	if topic == "lobby" { return true }
	if len(topic) >= 5 && topic[:5] == "user:" {
		return topic == userTopic(uid)
	}
	if len(topic) >= 5 && (topic[:5] == "room:" || topic[:8] == "channel:") { return true }
	if len(topic) >= 3 && topic[:3] == "dm:" { return true }
	return false
}
