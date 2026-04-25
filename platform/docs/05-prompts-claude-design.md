# Claude Design Prompts — TH-Platform

> 5 个按顺序使用的 prompt。**从 #1 开始**，它定义整体设计语言；
> 后续 prompt 复用 #1 的风格承诺 + 增量说明，避免每次重新描述。

---

## 使用守则

1. **先读 `01-design-constitution.md`** — 不读的话 prompt 里的色值和字体你对不上。
2. Claude Design 额度宝贵：**一次 prompt 就让它产出合格组件**。差一点的用文字微调，不要重发整个 prompt。
3. 所有 prompt 结尾的"Output format"段**不要删**。这是让它回 React+Tailwind 而不是别的东西的关键。
4. 发 prompt 前，把下面的 **SHARED CONTEXT** 复制到每个 prompt 开头。

---

## 🔷 SHARED CONTEXT（每次都带，不要省）

```
You are designing a page for TH-Platform, a desktop Windows client
(Tauri 2 + React 19 + TypeScript). It is a social + matchmaking
platform for Touhou shoot-em-up games — think "Discord meets Parsec"
but with its own identity.

Tech constraints:
- Tailwind CSS 4 utility classes only. No styled-components, no CSS modules.
- shadcn/ui primitives allowed (Button, Input, Card, Dialog, Avatar, ScrollArea, Separator, DropdownMenu, Tooltip). Assume @/components/ui/<primitive> imports exist.
- lucide-react for icons at sizes 14, 16, 20 px only, stroke 1.75.
- No runtime animations heavier than 300ms. Prefer CSS transitions.
- Window is minimum 1024×640 px, resizable, dark-only for V1.
- No hero videos, no particle effects, no parallax.
- Text respects i18next — use literal strings for now, we'll wire t() later.

Visual language:
- Dark theme, HSL tokens already defined. Use classes like:
    bg-canvas, bg-surface, bg-surface-raised, bg-surface-bright
    text-fg, text-fg-muted, text-fg-subtle
    border-border, border-border-strong
    bg-brand / text-brand (indigo-violet #8b5cf6)
    bg-brand-accent (cherry pink #ec5c8f — use SPARINGLY)
    bg-success / bg-warning / bg-danger
- Corner radius: rounded-md (6) for buttons/inputs, rounded-lg (10) for cards, rounded-xl (14) for dialogs, rounded-full for avatars.
- Spacing: 4px grid. Use Tailwind's 1,2,3,4,5,6,8,10,12,16.
- Type: Inter / Noto Sans SC / Noto Sans JP via --font-sans already loaded. Sizes 11 / 12 / 13 / 14 / 16 / 20 / 28 / 40 px.
- Motion: use transition-all duration-[120ms] ease-[cubic-bezier(.2,0,0,1)].

Voice:
- Direct, calm, no kaomoji, no exclamation marks in UI chrome. Error messages state cause + fix.

Output format:
- Return ONE self-contained React .tsx file, default export named as the page.
- All className values inline in JSX.
- Use realistic placeholder data inline (no fetch calls).
- Mark TODO-data comments where real data will plug in.
- No router imports, no context providers — pure layout.
- Include hover/focus/active/disabled states where relevant.
- Include ONE visible "empty state" if applicable.
```

---

# Prompt #1 — Lobby Page (START HERE, defines the rest)

