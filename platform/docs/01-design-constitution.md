# Design Constitution — TH-Platform Client

> 这份文档是所有设计决策的**唯一真相来源**。Claude Design prompt、组件开发、主题变量，全部从这里派生。
> 遇到分歧 → 回来看这份 → 如果要改 → 先改这份再改代码。

---

## 0. 品牌一句话

**"为东方系列玩家的现代联机客户端 — 像 Discord 一样社交，像 Steam 一样直观，像东方本身一样精致。"**

- 定位：桌面联机启动器 + 社交平台
- 目标用户：25-40 岁，东方 STG 玩家 / 同人圈子 / 日本 + 中国 + 英语圈
- 竞品对标：Discord（社交）+ Parsec（P2P 对战）+ 网易 UU（启动器形态）

---

## 1. 视觉语言三原则

### 原则 1 — 暗系优先但不压抑

Discord 的 `#36393f` 太灰闷。我们采用**深靛蓝**底 + **点点樱粉红**作为品牌色。
深色模式是默认，后期可做亮色主题但**不强制**（90% 用户会留在深色）。

### 原则 2 — 圆角但克制

全量 `border-radius: 8px` 太软萌。按照信息层级分级：

| 元素 | 圆角 |
|---|---|
| 按钮、输入框、标签 | `6px` |
| 卡片、面板 | `10px` |
| 弹窗、Modal | `14px` |
| 头像 | `50%` |

### 原则 3 — 动效要快、要短、要可预测

- 所有 transition 默认 **120ms cubic-bezier(0.2, 0, 0, 1)**
- 进场动画 **180ms**，退场动画 **140ms**
- 禁止：弹簧、跳跃、旋转入场、超过 300ms 的动效
- 例外：房间加入过渡可以 400ms（仪式感），但也就这一处

---

## 2. 色板（CSS Custom Properties, HSL）

```css
:root {
  /* ── 品牌色 ── */
  --brand-primary:     258 86% 66%;   /* #8b5cf6 — 深靛紫罗兰，代表科技感 */
  --brand-accent:      340 82% 68%;   /* #ec5c8f — 樱花粉，点缀用 */
  --brand-ring:        258 86% 66%;   /* focus ring，与 primary 同 */

  /* ── 表面（三级） ── */
  --canvas:            232 15% 9%;    /* #12141c — 窗口底色 */
  --surface:           232 14% 12%;   /* #181a24 — 主面板 */
  --surface-raised:    232 13% 15%;   /* #1f222d — 浮起面板 */
  --surface-bright:    232 12% 19%;   /* #282c3a — 弹窗/popover */
  --surface-hover:     232 14% 16%;   /* 鼠标悬停态 */

  /* ── 边框 ── */
  --border:            232 12% 18%;   /* 暗描边 */
  --border-strong:     232 10% 26%;   /* 分隔线 */

  /* ── 文字 ── */
  --fg:                220 15% 92%;   /* #e6e8ee */
  --fg-muted:          220 10% 65%;   /* #9ba1ae */
  --fg-subtle:         220 10% 48%;   /* 更弱，用于辅助信息 */

  /* ── 语义色 ── */
  --success:           142 72% 50%;   /* #22c55e */
  --warning:           38 92% 58%;    /* #f59e0b */
  --danger:            0 72% 58%;     /* #ef4444 */
  --info:              210 90% 60%;   /* #3b82f6 */

  /* ── 在线状态点 ── */
  --status-online:     142 72% 50%;
  --status-idle:       38 92% 58%;
  --status-dnd:        0 72% 58%;
  --status-offline:    232 10% 40%;
  --status-ingame:     258 86% 66%;   /* 品牌紫：正在游戏 */

  /* ── 东方主题点缀（可选，单页使用） ── */
  --touhou-reimu:      350 75% 58%;   /* 灵梦红白：红 */
  --touhou-marisa:     50 95% 62%;    /* 魔理沙黑白：黄 */
  --touhou-sakuya:     220 25% 55%;   /* 咲夜银蓝 */
  --touhou-youmu:      120 20% 62%;   /* 妖梦浅绿 */
}
```

### 为什么不直接抄 Discord

Discord 的 `#5865F2` 蓝紫太饱和、太"游戏感"。
我们用 `#8b5cf6` 靛紫：更稳、更现代、更配樱粉点缀。
樱粉 `#ec5c8f` 仅用在品牌强调场景（Logo、主 CTA 按钮、主题徽章），用量 < 10% 总像素。

---

## 3. 字体系统

```css
--font-sans: "Inter Variable", "Noto Sans SC", "Noto Sans JP",
             "Segoe UI Variable", system-ui, sans-serif;
--font-mono: "JetBrains Mono Variable", ui-monospace, Consolas, monospace;
--font-display: "Inter Variable", var(--font-sans);  /* 标题同 sans，靠 weight 区分 */
```

