# TH-Platform — Server

Go 1.22 + chi + pgx + nhooyr/websocket + Redis.

## Quick start (local dev)

```bash
# 1. bring up Postgres + Redis + Mailpit
make db-up

# 2. generate RSA keys for JWT signing
make keys

# 3. copy and edit env
cp .env.example .env
export $(grep -v '^#' .env | xargs)

# 4. apply migrations
make migrate

# 5. run gateway
make run
```

Healthcheck: `curl http://localhost:8080/healthz` → `ok`.

## Layout

```
cmd/
├── gateway/     main HTTP+WS entrypoint (production binary)
└── migrate/     one-shot migration applier

internal/
├── auth/        JWT + argon2 password hashing
├── config/      env → typed Config struct
├── db/          pgxpool wrapper
├── httpapi/     chi routes + middleware + handlers (thin)
├── users/       user CRUD + UID allocator
├── rooms/       (TODO) room lifecycle + state machine
├── friends/     (TODO)
├── channels/    (TODO)
├── lobby/       (TODO)
├── presence/    (TODO)
├── ws/          WebSocket hub + topic fanout
└── relay/       (TODO) relay-ticket signer / issuer

migrations/      *.up.sql / *.down.sql
```

## Design principles

- **httpapi thin**: handlers parse → call service → render. No business code in handlers.
- **Services return typed errors**: handlers map to `APIError` via `WriteError`.
- **Ctx everywhere**: every function taking a DB op takes `context.Context`.
- **No ORM**: sqlc for generated readers (TODO), handwritten SQL for writes.
- **No globals**: wire deps through `httpapi.Deps`.

## TODO (priority order)

1. Rooms service + state machine + codegen
2. Friends + DMs
3. Groups + channels
4. Lobby public chat (store + fan out)
5. Notifications
6. OAuth providers (GitHub first, Discord, QQ)
7. Relay ticket signer
8. sqlc codegen wiring
9. Rate limit per user (Redis token bucket)
10. pg_cron cleanup jobs
