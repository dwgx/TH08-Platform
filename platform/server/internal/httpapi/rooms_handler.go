package httpapi

import (
	"net/http"

	"github.com/dwgx/th-platform/server/internal/db"
)

// RoomsService — skeleton. Each method will be fleshed out alongside the
// rooms/service.go business-logic package (mirrors users/service.go).
type RoomsService struct{ DB *db.DB }

func (s *RoomsService) List(w http.ResponseWriter, r *http.Request)         { WriteError(w, ErrInternal) }
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
