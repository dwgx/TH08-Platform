// Tiny migration runner — reads migrations/*.up.sql in lexical order
// and applies those not yet recorded in schema_migrations. Idempotent.
//
// Usage:
//   go run ./cmd/migrate            # apply pending
//   go run ./cmd/migrate --down 1   # revert last N (careful!)
package main

import (
	"context"
	"flag"
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"sort"
	"strings"

	"github.com/jackc/pgx/v5/pgxpool"
)

func main() {
	down := flag.Int("down", 0, "revert last N migrations")
	dir := flag.String("dir", "./migrations", "migrations directory")
	flag.Parse()

	log := slog.New(slog.NewTextHandler(os.Stdout, nil))
	dsn := os.Getenv("TH_PG_DSN")
	if dsn == "" { fail(log, "TH_PG_DSN not set") }

	ctx := context.Background()
	pool, err := pgxpool.New(ctx, dsn)
	if err != nil { fail(log, "pg: "+err.Error()) }
	defer pool.Close()

	if _, err := pool.Exec(ctx, `
		CREATE TABLE IF NOT EXISTS schema_migrations (
		  version    TEXT PRIMARY KEY,
		  applied_at timestamptz NOT NULL DEFAULT now()
		)`); err != nil {
		fail(log, "bootstrap: "+err.Error())
	}

	applied := map[string]bool{}
	rows, err := pool.Query(ctx, `SELECT version FROM schema_migrations`)
	if err != nil { fail(log, "read: "+err.Error()) }
	for rows.Next() {
		var v string
		if err := rows.Scan(&v); err != nil { fail(log, err.Error()) }
		applied[v] = true
	}
	rows.Close()

	entries, err := os.ReadDir(*dir)
	if err != nil { fail(log, "readdir: "+err.Error()) }
	var ups, downs []string
	for _, e := range entries {
		if e.IsDir() { continue }
		name := e.Name()
		if strings.HasSuffix(name, ".up.sql") { ups = append(ups, name) }
		if strings.HasSuffix(name, ".down.sql") { downs = append(downs, name) }
	}
	sort.Strings(ups)
	sort.Strings(downs)

	if *down > 0 {
		sort.Sort(sort.Reverse(sort.StringSlice(downs)))
		count := 0
		for _, f := range downs {
			v := strings.TrimSuffix(f, ".down.sql")
			if !applied[v] { continue }
			log.Info("reverting", "version", v)
			if err := execFile(ctx, pool, filepath.Join(*dir, f)); err != nil {
				fail(log, "revert "+v+": "+err.Error())
			}
			count++
			if count >= *down { break }
		}
		log.Info("done", "reverted", count)
		return
	}

	count := 0
	for _, f := range ups {
		v := strings.TrimSuffix(f, ".up.sql")
		if applied[v] { continue }
		log.Info("applying", "version", v)
		if err := execFile(ctx, pool, filepath.Join(*dir, f)); err != nil {
			fail(log, "apply "+v+": "+err.Error())
		}
		count++
	}
	log.Info("done", "applied", count)
}

func execFile(ctx context.Context, p *pgxpool.Pool, path string) error {
	b, err := os.ReadFile(path)
	if err != nil { return err }
	_, err = p.Exec(ctx, string(b))
	return err
}

func fail(log *slog.Logger, msg string) {
	log.Error(msg)
	fmt.Fprintln(os.Stderr, msg)
	os.Exit(1)
}
