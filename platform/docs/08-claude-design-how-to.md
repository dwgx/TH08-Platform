# How to Use Claude Design — Step by Step

> 你问过"Claude Design 怎么用"。这是最短可行教程。
> 用 10 分钟读完，配合 `05-prompts-claude-design.md` 里的 5 个 prompt，
> 你就能从零做出符合 TH-Platform 品牌的完整页面。

---

## 1. 什么是 Claude Design

**claude.ai/design** 是 Anthropic 内部给设计师/开发者用的 UI 生成器。

- 你给它 prompt（文字描述 + 可选截图参考）
- 它返回一段 **React + Tailwind CSS** 代码，可以直接粘到你项目里
- 可多轮迭代（对话式）：「把按钮改大一点」，它改
- **额度按生成计** — 每次生成约等于一次 API 调用，消耗账户额度
- 输出质量**高于**纯文字聊天里让 Claude 写 UI

---

## 2. 使用前先到位三件事

1. **设计宪法** `01-design-constitution.md` — 颜色/字体/间距/动效。prompt 里要引用这些值。
2. **参考截图（可选）** — Discord / Steam / Parsec / VRCSM 的截图，给 Claude 视觉锚点。
3. **页面草图（可选）** — 哪怕手画一张，也比纯文字描述好。

---

## 3. 一次成功生成的 5 个要素

好的 prompt 必有这五段（参考 `05-prompts-claude-design.md`）：

| 段 | 作用 | 缺失的后果 |
|---|---|---|
| **Context** | 告诉它这是什么产品、给谁用 | 出来很通用，没 TH 气质 |
| **Layout** | 整体结构、栏位、尺寸 | 布局不对，每次手改浪费额度 |
| **Style** | 色彩、字体、圆角、间距 | 风格不统一，无法跨页面复用 |
| **Components** | 用哪些组件，什么语义 | 输出用自己造的 div，不匹配 shadcn |
| **States** | hover / empty / loading / error | 只给你 happy path，冷启动页面显空 |

**最关键**：Output format 段（我在 prompt 里标了"Output format"）。告诉它返回什么格式，不然它可能返回 HTML、Vue、甚至 Figma 描述。

---

## 4. 第一次怎么做

### Step 1 — 打开 claude.ai/design

（需要已登录 Anthropic 账户）

### Step 2 — 选 Lobby 页（最大的先做）

复制 `05-prompts-claude-design.md` 里的：

- **SHARED CONTEXT 整段**
- 然后 **Prompt #1 — Lobby Page** 整段

粘贴，发送。

### Step 3 — 等 30-60 秒

它会生成一份 `.tsx` 文件。检查：

- 是否用了 `bg-canvas`, `bg-surface-raised` 等我们定义的类名
- 是否 3 列布局
- 每张房间卡片是否 96px 高
- 头像状态点颜色是否对
- 是否有 hover 态

### Step 4 — 决定是否"要这一版"

- ✅ **要** → 复制代码，保存到 `src/pages/Lobby.tsx`
- ⚠️ **要但想小改** → 在对话里说："把房间卡片的封面从 64px 改成 48px"，一句话改一处
- ❌ **不要** → **不要重发整个 prompt**。先想清楚它哪里不对，针对性要求："重新做中间列的房间卡片，要比现在更紧凑，卡片间距改成 8px"

### Step 5 — 额度用尽预算规划（我建议）

```
Prompt #1  Lobby             1-2 次（定设计语言，值得多花 1 次）
Prompt #2  Room Detail       1 次
Prompt #3  Channels          1 次
Prompt #4  Profile+Settings  1 次
Prompt #5  Login+Register    1 次
─────────────────────────────────────
合计                          5-6 次
```

其他页面（好友列表、私信、创建房间弹窗）**自己动手**，参考上述 5 个主页面的代码风格模仿。

---

## 5. 提示词高级技巧

### 用 prompt 引用已经生成的组件

第二次 prompt 你可以写：