```
[PASTE SHARED CONTEXT]

Design the Lobby page (the main hub after login).

Layout:
- This page renders inside an AppShell that already provides:
    * A 72px-wide NavRail on the left (icons only)
    * A 36px-tall TitleBar at top
    * A 24px-tall StatusBar at bottom
  Your page renders inside the remaining rectangle.
- Your page itself is a 3-column split:
    LEFT  (fixed 260px): "Server categories" list
      - Section headers: "官方服务器 Official" / "玩家房间 Player rooms" / "社区服务器 Community"
      - Each section: 3-5 collapsible game-version groups (e.g., "东方永夜抄 TH08", "东方妖々梦 TH07"), each showing room count.
      - Top of this column has a prominent "Create Room" button and a small "Join by code" input.
    CENTER (flexes): room list
      - Top of this column: page title "大厅 Lobby" (28px bold), filter chips (game-version filter, region filter, "Only friends inside", "Show full"), and sort dropdown.
      - Below: grid/list of ROOM CARDS (see spec below). 3 columns by default at 1280px; 2 columns at 1024-1279; 1 column below 1024 (but honor 1024 minimum).
    RIGHT (fixed 340px): Public chat
      - Title "大厅聊天 Lobby chat" with an online-count badge.
      - Scrollable message stream. 40 messages visible. Each message:
          12px avatar row | username + UID (mono) + time | body
      - Footer: an input box with a small emoji picker button. Enter to send.

ROOM CARD spec:
- Width 100%, height 96px.
- 8px padding, rounded-lg.
- Left: a 64×64 rounded-md "cover" panel showing the game family logo monogram (just a big letter + small stage icon).
- Right: stacked text rows:
    Row 1 (14px/600): room name, with two subtle badges: game version ("TH08"), region ("CN-SH")
    Row 2 (12px/muted): host avatar (16px) + host handle + " • " + "2/4 玩家 · 0 观战"
    Row 3 (11px): small pills — "公开"/"私人", join-mode (绿/黄/红 dot + "自由"/"审批"/"锁定"), ping (mono "24ms")
- Hover: card rises 1px, border brightens, right edge reveals a "加入 Join" button that fades in.
- Selected (if any): left edge 3px brand-color bar.

Empty state:
- Center column shows a small geometric yin-yang icon + "还没有可加入的房间" + a primary button "创建房间".

Include in your output:
- Mock data for 5 example rooms (mix of full/empty/private).
- 6 example chat messages with different users.
- Plausible Chinese/Japanese usernames; one English one.
- All 3 empty states: empty room list, empty chat, empty server category.

Make it feel: confident, premium, faintly nocturnal. Like a good bar, not a nightclub.
```

---

# Prompt #2 — Room Detail Page

```
[PASTE SHARED CONTEXT]

Design the Room Detail page. This page is what players see AFTER
joining a room, while waiting to start or between matches.

Layout (within AppShell main area):
- Top hero strip (height 120px):
    * Left: room name (28px/700) + short room code in monospace badge ("TH8-2F9A") with a copy button
    * Middle: game version chip + "2/4 玩家" + "0 观战" + region chip
    * Right: primary action changes by role:
         - host, state=lobby:     big "开始游戏" (filled brand)
         - host, state=playing:   "暂停" + "结束"
         - non-host, state=lobby: ghost "准备就绪" toggle
         - spectator:             ghost "加入玩家席 (if slot)" and "离开"
- Below hero, 3-column split:
    LEFT (fixed 260px): Members list
      - Section "玩家席" with slots (P1, P2) as big cards (110px tall): avatar (56px) + username + UID + character pick chip + ping
        * Empty slots show a dashed border and a small "邀请" button
      - Section "观战席" with smaller rows
    CENTER (flexes): Character select / room config
      - Tabbed interface: "角色" (default) / "规则" / "聊天"
      - "角色" tab: 2x2 grid of character cards for TH08 (灵梦&紫, 魔理沙&爱丽丝, 咲夜&蕾米, 妖梦&幽幽子). Each card shows a stylized symbol (not real art), character name in CN/JP/EN, and small "我选这个" ghost button. Currently selected card has brand border + check icon.
      - "规则" tab: list of adjustable parameters as labeled rows with shadcn switches / numeric inputs / select dropdowns. Examples: Difficulty (Easy/Normal/Hard/Lunatic), Lives (integer 1-9), Spellcard practice (switch), Deterministic seed (mono input).
      - "聊天" tab: mini chat specifically for this room.
    RIGHT (fixed 300px): Invite + Network
      - Card 1: Invite
        * Copy room code button
        * "邀请好友" button opens a Command-Palette-like search over friends
        * List of recent invites sent (pending/accepted/declined)
      - Card 2: Network
        * Relay region selected + latency dot
        * P2P vs Relay indicator
        * "切换中继" ghost button

Interaction details to convey:
- Host-only controls visually distinguished (small crown icon next to host's name; host action buttons slightly brighter).
- When state=playing, the center area greys out slightly and shows "游戏进行中…" with an oval brand-color "回到游戏" button that brings the game window forward.
- When state=paused, a yellow banner at top.

Mock data: 2 players (one of them you, with the crown), 1 spectator, 1 pending invite, ping = 24ms.
```

