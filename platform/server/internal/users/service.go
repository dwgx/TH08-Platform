package users

import (
	"context"
	"errors"
	"strings"
	"time"

	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/jackc/pgx/v5"
)

type Service struct{ db *db.DB }

func NewService(d *db.DB) *Service { return &Service{db: d} }

// User is the internal shape pulled from the users table. Avoid shipping
// this struct directly to the client — use ToProfile() for public views.
type User struct {
	UID          int64
	Username     string
	Handle       string
	Email        *string
	EmailVerifiedAt *time.Time
	PasswordHash *string
	AvatarURL    *string
	Bio          *string
	Tags         []string
	Locale       string
	Country      *string
	Status       string
	Roles        []string
	CreatedAt    time.Time
	UpdatedAt    time.Time
	LastSeenAt   *time.Time
}

// Profile is the public-safe shape (no email/password/etc).
type Profile struct {
	UID         string    `json:"uid"`
	Username    string    `json:"username"`
	Handle      string    `json:"handle"`
	AvatarURL   *string   `json:"avatar_url,omitempty"`
	Bio         *string   `json:"bio,omitempty"`
	Tags        []string  `json:"tags"`
	Locale      string    `json:"locale"`
	Country     *string   `json:"country,omitempty"`
	Roles       []string  `json:"roles"`
	CreatedAt   time.Time `json:"created_at"`
	LastSeenAt  *time.Time `json:"last_seen_at,omitempty"`
}

func (u *User) ToProfile() *Profile {
	return &Profile{
		UID:        formatUID(u.UID),
		Username:   u.Username,
		Handle:     u.Handle,
		AvatarURL:  u.AvatarURL,
		Bio:        u.Bio,
		Tags:       u.Tags,
		Locale:     u.Locale,
		Country:    u.Country,
		Roles:      u.Roles,
		CreatedAt:  u.CreatedAt,
		LastSeenAt: u.LastSeenAt,
	}
}

func (s *Service) FindByUID(ctx context.Context, uid int64) (*User, error) {
	row := s.db.Pool.QueryRow(ctx, selectUserCols+` WHERE uid = $1 AND deleted_at IS NULL`, uid)
	return scanUser(row)
}

func (s *Service) FindByHandle(ctx context.Context, handle string) (*User, error) {
	row := s.db.Pool.QueryRow(ctx, selectUserCols+` WHERE handle = $1 AND deleted_at IS NULL`, handle)
	return scanUser(row)
}

func (s *Service) FindByEmail(ctx context.Context, email string) (*User, error) {
	row := s.db.Pool.QueryRow(ctx, selectUserCols+` WHERE email = $1 AND deleted_at IS NULL`, strings.ToLower(email))
	return scanUser(row)
}

// FindByIdentifier accepts either an email (contains '@') or a handle.
// Used by /auth/login-email to allow either.
func (s *Service) FindByIdentifier(ctx context.Context, ident string) (*User, error) {
	if strings.Contains(ident, "@") {
		return s.FindByEmail(ctx, ident)
	}
	return s.FindByHandle(ctx, ident)
}

const selectUserCols = `
SELECT uid, username, handle, email, email_verified_at, password_hash,
       avatar_url, bio, tags, locale, country, status, roles,
       created_at, updated_at, last_seen_at
FROM users
`

func scanUser(row pgx.Row) (*User, error) {
	u := &User{}
	err := row.Scan(
		&u.UID, &u.Username, &u.Handle, &u.Email, &u.EmailVerifiedAt,
		&u.PasswordHash, &u.AvatarURL, &u.Bio, &u.Tags, &u.Locale,
		&u.Country, &u.Status, &u.Roles,
		&u.CreatedAt, &u.UpdatedAt, &u.LastSeenAt,
	)
	if errors.Is(err, pgx.ErrNoRows) {
		return nil, nil
	}
	if err != nil { return nil, err }
	return u, nil
}

func formatUID(uid int64) string {
	var buf [20]byte
	i := len(buf)
	for n := uid; ; {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
		if n == 0 { break }
	}
	return string(buf[i:])
}
