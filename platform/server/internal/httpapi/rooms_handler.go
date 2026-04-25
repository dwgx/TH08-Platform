package httpapi

import (
	"net/http"
	"strconv"
	"time"

	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/jackc/pgx/v5"
)

// RoomsService — V1 scope: List (real) and the rest still stubbed until
// the full rooms service lands.
type RoomsService struct{ DB *db.DB }

type roomOut struct {
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
	HasPassword    bool    `json:"has_password"`
	CreatedAt      string  `json:"created_at"`
	LastActiveAt   string  `json:"last_active_at"`
}

type roomsListOut struct {
	Items []roomOut `json:"items"`
}

// List — GET /api/v1/rooms  (supports ?kind=&game=&state=&limit=)
func (s *RoomsService) List(w http.ResponseWriter, r *http.Request) {
	q := r.URL.Query()
	limit := 50
	if v, err := strconv.Atoi(q.Get("limit")); err == nil && v > 0 && v <= 100 {
		limit = v
	}

	sql := `
	SELECT r.id::text, r.code, r.name, r.kind, r.visibility, r.join_mode, r.state, r.region,
	       r.game_id, r.host_uid, r.player_count, r.max_players, r.spectator_count, r.max_spectators,
	       r.password_hash IS NOT NULL AS has_password,
	       r.created_at, r.last_active_at,
	       u.username, u.handle, COALESCE(u.country, '')
	FROM rooms r
	JOIN users u ON u.uid = r.host_uid
	WHERE r.closed_at IS NULL`
	args := []any{}
	i := 1
	if v := q.Get("kind"); v != "" {
		sql += " AND r.kind = $" + strconv.Itoa(i)
		args = append(args, v)
		i++
	}
	if v := q.Get("game"); v != "" {
		sql += " AND r.game_id = $" + strconv.Itoa(i)
		args = append(args, v)
		i++
	}
	if v := q.Get("state"); v != "" {
		sql += " AND r.state = $" + strconv.Itoa(i)
		args = append(args, v)
		i++
	}
	sql += ` ORDER BY (r.state = 'playing') DESC, r.last_active_at DESC LIMIT ` + strconv.Itoa(limit)

	rows, err := s.DB.Pool.Query(r.Context(), sql, args...)
	if err != nil { WriteError(w, ErrInternal); return }
	defer rows.Close()

	out := roomsListOut{Items: []roomOut{}}
	for rows.Next() {
		var rm roomOut
		var hostUID int64
		var hostUser, hostHandle, hostCountry string
		var createdAt, lastActiveAt time.Time
		if err := rows.Scan(
			&rm.ID, &rm.Code, &rm.Name, &rm.Kind, &rm.Visibility, &rm.JoinMode, &rm.State, &rm.Region,
			&rm.GameID, &hostUID, &rm.PlayerCount, &rm.MaxPlayers, &rm.SpectatorCount, &rm.MaxSpectators,
			&rm.HasPassword, &createdAt, &lastActiveAt,
			&hostUser, &hostHandle, &hostCountry,
		); err != nil { continue }
		rm.Host = demoUser{UID: int64ToStr(hostUID), Username: hostUser, Handle: hostHandle, Country: hostCountry}
		rm.CreatedAt = createdAt.UTC().Format(time.RFC3339)
		rm.LastActiveAt = lastActiveAt.UTC().Format(time.RFC3339)
		out.Items = append(out.Items, rm)
	}
	WriteOK(w, out)
	_ = pgx.ErrNoRows // keep import stable
}

// Stubs — implement as real handlers land (rooms service).
func (s *RoomsService) Create(w http.ResponseWriter, r *http.Request)       { WriteError(w, ErrInternal) }
func (s *RoomsService) Get(w http.ResponseWriter, r *http.Request)          { WriteError(w, ErrInternal) }
func (s *RoomsService) Update(w http.ResponseWriter, r *http.Request)       { WriteError(w, ErrInternal) }
func (s *RoomsService) Join(w http.ResponseWriter, r *http.Request)         { WriteError(w, ErrInternal) }
func (s *RoomsService) Leave(w http.ResponseWriter, r *http.Request)        { WriteError(w, ErrInternal) }
func (s *RoomsService) Kick(w http.ResponseWriter, r *http.Request)         { WriteError(w, ErrInternal) }
func (s *RoomsService) Invite(w http.ResponseWriter, r *http.Request)       { WriteError(w, ErrInternal) }
func (s *RoomsService) ApproveJoin(w http.ResponseWriter, r *http.Request)  { WriteError(w, ErrInternal) }
func (s *RoomsService) Start(w http.ResponseWriter, r *http.Request)        { WriteError(w, ErrInternal) }
func (s *RoomsService) Pause(w http.ResponseWriter, r *http.Request)        { WriteError(w, ErrInternal) }
func (s *RoomsService) Resume(w http.ResponseWriter, r *http.Request)       { WriteError(w, ErrInternal) }
func (s *RoomsService) End(w http.ResponseWriter, r *http.Request)          { WriteError(w, ErrInternal) }
func (s *RoomsService) MatchResult(w http.ResponseWriter, r *http.Request)  { WriteError(w, ErrInternal) }
func (s *RoomsService) SetCharacter(w http.ResponseWriter, r *http.Request) { WriteError(w, ErrInternal) }