**为什么不用 Geist**：VRCSM 用 Geist 是 Vercel 美学，适合工具类产品。我们要兼顾中文/日文可读性，Noto Sans SC/JP 是最成熟选项。Inter Variable 覆盖拉丁文全谱。

### 字号阶梯（rem-free，固定 px，桌面端更稳）

| 用途 | 字号 | 字重 | 行高 |
|---|---|---|---|
| display-lg（注册页标题） | 40px | 700 | 1.1 |
| display（页面大标题） | 28px | 700 | 1.2 |
| h1 | 20px | 600 | 1.3 |
| h2 | 16px | 600 | 1.4 |
| h3 / 卡片标题 | 14px | 600 | 1.4 |
| body | 14px | 400 | 1.5 |
| label / 按钮 | 13px | 500 | 1.4 |
| caption / 辅助 | 12px | 400 | 1.4 |
| micro（UID、时间戳） | 11px | 500 | 1.3 |

**禁用**：< 11px 字号、`font-weight: 300`、`letter-spacing > 0.05em`（中文会散）。

### 数字显示约定

- UID、端口号、IP、版本号一律用等宽字体 `--font-mono`
- 玩家分数、帧率、延迟：等宽 + 固定宽度容器（避免跳动）

---

## 4. 间距系统（4px 基础网格）

```
0    2    4    6    8    10   12   16   20   24   32   40   48   64
```

- 组件内部 padding：`8/12/16`
- 卡片 padding：`16/20`
- 页面 padding：`24/32`
- 区块之间间距：`20/24/32`

**禁用**：奇数 px（1/3/5/7/…除了 1px 边框）、>64px 间距。

---

## 5. 布局骨架

整个 App 沿用 VRCSM 三栏可拖布局，但角色不同：

```
┌────────────────────────────────────────────────────────────────┐
│ TitleBar (高 36px — 窗口控制 + 面包屑 + 搜索 + 状态)                │
├──────┬─────────────────────────────────────────┬─────────────┤
│      │                                          │             │
│ Nav  │             Main Content                 │  Right Dock │
│ Rail │  (路由切换：大厅/频道/房间/好友/设置)          │  (可折叠：   │
│      │                                          │   上下文面板) │
│ 72px │                                          │  240-360px  │
│      │                                          │             │
├──────┴─────────────────────────────────────────┴─────────────┤
│ StatusBar (高 24px — 连接状态 / 当前身份 / 版本 / 通知)            │
└────────────────────────────────────────────────────────────────┘
```

- **NavRail（左）72px 固定宽**：纯 icon（Lucide），模仿 Discord 服务器栏 + VSCode 活动栏。
- **RightDock（右）可折叠**：根据当前页面内容切换用途（好友列表/房间信息/频道成员）。
- 主内容区最小 480px，达不到隐藏 RightDock。
- 窗口最小尺寸：1024 × 640。

### 为什么不要 Discord 双侧栏（服务器 + 频道）

Discord 左栏其实是 **服务器** + **频道** 两列，视觉拥挤。我们简化：
- 左栏只保留功能入口（大厅 / 频道 / 好友 / 我的房间 / 设置）
- 频道列表放进"频道"页内部的子栏，延迟加载
- 服务器/房间列表不占永驻空间，需要时在大厅展开

---

## 6. 核心组件规范

### 6.1 按钮

```
primary    背景 brand-primary      文字白          hover 亮 6%
secondary  背景 surface-bright     文字 fg        hover 亮 4%
ghost      透明                    文字 fg-muted  hover surface-hover
danger     背景 danger             文字白          hover 亮 6%
link       透明                    文字 brand-primary 下划线 on hover
```

- 高度：`sm 28px / md 36px / lg 44px`
- 禁止：渐变填充、内阴影、大于 `4px` 的 drop-shadow

### 6.2 卡片

```
背景：surface-raised
边框：1px border
圆角：10px
hover：边框色变 border-strong，卡片整体上移 1px（transform: translateY(-1px)）
active/selected：边框色变 brand-primary，左侧 3px 实心条
```

### 6.3 输入框

```
背景：surface（比卡片更暗，视觉下沉）
边框：1px border
圆角：6px
focus：边框 brand-primary + 0 0 0 3px brand-primary/20 外发光
禁用：亮度降 40%，鼠标 not-allowed
```

### 6.4 头像

- 圆形（`border-radius: 50%`）
- 状态点叠加右下角：直径 12px，白色 2px 外环
- 尺寸：`xs 20 / sm 28 / md 40 / lg 56 / xl 96`
- 无图时：UID 哈希生成渐变底 + 姓名首字符

### 6.5 服务器/房间卡片（大厅关键组件）

```
┌─────────────────────────────────────────┐
│ [封面 40x40]  TH08 · 永夜抄联机 Lv.4        │ ← 标题 14px/600
│               房主：dwgx · 2/4 玩家         │ ← caption 12px
│               [PING 24ms] [锁] [观战 3]     │ ← 徽章行
└─────────────────────────────────────────┘
```