```
Reuse the "ROOM CARD" spec from the Lobby page we designed earlier
(96px tall, 8px padding, rounded-lg, cover on the left). Apply the
same style to this page's "MATCH HISTORY" cards, but …
```

这让跨页一致性直接写进 prompt。

### 附截图

```
Here is a screenshot of Discord's server list [drag image into prompt].
Take the **spacing rhythm** but NOT the colors — use our palette.
```

"take X but not Y" 是明确取舍的关键句式。

### 告诉它什么**不要**做

反列表往往比正列表管用：

```
Do NOT:
- Add gradients on cards
- Use emoji as functional icons
- Make the avatar > 48px on cards (reserved for profile page only)
- Add animations longer than 200ms
```

### 定位"风格类比"

```
The aesthetic reference is:
- Linear (line weight, typography)
- Raycast (command palette vibe)
- Discord Nitro (button shapes, NOT colors)

Explicitly avoid: Bootstrap, Material Design, iOS Settings app.
```

---

## 6. 迭代循环

```
┌─────────────────────────────────────┐
│ 1. 开新对话 / 附 SHARED CONTEXT       │
│ 2. 粘页面 prompt                     │
│ 3. 生成 → 视觉检查 5 项 ↓             │
│     - 颜色用的是我们的 token         │
│     - 布局对                          │
│     - 间距在 4px 网格上                │
│     - 空/loading 态有                 │
│     - hover/focus 清晰                │
│ 4a. 全过 → 保存到 pages/*.tsx        │
│ 4b. 部分过 → 单点修改（"只改 X"）     │
│ 4c. 全不过 → 关对话，想清楚后重来     │
└─────────────────────────────────────┘
```

---

## 7. 额度耗完了怎么办

- 用 Claude Sonnet 在普通对话里让它生成（质量略降）
- 用 **v0 by Vercel**、**Magic Patterns**、**Uizard** 作为备胎
- 最差路线：自己看设计宪法写 Tailwind —— 这也是我们写这份宪法的目的

---

## 8. 常见翻车

| 症状 | 因 | 解 |
|---|---|---|
| 颜色全是 `bg-gray-800` 而不是 `bg-canvas` | prompt 没强调用项目 token | SHARED CONTEXT 必须每次带 |
| 组件用 `<button>` 不是 shadcn Button | 没说明 | prompt "Assume @/components/ui/button exists" |
| 给的动效 500ms+ 太慢 | 默认偏好 | prompt "No animations > 200ms" |
| 无 focus ring | 没要求 | "Include :focus-visible states" |
| 硬编码 English | 没说 | "Do not worry about translations; use Chinese literals as shown in the prompt" |
| 组件太"AI 味" | 风格模糊 | 给 3 个明确**排除**的参考品 |

---

## 9. 把输出融入项目的清单

收到 Claude Design 代码后：

- [ ] 文件改成 `.tsx`，放到 `src/pages/` 或 `src/components/`
- [ ] import 补齐（shadcn 组件、lucide icons）
- [ ] 硬编码字符串 → `t(".....")`
- [ ] Mock 数据 → TanStack Query (`useQuery`) 拿真实数据
- [ ] 事件（onClick / onChange）接到 API (`api.post(...)`) 和 WebSocket (`ws.subscribe(...)`)
- [ ] 类型：用 `types/` 里的 `UserProfile`, `Room` 等取代 `any`
- [ ] 移除 Claude 留下的 `TODO-data` 注释（或替换成真实实现）

---

## 10. 给你的第一次保底

如果你**第一次**用 Claude Design，先按这个顺序做：

1. 只发 **Prompt #1 (Lobby)**
2. 生成后，**不做任何修改**，直接保存到 `src/pages/Lobby.tsx`
3. `pnpm tauri:dev` 启动，用浏览器或窗口看实际效果
4. **然后**决定哪些地方要改，列出清单
5. 再对话一次批量改（比如一次说"改 3 处：A/B/C"）
6. 达标后，进入 Prompt #2

**这样你用 2 次额度做完了 Lobby。** 剩下 4 个页面各一次，5 次搞定主要 UI。

---

*Last updated: 2026-04-25.*
