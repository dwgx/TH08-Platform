package httpapi

import (
	"net/http"
	"time"

	"github.com/dwgx/th-platform/server/internal/auth"
	"github.com/dwgx/th-platform/server/internal/config"
	"github.com/dwgx/th-platform/server/internal/ws"
	"github.com/go-chi/chi/v5"
	"github.com/go-chi/chi/v5/middleware"
	"github.com/go-chi/cors"
	"github.com/go-chi/httprate"
)

// Deps bundles everything handlers need. Adding a new service → add a
// field here and wire it in main.go, no global singletons.
type Deps struct {
	Cfg    *config.Config
	Signer *auth.Signer
	Hub    *ws.Hub
	Auth   *AuthService
	Users  *UsersService
	Rooms  *RoomsService
	Demo   *DemoService
	// Friends, Groups, Channels, DMs, Lobby, Notifications, Games — add as they land
}

func NewRouter(d *Deps) http.Handler {
	r := chi.NewRouter()

	r.Use(middleware.RequestID)
	r.Use(middleware.RealIP)
	r.Use(middleware.Recoverer)
	r.Use(middleware.Timeout(30 * time.Second))

	r.Use(cors.Handler(cors.Options{
		AllowedOrigins:   d.Cfg.CORSAllowedOrigins,
		AllowedMethods:   []string{"GET", "POST", "PATCH", "PUT", "DELETE", "OPTIONS"},
		AllowedHeaders:   []string{"Authorization", "Content-Type", "X-Request-ID"},
		ExposedHeaders:   []string{"X-Request-ID"},
		AllowCredentials: true,
		MaxAge:           300,
	}))

	r.Use(httprate.LimitByIP(100, 1*time.Minute))

	// ── public ────────────────────────────────────────────────────
	r.Get("/healthz", func(w http.ResponseWriter, r *http.Request) {
		w.Write([]byte("ok"))
	})
	r.Get("/readyz", func(w http.ResponseWriter, r *http.Request) {
		// TODO: ping DB + Redis
		w.Write([]byte("ready"))
	})

	// Public demo preview — landing page + read-only snapshot.
	r.Get("/", Landing)
	r.Get("/demo/snapshot", d.Demo.Snapshot)

	r.Route("/api/v1", func(r chi.Router) {
		// ── unauth ─────────────────────────────────────────────────
		r.Group(func(r chi.Router) {
			r.Use(httprate.LimitByIP(20, 1*time.Minute)) // stricter on auth
			r.Route("/auth", func(r chi.Router) {
				r.Post("/register-email", d.Auth.RegisterEmail)
				r.Post("/login-email",    d.Auth.LoginEmail)
				r.Post("/refresh",        d.Auth.Refresh)
				r.Post("/verify-email",   d.Auth.VerifyEmail)
				r.Post("/request-password-reset", d.Auth.RequestPasswordReset)
				r.Post("/reset-password", d.Auth.ResetPassword)
				// OAuth flows
				r.Route("/oauth/{provider}", func(r chi.Router) {
					r.Post("/start",    d.Auth.OAuthStart)
					r.Get ("/callback", d.Auth.OAuthCallback)
				})
			})
			r.Get("/games",      d.Users.ListGames)   // read-only, public
			r.Get("/games/{id}", d.Users.GetGame)
		})

		// ── auth required ─────────────────────────────────────────
		r.Group(func(r chi.Router) {
			r.Use(RequireAuth(d.Signer))

			r.Route("/users", func(r chi.Router) {
				r.Get ("/me",                  d.Users.GetMe)
				r.Patch("/me",                 d.Users.UpdateMe)
				r.Put  ("/me/avatar",          d.Users.UpdateAvatar)
				r.Get  ("/{uid}",              d.Users.GetByUID)
				r.Get  ("/by-handle/{handle}", d.Users.GetByHandle)
				r.Get  ("/search",             d.Users.Search)
			})

			r.Post("/auth/logout",     d.Auth.Logout)
			r.Post("/auth/logout-all", d.Auth.LogoutAll)
			r.Post("/auth/oauth/bind", d.Auth.OAuthBind)

			r.Route("/friends", func(r chi.Router) {
				// handlers stubbed in friends_handler.go
			})
			r.Route("/rooms", func(r chi.Router) {
				r.Get ("/",                   d.Rooms.List)
				r.Post("/",                   d.Rooms.Create)
				r.Get ("/{id}",               d.Rooms.Get)
				r.Patch("/{id}",              d.Rooms.Update)
				r.Post("/{id}/join",          d.Rooms.Join)
				r.Post("/{id}/leave",         d.Rooms.Leave)
				r.Post("/{id}/kick",          d.Rooms.Kick)
				r.Post("/{id}/invite",        d.Rooms.Invite)
				r.Post("/{id}/approve",       d.Rooms.ApproveJoin)
				r.Post("/{id}/start",         d.Rooms.Start)
				r.Post("/{id}/pause",         d.Rooms.Pause)
				r.Post("/{id}/resume",        d.Rooms.Resume)
				r.Post("/{id}/end",           d.Rooms.End)
				r.Post("/{id}/match-result",  d.Rooms.MatchResult)
				r.Post("/{id}/character",    d.Rooms.SetCharacter)
			})
			// dms, groups, channels, lobby, notifications, reports — same pattern
		})

		// ── WebSocket (auth via query token; handled inside Hub) ──
		r.Get("/ws", d.Hub.ServeHTTP)
	})

	return r
}
