# TH-Platform — Client

Tauri 2 + React 19 + TypeScript + Tailwind 4 + shadcn/ui.

## Quick start

```bash
# 1. install deps
pnpm install

# 2. start the backend first (../server — see its README)

# 3. run dev (spawns Vite + Tauri window)
pnpm tauri:dev
```

The window opens at 1280×800, frameless, dark.

## Build release

```bash
pnpm tauri:build
# outputs:
#   src-tauri/target/release/bundle/nsis/TH-Platform_0.1.0_x64-setup.exe
#   src-tauri/target/release/bundle/msi/TH-Platform_0.1.0_x64_en-US.msi
```

## Project layout

```
src/
├── main.tsx            entry — boot i18n, hydrate auth, mount
├── App.tsx             router + WS lifecycle
├── components/
│   ├── AppShell.tsx    3-panel resizable root layout
│   ├── NavRail.tsx     72px icon rail
│   ├── TitleBar.tsx    draggable title bar
│   ├── StatusBar.tsx   connection + version status
│   └── ui/             shadcn components land here after `pnpm dlx shadcn add ...`
├── pages/              one file per route — PLACEHOLDERS until Claude Design
├── i18n/
│   ├── index.ts        i18next init
│   └── locales/
│       ├── zh-CN/common.json   default
│       ├── en/common.json
│       └── ja/common.json
├── lib/
│   ├── api.ts          fetch wrapper + refresh flow
│   ├── auth-store.ts   zustand + @tauri-apps/plugin-store
│   ├── queryClient.ts  TanStack Query defaults
│   ├── utils.ts        cn() + UID formatter
│   └── ws.ts           WebSocket client w/ auto-reconnect
└── styles/
    └── globals.css     design tokens (design-constitution §2)

src-tauri/
├── Cargo.toml
├── tauri.conf.json
└── src/
    ├── main.rs         entrypoint
    ├── lib.rs          tauri::Builder + invoke_handler commands
    └── loader.rs       DLL injection (Windows CreateRemoteThread)
```

## Adding shadcn components

```bash
# after `pnpm install`
pnpm dlx shadcn@latest init     # first time (components.json already committed)
pnpm dlx shadcn@latest add button input card dialog dropdown-menu tabs avatar scroll-area
```

## Environment

```bash
# .env.local (not committed)
VITE_API_BASE_URL=http://localhost:8080
```

## Tauri commands exposed to React

| Command       | Args                                 | Returns           |
|---------------|--------------------------------------|-------------------|
| `app_info`    | –                                    | `{name,version,os,arch}` |
| `verify_exe`  | `path, expected_sha256`              | `bool`            |
| `launch_game` | `game_id, exe_path, dll_path`        | `pid: u32`        |

Call from React:

```ts
import { invoke } from "@tauri-apps/api/core";

const pid = await invoke<number>("launch_game", {
  gameId: "th08-1.00d",
  exePath: "C:\\Touhou\\th08.exe",
  dllPath: "C:\\Program Files\\TH-Platform\\th08_platform.dll",
});
```

## Replacing placeholder pages with Claude Design output

1. Pick a page (e.g., `src/pages/Lobby.tsx`)
2. Open `../docs/05-prompts-claude-design.md` — find the numbered prompt
3. Paste prompt at claude.ai/design, attach screenshots if available
4. Iterate until happy; copy the final component
5. Paste into the page file, wire data via TanStack Query + `api.get(...)`
6. Replace hard-coded strings with `t("lobby.*")` using `useTranslation()`
