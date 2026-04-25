// Lobby public chat — authoritative store in Postgres, live fanout via
// the WebSocket hub on topic "lobby".
package httpapi

import (
	"encoding/json"
	"net/http"
	"strconv"
	"time"

	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/dwgx/th-platform/server/internal/ws"
	"github.com/jackc/pgx/v5"
)

type LobbyService struct {
	DB  *db.DB
	Hub *ws.Hub
}

type lobbyMsgOut struct {
	ID        int64    `json:"id"`
	Sender    demoUser `json:"sender"` // reuse the compact user shape
	Content   string   `json:"content"`
	Kind      string   `json:"kind"`
	CreatedAt string   `json:"created_at"`
}

type lobbyListOut struct {
	Items []lobbyMsgOut `json:"items"`
}

// ListMessages — GET /api/v1/lobby/messages?limit=100&before=<id>
// Public-ish: still behind auth in the router, but unauthenticated users
// can get a preview via /demo/snapshot. This is the real endpoint.
func (s *LobbyService) ListMessages(w http.ResponseWriter, r *http.Request) {
	limit := 50
	if v, err := strconv.Atoi(r.URL.Query().Get("limit")); err == nil && v > 0 && v <= 200 {
		limit = v
	}
	before := int64(0)
	if v, err := strconv.ParseInt(r.URL.Query().Get("before"), 10, 64); err == nil && v > 0 {
		before = v
	}

	ctx := r.Context()
	var rows pgx.Rows
	var err error
	if before > 0 {
		rows, err = s.DB.Pool.Query(ctx, `
			SELECT lm.id, lm.content, lm.kind, lm.created_at, u.uid, u.username, u.handle
			FROM lobby_messages lm JOIN users u ON u.uid = lm.sender_uid
			WHERE lm.id < $1
			ORDER BY lm.id DESC LIMIT $2`, before, limit)
	} else {
		rows, err = s.DB.Pool.Query(ctx, `
			SELECT lm.id, lm.content, lm.kind, lm.created_at, u.uid, u.username, u.handle
			FROM lobby_messages lm JOIN users u ON u.uid = lm.sender_uid
			ORDER BY lm.id DESC LIMIT $1`, limit)
	}
	if err != nil { WriteError(w, ErrInternal); return }
	defer rows.Close()

	out := lobbyListOut{Items: []lobbyMsgOut{}}
	for rows.Next() {
		var m lobbyMsgOut
		var uid int64
		var created time.Time
		var username, handle string
		if err := rows.Scan(&m.ID, &m.Content, &m.Kind, &created, &uid, &username, &handle); err != nil {
			continue
		}
		m.Sender = demoUser{UID: int64ToStr(uid), Username: username, Handle: handle}
		m.CreatedAt = created.UTC().Format(time.RFC3339)
		out.Items = append(out.Items, m)
	}
	// Reverse to chronological order (oldest first) for direct render.
	for i, j := 0, len(out.Items)-1; i < j; i, j = i+1, j-1 {
		out.Items[i], out.Items[j] = out.Items[j], out.Items[i]
	}
	WriteOK(w, out)
}

type postLobbyReq struct {
	Content string                 `json:"content"`
	Kind    string                 `json:"kind"`
	Meta    map[string]interface{} `json:"meta"`
}

// PostMessage — POST /api/v1/lobby/messages
// Requires auth (uid lifted from ctx).
func (s *LobbyService) PostMessage(w http.ResponseWriter, r *http.Request) {
	uid, ok := UIDFromCtx(r.Context())
	if !ok { WriteError(w, ErrAuthRequired); return }

	var req postLobbyReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		WriteError(w, ErrValidationFailed); return
	}
	if len(req.Content) == 0 || len(req.Content) > 2000 {
		WriteError(w, ErrValidationFailed); return
	}
	if req.Kind == "" { req.Kind = "text" }
	if !validLobbyKind(req.Kind) {
		WriteError(w, ErrValidationFailed); return
	}

	metaJSON := []byte("{}")
	if req.Meta != nil {
		if b, err := json.Marshal(req.Meta); err == nil {
			metaJSON = b
		}
	}

	var id int64
	var createdAt time.Time
	err := s.DB.Pool.QueryRow(r.Context(), `
		INSERT INTO lobby_messages (sender_uid, content, kind, meta)
		VALUES ($1, $2, $3, $4)
		RETURNING id, created_at
	`, uid, req.Content, req.Kind, string(metaJSON)).Scan(&id, &createdAt)
	if err != nil { WriteError(w, ErrInternal); return }

	// Also pull the sender's profile bits for the response + broadcast.
	var username, handle string
	_ = s.DB.Pool.QueryRow(r.Context(),
		`SELECT username, handle FROM users WHERE uid = $1`, uid).Scan(&username, &handle)

	msg := lobbyMsgOut{
		ID:        id,
		Sender:    demoUser{UID: int64ToStr(uid), Username: username, Handle: handle},
		Content:   req.Content,
		Kind:      req.Kind,
		CreatedAt: createdAt.UTC().Format(time.RFC3339),
	}

	// Live fanout to every client subscribed to "lobby".
	s.Hub.PublishTopic("lobby", map[string]any{
		"type":    "lobby.message",
		"message": msg,
	})

	WriteCreated(w, msg)
}

func validLobbyKind(k string) bool {
	switch k {
	case "text", "emoji", "room_invite", "system":
		return true
	}
	return false
}