---

# Prompt #3 — Channels Page (group channels + chat)

```
[PASTE SHARED CONTEXT]

Design the Channels page — a Discord-like group + channel + message
three-column layout, but **simpler** and more spatial.

Layout (within AppShell main area):
- LEFT column (260px fixed): Group list
    * Top: search bar + "Create group" button (ghost with + icon)
    * List of groups I'm in. Each row shows group icon (28px rounded-md), group name (14px/600), unread badge (brand-accent pink pill if > 0), last-activity time (11px muted).
    * Active group: left 3px brand bar + surface-raised bg.
- MIDDLE column (220px fixed): Channels within selected group
    * Header: group name + dropdown arrow revealing "group settings / leave group / mute" options
    * Channel categories (collapsible):
        #text channels (with # icon)
        📌 pinned channels (with pin icon, smaller)
    * Each channel: hover reveals "settings gear" on the right.
    * Active channel: brand bar on the left + surface-raised.
    * Under the list, a small presence panel: "在线 12 · 总人数 28"
- RIGHT column (flexes): Messages
    * Header strip: channel name + "#" prefix, topic in muted text, right-side icons: notification-bell, pin-list, members-list toggle.
    * Scrollable message list:
        - Grouped by sender when successive messages from the same user within 5 minutes.
        - Each group starts with avatar (32px) + username (14px/600) + UID (11px mono muted) + time (11px muted). Subsequent messages show only the body offset left.
        - System messages ("xxx 加入了频道") in small italic muted text, centered.
        - Message body: plain text, emojis inline (slightly larger), at-mention highlighted with brand color + subtle background pill.
        - On hover: right-edge action strip appears (react, reply, more...).
    * "Someone is typing" indicator above the composer (thin 16px row).
    * Composer at bottom:
        - Multi-line auto-growing textarea (max 160px tall)
        - Left: emoji-picker button + "share room" button (Gamepad2 icon)
        - Right: send button (primary, disabled when empty)
        - Below composer: hint "按 Enter 发送 · Shift+Enter 换行"

Right edge (optional, toggled via header icon):
- A 220px members panel. Sections: "ONLINE 7 / OFFLINE 21". Each member row: avatar + status dot + name + role tag if admin.

Mock data:
- 2 groups ("TH08 天朝联机群 — 14 在线", "东方同人试玩圈 — 3 在线")
- 4 channels in active group
- 10 chat messages including 1 system message and 1 with an image-placeholder (rounded-lg zinc block labeled "image preview")
```

---

# Prompt #4 — Profile + Settings Pages

```
[PASTE SHARED CONTEXT]

Produce TWO pages in one file (export both):
1. UserProfile — viewing another user
2. Settings — managing my own account

── 1. UserProfile ──
Layout inside the main content area:
- Top banner (height 180px) with a muted gradient background (hsl(258 40% 20%) to hsl(232 30% 12%)). On top:
    * 96px circular avatar centered-left with status dot
    * Right of avatar: username (28px/700), @handle (16px/500 muted, monospace), UID chip (monospace surface-bright pill)
    * Primary actions row: "添加好友" (or "发消息" if already friends), "邀请加入房间", overflow-menu (block, report)
- Below banner, 3-column content:
    LEFT (260px):
      * Stats card: matches played, win rate, favorite game, signup date
      * Connected accounts card: GitHub / Discord / QQ with bound-state icons
    CENTER (flexes):
      * Bio card (if bio present)
      * Tags (multi-colored small pills)
      * Recent matches (list of 5, each row: game icon + game name + result win/lose pill + duration + time-ago)
    RIGHT (300px):
      * Mutual friends (if any) — avatar cluster + "5 位共同好友"
      * Current activity (if online) — "正在东方永夜抄 · 房间 TH8-2F9A" with a "加入" button

Empty state: if visiting a suspended/deleted user, show a big "此用户已注销" card with a muted ghost avatar.

── 2. Settings ──
Layout:
- LEFT sidebar (220px): sections list
    账号 / 个人资料 / 外观 / 通知 / 语言 / 隐私 / 游戏与启动器 / 快捷键 / 关于
- RIGHT flex: settings content. Show the "账号" section populated:
    Section title (20px/600), subtle description (12px muted).
    Subsections as th-card groups separated by 24px:
      - 账号基础: email field (with "verified" check), handle field (with "check availability"), avatar picker button, password-change link.
      - 第三方绑定: list rows with Github/Discord/QQ — each row shows bound status + "解绑"/"绑定" ghost button.
      - 危险区域: a red-bordered card with "退出所有设备", "注销账号" (both require double confirmation in modal).

Make primary buttons consistent with the brand. Make danger buttons red border + red text on the destructive ghost pattern, filled red only when user confirms intent (two-step).

Mock data: the viewing user is "夜雀ミスティア" (@nightsparrow, UID 10328), connected accounts: GitHub+Discord bound, QQ not bound. Recent matches: 3 wins 2 losses in TH08.
```