- 高度固定 72px，可扫读
- 悬停时显示"加入"按钮（右侧滑入）
- 满员、锁定、好友在里面 → 三种明确视觉反馈

---

## 7. 图标系统

- 唯一来源：**lucide-react**（跟 VRCSM 一致）
- 尺寸：`14 / 16 / 20` — 禁止其他尺寸
- 线宽：`1.5px`（默认）
- 颜色：currentColor，跟随文字
- **永不**：彩色 emoji 当图标用（emoji 只用于消息内容）

### 关键功能图标映射

| 功能 | Icon |
|---|---|
| 大厅 | `Compass` |
| 频道 | `Hash` |
| 好友 | `Users` |
| 房间/服务器 | `Gamepad2` |
| 私信 | `MessageCircle` |
| 通知 | `Bell` |
| 设置 | `Settings` |
| 个人 | `User` |
| 麦克风（后期） | `Mic` |
| 观战 | `Eye` |
| 邀请 | `UserPlus` |

---

## 8. 东方味道 — 如何点到即止

**不要做**：
- 全页面樱花飘落特效（廉价）
- 灵梦魔理沙卡通吉祥物（版权）
- 日式毛笔字 logo（过度）

**可以做**：
- 头像无图占位：用 UID 哈希映射到灵梦/魔理沙/咲夜/妖梦/萃香 **5 个抽象符号**（几何化，不是原画）
- 加载 spinner：阴阳玉旋转（黑白圆点），简化几何风
- 空状态插画：浅色几何红白衣、黑白小魔女帽（色块 + 线条，非原画）
- 音效：按钮点击 150Hz 短音（可关闭，默认关）

**关键**：所有同人元素**原创绘制**（或调用生成模型），**绝不引用 ZUN 原画或二次同人作品**。

---

## 9. 响应式策略

桌面独占，不考虑手机。
唯二需要响应的断点：

```
--bp-narrow:  1024px   宽度小于此，RightDock 自动折叠
--bp-compact: 1280px   卡片改两列（默认三列）
```

---

## 10. 动效细节

```css
--motion-fast:     120ms;
--motion-normal:   180ms;
--motion-slow:     300ms;
--ease-standard:   cubic-bezier(0.2, 0, 0, 1);
--ease-emphasized: cubic-bezier(0.2, 0, 0, 1.2);  /* 仅入场关键元素 */
```

场景分配：
- 悬停态变色、Tooltip 出现：`fast`
- Modal/抽屉进场、路由切换：`normal`
- 房间大跳转（如游戏启动）：`slow`

**禁用**：连续页面都动、每个卡片 stagger 进场、鼠标 trail。

---

## 11. 可访问性底线

- 对比度：正文 ≥ 4.5:1，辅助 ≥ 3:1（夜间场景不降标准）
- Focus ring 必须可见，键盘 Tab 顺序合逻辑
- 所有交互元素 Enter/Space 可激活
- `prefers-reduced-motion` 生效时关掉非必要动效
- 所有 icon-only 按钮必须有 aria-label

---

## 12. 声音 Voice & Tone

### 中文

- 温和、准确、不卖萌
- 禁用：「小哥哥/姐姐」「嘿嘿」「嗷呜」
- 错误消息：先说"发生了什么"，再说"怎么办"
- 示例：
  - 好：「房间已满，你可以加入观战席或等待空位」
  - 差：「喵呜～房间满啦 o(*￣▽￣*)ブ」

### 英语

- Direct, Steam-style（不是 Discord 的青少年感）
- 示例：`Room is full. You can join as a spectator.`

### 日语

- 敬体（です・ます）
- 示例：`ルームが満員です。観戦席に参加できます。`

---

## 13. 设计决策备忘录（历史决策不回头再讨论）

| 决策 | 何时定的 | 为什么 |
|---|---|---|
| 用 #8b5cf6 靛紫而非 Discord 蓝紫 | 2026-04-25 | 更现代、避免"抄袭感"、配樱粉点缀更和谐 |
| 不做亮色主题（V1） | 2026-04-25 | 桌面客户端 + 游戏玩家 = 90% 深色，先集中精力 |
| 不做毛玻璃/Mica | 2026-04-25 | 性能+视觉稳定性，跨 Windows 10/11 一致 |
| 字体用 Inter + Noto Sans SC/JP | 2026-04-25 | 覆盖中英日，免费商用 |
| Icon 只用 lucide-react | 2026-04-25 | 一致性，VRCSM 已验证 |
| 东方元素原创绘制 | 2026-04-25 | 版权安全 |

---

*Last updated: 2026-04-25. 改动此文档必须同步改 `src/styles/globals.css` 和任何在途 Claude Design prompt。*
