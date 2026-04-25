// Demo handler — public endpoints that return read-only platform state
// for the live preview at http://<host>/. Bypasses auth so visitors can
// browse the lobby. Production builds should disable these.
package httpapi

import (
	"context"
	"net/http"
	"time"

	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/jackc/pgx/v5"
)

type DemoService struct{ DB *db.DB }

type demoSnapshot struct {
	Stats struct {
		Users          int    `json:"users"`
		ActiveRooms    int    `json:"active_rooms"`
		LobbyMessages  int    `json:"lobby_messages"`
		Groups         int    `json:"groups"`
		ServerStartUTC string `json:"server_start_utc"`
		Version        string `json:"version"`
	} `json:"stats"`
	Rooms          []demoRoom    `json:"rooms"`
	LobbyMessages  []demoMessage `json:"lobby_messages"`
	OnlineSamples  []demoUser    `json:"online_samples"`
	Games          []demoGame    `json:"games"`
}

type demoRoom struct {
	ID             string  `json:"id"`
	Code           string  `json:"code"`
	Name           string  `json:"name"`
	Kind           string  `json:"kind"`
	Visibility     string  `json:"visibility"`
	JoinMode       string  `json:"join_mode"`
	State          string  `json:"state"`
	Region         *string `json:"region"`
	GameID         string  `json:"game_id"`
	Host           demoUser `json:"host"`
	PlayerCount    int     `json:"player_count"`
	MaxPlayers     int     `json:"max_players"`
	SpectatorCount int     `json:"spectator_count"`
	MaxSpectators  int     `json:"max_spectators"`
	CreatedAt      string  `json:"created_at"`
	LastActiveAt   string  `json:"last_active_at"`
}

type demoMessage struct {
	ID        int64    `json:"id"`
	Sender    demoUser `json:"sender"`
	Content   string   `json:"content"`
	Kind      string   `json:"kind"`
	CreatedAt string   `json:"created_at"`
}

type demoUser struct {
	UID      string `json:"uid"`
	Username string `json:"username"`
	Handle   string `json:"handle"`
	Country  string `json:"country,omitempty"`
}

type demoGame struct {
	ID          string `json:"id"`
	DisplayName string `json:"display_name"`
	Status      string `json:"status"`
}

var serverStart = time.Now().UTC().Format(time.RFC3339)

