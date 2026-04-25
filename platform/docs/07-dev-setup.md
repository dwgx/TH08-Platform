# Dev Setup — TH-Platform

> How to run the whole stack locally on Windows. ~30 minutes first time.

---

## Prerequisites

| Tool | Version | Install |
|---|---|---|
| Node.js + pnpm | Node 20+, pnpm 9+ | https://pnpm.io/installation |
| Rust | stable (2021+) | `rustup default stable` |
| Go | 1.22+ | https://go.dev/dl/ |
| Docker Desktop | latest | https://www.docker.com/products/docker-desktop/ |
| WebView2 Runtime | latest (pre-installed on Win11) | https://developer.microsoft.com/webview2 |
| MSVC Build Tools | 2022+ | Visual Studio Installer → "Desktop development with C++" |
| OpenSSL | any | via Git Bash (it ships ssl) or `choco install openssl` |

Once-off Tauri prereqs check:

```bash
cargo install create-tauri-app
pnpm dlx @tauri-apps/cli --version
```

---

## 1. Backend

```bash
cd TH08-Platform/platform/server

# Start Postgres + Redis + Mailpit locally
make db-up

# Generate JWT signing keys (once)
make keys

# Copy env
cp .env.example .env
# Edit .env if needed — default values work for local dev

# Apply migrations
export $(grep -v '^#' .env | xargs)          # bash
# or PowerShell:
#   Get-Content .env | Where-Object { $_ -notmatch '^#' } | ForEach-Object { $name, $value = $_ -split '=', 2; Set-Item -Path "env:$name" -Value $value }
make migrate

# Run the gateway
make run
# → "gateway listening" on :8080
```

Smoke test:

```bash
curl http://localhost:8080/healthz       # → ok
curl http://localhost:8080/api/v1/games  # (will 500 until games handler is wired)
```

---

## 2. Client

```bash
cd TH08-Platform/platform/client

pnpm install

# Set the backend URL for dev
echo "VITE_API_BASE_URL=http://localhost:8080" > .env.local

# Start Tauri dev window (opens a frameless 1280×800 window)
pnpm tauri:dev
```

The window boots to the Login placeholder. Register a user via the
`/api/v1/auth/register-email` endpoint (curl for now, until Claude
Design output replaces the placeholder forms):

```bash
curl -X POST http://localhost:8080/api/v1/auth/register-email \
  -H "Content-Type: application/json" \
  -d '{"email":"me@example.com","password":"password123","username":"dwgx","handle":"dwgx","locale":"zh-CN"}'
```

---

## 3. Add shadcn primitives as you need them

```bash
cd TH08-Platform/platform/client
pnpm dlx shadcn@latest add button input card dialog dropdown-menu \
  avatar scroll-area separator tabs tooltip switch
```

Files land in `src/components/ui/`.

---

## 4. Seed data for local dev (optional)

After migrations applied, hand-seed a few users + rooms so the lobby
isn't empty during design work. Quick script at
`server/scripts/seed-dev.sql` (create it as needed).

---

## 5. DLL integration (optional, when Phase 1 DLL lands)

```bash
# Build the DLL (see ../../dll)
cd TH08-Platform/dll
cmake -B build -A Win32
cmake --build build --config Release
# -> TH08-Platform/dll/build/bin/th08_platform.dll

# From the Tauri app (Rust side) point at that DLL
#   — invoke "launch_game" passes dll_path to loader.rs
```

---

## Common issues

| Symptom | Fix |
|---|---|
| `pnpm tauri:dev` hangs at "compiling tauri" first time | First build compiles ~700 Rust crates. 10-15 min is normal. |
| `CreateProcessW` returns `ERROR_ELEVATION_REQUIRED` | Your `th08.exe` requires admin; run the launcher as admin. |
| PG migration fails with `permission denied for schema public` | `docker compose` user is th/th — `psql -U th -d thplatform` to inspect. |
| WebView2 not found at runtime | Install Evergreen WebView2 Runtime from Microsoft. |
| Fonts fall back to system on first load | `@fontsource-variable/inter` needs `pnpm install` to download woff2. |
| JWT verify error "key is of invalid type" | Re-run `make keys` — the PEM must be PKCS#8/PKCS#1 RSA. |

---

*Last updated: 2026-04-25.*
