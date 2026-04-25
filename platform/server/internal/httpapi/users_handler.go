package httpapi

import (
	"net/http"

	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/dwgx/th-platform/server/internal/users"
)

type UsersService struct {
	DB    *db.DB
	Users *users.Service
}

func (s *UsersService) GetMe(w http.ResponseWriter, r *http.Request) {
	uid, _ := UIDFromCtx(r.Context())
	u, err := s.Users.FindByUID(r.Context(), uid)
	if err != nil { WriteError(w, ErrInternal); return }
	if u == nil   { WriteError(w, ErrNotFound);  return }
	WriteOK(w, u.ToProfile())
}

// Stubs — implement as real handlers land.
func (s *UsersService) UpdateMe(w http.ResponseWriter, r *http.Request)      { WriteError(w, ErrInternal) }
func (s *UsersService) UpdateAvatar(w http.ResponseWriter, r *http.Request)  { WriteError(w, ErrInternal) }
func (s *UsersService) GetByUID(w http.ResponseWriter, r *http.Request)      { WriteError(w, ErrInternal) }
func (s *UsersService) GetByHandle(w http.ResponseWriter, r *http.Request)   { WriteError(w, ErrInternal) }
func (s *UsersService) Search(w http.ResponseWriter, r *http.Request)        { WriteError(w, ErrInternal) }
func (s *UsersService) ListGames(w http.ResponseWriter, r *http.Request)     { WriteError(w, ErrInternal) }
func (s *UsersService) GetGame(w http.ResponseWriter, r *http.Request)       { WriteError(w, ErrInternal) }
