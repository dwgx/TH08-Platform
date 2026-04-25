# TH-Platform — Matchmaking & Social Platform

> The desktop launcher + social layer for TH-Platform multi-game Touhou
> multiplayer. Lives alongside `../dll/` (the in-game injected netcode)
> and `../game/` (the th08 decomp knowledge base).

## What is this

A Touhou-first alternative to GameRanger / Parsec with Discord-style
social features. Players launch this, sign in, browse / create rooms,
chat, and hit "Join" — the desktop client injects the multiplayer DLL
into the user-supplied game `.exe` and handles peer relay.

Target scale: 5000+ MAU, Windows-only, multilingual (zh-CN default,
en + ja from day 1).

## Layout

```
platform/
├── client/           Tauri 2 + React 19 desktop client
│   ├── src/          React app (components, pages, i18n, lib, styles)
│   └── src-tauri/    Rust core (window lifecycle, DLL injection)
├── server/           Go 1.22 gateway (REST + WebSocket)
│   ├── cmd/
│   │   ├── gateway/    main HTTP binary
│   │   └── migrate/    one-shot migration runner
│   ├── internal/     business code (auth, users, rooms, ws, …)
│   ├── migrations/   *.up.sql + *.down.sql
│   └── docker-compose.dev.yml
├── relay/            Go UDP relay (STUN/TURN + fallback)
├── shared/           TypeScript types generated from Go models (V2)
├── prompts/          Claude Design prompt drafts (also in docs/)
└── docs/
    ├── 01-design-constitution.md   ← READ FIRST
    ├── 02-architecture.md
    ├── 03-data-model.md
    ├── 04-api-spec.md
    ├── 05-prompts-claude-design.md
    ├── 06-i18n-framework.md
    └── 07-dev-setup.md
```

## Read in this order

1. `docs/01-design-constitution.md` — visual language & brand rules
2. `docs/02-architecture.md` — system topology
3. `docs/07-dev-setup.md` — get it running locally
4. `docs/04-api-spec.md` when you're ready to wire front-end to back-end
5. `docs/05-prompts-claude-design.md` when you go to claude.ai/design

## Status (2026-04-25)

| Area | State |
|---|---|
| Design constitution | ✅ written |
| Data model + migrations | ✅ written |
| API spec (REST + WS) | ✅ written |
| Go server scaffold | ✅ compilable skeleton, handlers stubbed |
| Tauri client scaffold | ✅ runs; page bodies are placeholders |
| DLL-injection loader | ✅ Rust impl in `client/src-tauri/src/loader.rs` (Windows only) |
| Claude Design prompts | ✅ 5 prompts drafted |
| Real page UIs | ⏳ pending Claude Design run |
| Auth flows (email login) | ⏳ handler sketched, needs session persistence |
| OAuth providers | ⏳ stubs only |
| Relay service | ⏳ not started |
| Matchmaking | ⏭ out of scope for V1 |

## License

AGPL-3.0-or-later — inherited from the th08 decomp upstream.
The platform itself contains no game binaries or copyrighted game code.
