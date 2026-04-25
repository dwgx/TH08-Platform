// Entrypoint for the TH-Platform gateway — serves REST API + WebSocket
// hub on a single HTTP port.
//
// Run:
//   TH_PG_DSN=postgres://... ./gateway
package main

import (
	"context"
	"errors"
	"log/slog"
	"net/http"
	"os"
	"os/signal"
	"syscall"
	"time"

	"github.com/dwgx/th-platform/server/internal/auth"
	"github.com/dwgx/th-platform/server/internal/config"
	"github.com/dwgx/th-platform/server/internal/db"
	"github.com/dwgx/th-platform/server/internal/httpapi"
	"github.com/dwgx/th-platform/server/internal/users"
	"github.com/dwgx/th-platform/server/internal/ws"
)

func main() {
	log := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelInfo}))

	cfg, err := config.Load()
	if err != nil { log.Error("config load", "err", err); os.Exit(1) }

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	dbh, err := db.Open(ctx, cfg.PGDSN, log)
	if err != nil { log.Error("db open", "err", err); os.Exit(1) }
	defer dbh.Close()

	signer, err := auth.NewSigner(
		cfg.JWTPrivateKeyPath, cfg.JWTPublicKeyPath,
		"th-platform", cfg.JWTAccessTTL, cfg.JWTRefreshTTL,
	)
	if err != nil { log.Error("jwt signer", "err", err); os.Exit(1) }

	userSvc := users.NewService(dbh)
	uidAlloc := users.NewUIDAllocator(dbh)
	hub := ws.NewHub(log, signer)

	deps := &httpapi.Deps{
		Cfg:    cfg,
		Signer: signer,
		Hub:    hub,
		Auth:   &httpapi.AuthService{DB: dbh, Signer: signer, Users: userSvc, UID: uidAlloc},
		Users:  &httpapi.UsersService{DB: dbh, Users: userSvc},
		Rooms:  &httpapi.RoomsService{DB: dbh},
	}

	srv := &http.Server{
		Addr:              cfg.BindAddr,
		Handler:           httpapi.NewRouter(deps),
		ReadHeaderTimeout: 5 * time.Second,
	}

	go func() {
		log.Info("gateway listening", "addr", cfg.BindAddr, "env", cfg.Env)
		if err := srv.ListenAndServe(); err != nil && !errors.Is(err, http.ErrServerClosed) {
			log.Error("http serve", "err", err)
			cancel()
		}
	}()

	// Signals → graceful shutdown
	sig := make(chan os.Signal, 1)
	signal.Notify(sig, os.Interrupt, syscall.SIGTERM)
	select {
	case <-sig:
		log.Info("shutdown signal received")
	case <-ctx.Done():
	}

	shutdownCtx, shutdownCancel := context.WithTimeout(context.Background(), 30*time.Second)
	defer shutdownCancel()
	_ = srv.Shutdown(shutdownCtx)
	log.Info("gateway stopped")
}
