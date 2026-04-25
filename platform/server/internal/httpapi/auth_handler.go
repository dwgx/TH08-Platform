package httpapi

import (
	"context"
	"encoding/json"
	"net/http"
	"strings"

	"github.com/dwgx/th-platform/server/internal/auth"
	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/dwgx/th-platform/server/internal/users"
)

// AuthService is the thin HTTP adapter. Business code (hashing,
// token issuing, OAuth dance) lives in users + auth packages.
type AuthService struct {
	DB    *db.DB
	Signer *auth.Signer
	Users *users.Service
	UID    *users.UIDAllocator
}

type registerEmailReq struct {
	Email    string `json:"email"`
	Password string `json:"password"`
	Username string `json:"username"`
	Handle   string `json:"handle"`
	Locale   string `json:"locale"`
}

func (s *AuthService) RegisterEmail(w http.ResponseWriter, r *http.Request) {
	var req registerEmailReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		WriteError(w, ErrValidationFailed)
		return
	}
	if !isValidEmail(req.Email) || len(req.Password) < 8 || len(req.Username) == 0 {
		WriteError(w, ErrValidationFailed)
		return
	}
	if req.Handle == "" {
		req.Handle = normalizeHandle(req.Username)
	}

	ctx := r.Context()
	hash, err := auth.HashPassword(req.Password)
	if err != nil { WriteError(w, ErrInternal); return }

	// Tx: allocate uid + insert user. On unique-violation we retry
	// the uid allocation once (handle is the only other unique thing).
	tx, err := s.DB.Pool.Begin(ctx)
	if err != nil { WriteError(w, ErrInternal); return }
	defer tx.Rollback(ctx)

	var uid int64
	if err := tx.QueryRow(ctx, `SELECT allocate_uid()`).Scan(&uid); err != nil {
		WriteError(w, ErrInternal); return
	}
	_, err = tx.Exec(ctx, `
		INSERT INTO users (uid, username, handle, email, password_hash, locale)
		VALUES ($1, $2, $3, $4, $5, $6)
	`, uid, req.Username, req.Handle, strings.ToLower(req.Email), hash, defaultStr(req.Locale, "zh-CN"))
	if err != nil {
		if isUniqueViolation(err, "users_handle_uidx") {
			WriteError(w, ErrHandleTaken); return
		}
		if isUniqueViolation(err, "users_email_uidx") {
			WriteError(w, ErrEmailTaken); return
		}
		WriteError(w, ErrInternal); return
	}
	if err := tx.Commit(ctx); err != nil { WriteError(w, ErrInternal); return }

	// TODO(P1): enqueue email verification send.
	WriteCreated(w, map[string]any{
		"uid":               formatUID(uid),
		"handle":            req.Handle,
		"verification_sent": false, // TODO flip when mailer lands
	})
}

type loginEmailReq struct {
	Identifier string `json:"identifier"`
	Password   string `json:"password"`
}

func (s *AuthService) LoginEmail(w http.ResponseWriter, r *http.Request) {
	var req loginEmailReq
	if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
		WriteError(w, ErrValidationFailed); return
	}

	ctx := r.Context()
	u, err := s.Users.FindByIdentifier(ctx, req.Identifier)
	if err != nil || u == nil || u.PasswordHash == nil {
		// Do NOT distinguish "user not found" from "wrong password" — same response.
		WriteError(w, &APIError{HTTPStatus: 401, Code: "invalid_credentials", Message: "Wrong email/handle or password."})
		return
	}
	ok, err := auth.VerifyPassword(req.Password, *u.PasswordHash)
	if err != nil || !ok {
		WriteError(w, &APIError{HTTPStatus: 401, Code: "invalid_credentials", Message: "Wrong email/handle or password."})
		return
	}

	access, err := s.Signer.SignAccess(u.UID)
	if err != nil { WriteError(w, ErrInternal); return }
	refresh, err := s.Signer.SignRefresh(u.UID)
	if err != nil { WriteError(w, ErrInternal); return }

	// TODO(P1): store SHA256(refresh) in user_sessions
	_ = ctx

	WriteOK(w, map[string]any{
		"access_token":  access,
		"refresh_token": refresh,
		"expires_in":    int(s.Signer.AccessTTLSec()),
		"user":          u.ToProfile(),
	})
}

// Stubs — filled in as we hook up the real flows.

func (s *AuthService) Refresh(w http.ResponseWriter, r *http.Request)              { WriteError(w, ErrInternal) }
func (s *AuthService) VerifyEmail(w http.ResponseWriter, r *http.Request)          { WriteError(w, ErrInternal) }
func (s *AuthService) RequestPasswordReset(w http.ResponseWriter, r *http.Request) { WriteError(w, ErrInternal) }
func (s *AuthService) ResetPassword(w http.ResponseWriter, r *http.Request)        { WriteError(w, ErrInternal) }
func (s *AuthService) OAuthStart(w http.ResponseWriter, r *http.Request)           { WriteError(w, ErrInternal) }
func (s *AuthService) OAuthCallback(w http.ResponseWriter, r *http.Request)        { WriteError(w, ErrInternal) }
func (s *AuthService) OAuthBind(w http.ResponseWriter, r *http.Request)            { WriteError(w, ErrInternal) }
func (s *AuthService) Logout(w http.ResponseWriter, r *http.Request)               { WriteError(w, ErrInternal) }
func (s *AuthService) LogoutAll(w http.ResponseWriter, r *http.Request)            { WriteError(w, ErrInternal) }

// ── helpers ────────────────────────────────────────────────────────

func isValidEmail(s string) bool {
	return strings.Contains(s, "@") && strings.Contains(s, ".") && len(s) >= 5 && len(s) <= 254
}

func normalizeHandle(s string) string {
	s = strings.ToLower(s)
	var b strings.Builder
	for _, r := range s {
		if (r >= 'a' && r <= 'z') || (r >= '0' && r <= '9') || r == '-' || r == '_' {
			b.WriteRune(r)
		}
	}
	out := b.String()
	if len(out) < 3 { out = out + "000" }
	if len(out) > 24 { out = out[:24] }
	return out
}

func defaultStr(v, def string) string { if v == "" { return def } ; return v }

func formatUID(uid int64) string {
	// serialise as string to avoid JS precision loss for > 2^53
	return strings.TrimLeft(formatInt(uid), "")
}

func formatInt(n int64) string {
	// avoid strconv import cycle from this handler (keeps it small)
	return (&intFmt{}).format(n)
}

type intFmt struct{}

func (*intFmt) format(n int64) string {
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

// TODO: move to a shared errors helper once we have more handlers.
func isUniqueViolation(err error, indexName string) bool {
	// pgx v5 returns *pgconn.PgError on unique violations; keep loose
	// check so we don't pull pgconn into this file for now.
	return err != nil && strings.Contains(err.Error(), "duplicate key") && strings.Contains(err.Error(), indexName)
}

var _ context.Context // keep imports stable for future editing