---

# Prompt #5 — Login + Register Pages

```
[PASTE SHARED CONTEXT]

These two pages render OUTSIDE the AppShell (no NavRail, no TitleBar).
They must fill the entire window (flex centered content, bg-canvas).

Produce a single .tsx file that exports both LoginPage and RegisterPage.

── Shared look ──
- Centered card (460px wide for both, padding 40px, rounded-xl,
  surface-raised, 1px border, shadow-md).
- Above the card, app logo lockup: 32px circular brand mark (a stylized moon+shrine bell geometric shape) + "TH-Platform" wordmark + small tagline (东方联机平台).
- Below the card: footer with links "关于 · 帮助 · 隐私协议 · 服务条款" (11px muted).

── LoginPage ──
Inside the card:
1. Title "欢迎回来 Welcome back" (28px/700)
2. Subtitle (12px muted): "用邮箱或 @ 用户名登录 · Sign in with email or @handle"
3. Input: email or handle (Input primitive, full-width, height 40px)
4. Input: password (with show/hide eye toggle inside)
5. Link on right side: "忘记密码？"
6. Primary button (full-width): "登录" (brand, height 40px, 13px/500)
7. Horizontal separator with "或使用以下方式 Or continue with"
8. Three OAuth buttons in a row (equal width): GitHub / Discord / QQ
   - Each button: 40px tall, surface-bright bg, provider icon on left, provider name on right, white text.
   - Subtle 1px border.
9. Bottom line: "没有账号？ Create one →" links to /register

Loading state: primary button shows small spinner; inputs disabled.
Error state: a thin destructive-bordered banner between subtitle and inputs, 11px text, icon AlertCircle.

── RegisterPage ──
Same card layout. Inside:
1. Title "创建账号 Create account"
2. Subtitle explaining "邮箱注册 + 绑定第三方账号 Sign up with email, optionally bind social accounts"
3. Input: email
4. Input: password (with strength meter below — 4 segments colored empty/red/orange/green)
5. Input: username (display name)
6. Input: @handle — with live uniqueness check indicator on the right (checkmark or cross, subtle)
7. Small "我已阅读并同意《服务条款》和《隐私政策》" checkbox (required to enable submit)
8. Primary button "创建账号"
9. Separator + 3 OAuth buttons like login
10. Footer: "已有账号？ Sign in →"

Both pages should feel calm and quick. No heavy imagery — at most a very faint geometric pattern in the card's top-right corner (diagonal subtle lines at 4% opacity). Absolutely no anime art.

Responsive: at 1024×640 the card is still centered comfortably.
```

---

## 发送后的回收流程

1. Claude Design 返回组件 → 粘到对应 `src/pages/*.tsx`
2. 顶部 import 调整（`import { Button } from "@/components/ui/button"` 等 — 先 `pnpm dlx shadcn@latest add button card dialog avatar input ...` 把需要的 primitive 落盘）
3. 把硬编码文案替换成 `t("...")`
4. 接 API：用 `useQuery` + `api.get(...)` 接入
5. WebSocket 订阅：`useEffect(() => { ws.subscribe("lobby"); return () => ws.unsubscribe("lobby") }, [])`

---

*Last updated: 2026-04-25.*