func (s *DemoService) Snapshot(w http.ResponseWriter, r *http.Request) {
	ctx, cancel := context.WithTimeout(r.Context(), 5*time.Second)
	defer cancel()

	snap := demoSnapshot{}
	snap.Stats.ServerStartUTC = serverStart
	snap.Stats.Version = "0.1.0-alpha"

	// Stats
	if err := s.DB.Pool.QueryRow(ctx,
		`SELECT COUNT(*) FROM users WHERE deleted_at IS NULL`).Scan(&snap.Stats.Users); err != nil {
		WriteError(w, ErrInternal); return
	}
	if err := s.DB.Pool.QueryRow(ctx,
		`SELECT COUNT(*) FROM rooms WHERE closed_at IS NULL`).Scan(&snap.Stats.ActiveRooms); err != nil {
		WriteError(w, ErrInternal); return
	}
	if err := s.DB.Pool.QueryRow(ctx,
		`SELECT COUNT(*) FROM lobby_messages`).Scan(&snap.Stats.LobbyMessages); err != nil {
		WriteError(w, ErrInternal); return
	}
	if err := s.DB.Pool.QueryRow(ctx,
		`SELECT COUNT(*) FROM groups WHERE deleted_at IS NULL`).Scan(&snap.Stats.Groups); err != nil {
		WriteError(w, ErrInternal); return
	}

	// Rooms
	rows, err := s.DB.Pool.Query(ctx, `
		SELECT r.id::text, r.code, r.name, r.kind, r.visibility, r.join_mode, r.state, r.region,
		       r.game_id, r.host_uid, r.player_count, r.max_players, r.spectator_count, r.max_spectators,
		       r.created_at, r.last_active_at,
		       u.username, u.handle, COALESCE(u.country, '')
		FROM rooms r
		JOIN users u ON u.uid = r.host_uid
		WHERE r.closed_at IS NULL
		ORDER BY (r.state = 'playing') DESC, r.last_active_at DESC
		LIMIT 30`)
	if err != nil { WriteError(w, ErrInternal); return }
	defer rows.Close()
	for rows.Next() {
		var rm demoRoom
		var hostUID int64
		var hostUser, hostHandle, hostCountry string
		var createdAt, lastActiveAt time.Time
		if err := rows.Scan(
			&rm.ID, &rm.Code, &rm.Name, &rm.Kind, &rm.Visibility, &rm.JoinMode, &rm.State, &rm.Region,
			&rm.GameID, &hostUID, &rm.PlayerCount, &rm.MaxPlayers, &rm.SpectatorCount, &rm.MaxSpectators,
			&createdAt, &lastActiveAt, &hostUser, &hostHandle, &hostCountry,
		); err != nil { continue }
		rm.Host = demoUser{
			UID: int64ToStr(hostUID), Username: hostUser, Handle: hostHandle, Country: hostCountry,
		}
		rm.CreatedAt = createdAt.UTC().Format(time.RFC3339)
		rm.LastActiveAt = lastActiveAt.UTC().Format(time.RFC3339)
		snap.Rooms = append(snap.Rooms, rm)
	}

	// Lobby messages
	mrows, err := s.DB.Pool.Query(ctx, `
		SELECT lm.id, lm.content, lm.kind, lm.created_at,
		       u.uid, u.username, u.handle
		FROM lobby_messages lm
		JOIN users u ON u.uid = lm.sender_uid
		ORDER BY lm.id DESC
		LIMIT 30`)
	if err != nil { WriteError(w, ErrInternal); return }
	defer mrows.Close()
	var msgs []demoMessage
	for mrows.Next() {
		var m demoMessage
		var uid int64
		var createdAt time.Time
		var username, handle string
		if err := mrows.Scan(&m.ID, &m.Content, &m.Kind, &createdAt,
			&uid, &username, &handle); err != nil { continue }
		m.Sender = demoUser{UID: int64ToStr(uid), Username: username, Handle: handle}
		m.CreatedAt = createdAt.UTC().Format(time.RFC3339)
		msgs = append(msgs, m)
	}
	// Reverse so we render oldest first.
	for i, j := 0, len(msgs)-1; i < j; i, j = i+1, j-1 { msgs[i], msgs[j] = msgs[j], msgs[i] }
	snap.LobbyMessages = msgs

	// Online samples (recent users by last_seen_at, fall back to created_at)
	urows, err := s.DB.Pool.Query(ctx, `
		SELECT uid, username, handle, COALESCE(country, '')
		FROM users
		WHERE deleted_at IS NULL
		ORDER BY GREATEST(COALESCE(last_seen_at, '1970-01-01'::timestamptz), created_at) DESC
		LIMIT 8`)
	if err == nil {
		defer urows.Close()
		for urows.Next() {
			var uid int64
			var username, handle, country string
			if err := urows.Scan(&uid, &username, &handle, &country); err != nil { continue }
			snap.OnlineSamples = append(snap.OnlineSamples,
				demoUser{UID: int64ToStr(uid), Username: username, Handle: handle, Country: country})
		}
	}

	// Games
	grows, err := s.DB.Pool.Query(ctx,
		`SELECT id, display_name, status FROM games ORDER BY family ASC`)
	if err == nil {
		defer grows.Close()
		for grows.Next() {
			var g demoGame
			if err := grows.Scan(&g.ID, &g.DisplayName, &g.Status); err != nil { continue }
			snap.Games = append(snap.Games, g)
		}
	}

	WriteOK(w, snap)
}

// Helper: avoid importing strconv just for this.
func int64ToStr(n int64) string {
	if n == 0 { return "0" }
	var buf [20]byte
	i := len(buf)
	neg := n < 0
	if neg { n = -n }
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	if neg { i--; buf[i] = '-' }
	return string(buf[i:])
}

// Compile-time use to silence unused import warnings in transition.
var _ pgx.Row
