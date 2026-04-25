// Package users owns the user entity and the UID allocator. The
// allocator wraps the SQL function allocate_uid() — see the migration
// 0001_init.up.sql for the vanity-pool semantics.
package users

import (
	"context"

	"github.com/dwgx/th-platform/server/internal/db"
)

type UIDAllocator struct{ db *db.DB }

func NewUIDAllocator(d *db.DB) *UIDAllocator { return &UIDAllocator{db: d} }

// Allocate returns a fresh UID. Internally this:
//   1. With 5% probability pulls a vanity UID from uid_vanity_pool
//      (locking the row with SKIP LOCKED to avoid contention).
//   2. Otherwise advances uid_seq, skipping entries in uid_reserved.
// Call this inside the same transaction where the new users row is
// inserted — the vanity_pool row stays "reserved" even if the caller
// rolls back, which is acceptable (small leak; better than double-use).
func (a *UIDAllocator) Allocate(ctx context.Context) (int64, error) {
	var uid int64
	err := a.db.Pool.QueryRow(ctx, `SELECT allocate_uid()`).Scan(&uid)
	return uid, err
}
