# i18n Framework — TH-Platform

> 底层使用 `i18next` + `react-i18next`，已在 `client/src/i18n/` 配好。
> 本文档解释"怎么用好它"和"怎么让 AI 帮我们翻译"。

---

## 0. 设计目标

1. **默认中文**（zh-CN），英/日为完整补充，后续可加韩/泰/俄
2. **类型安全**：错拼 key 在编译期就报错
3. **按需加载**：大词表（例如角色 lore）不一次性塞进 bundle
4. **翻译可维护**：`zh-CN` 是权威源，其他语言从它派生
5. **AI 辅助翻译**：新增 key 自动调用 Claude API 翻译，人工审阅
6. **回退链清晰**：缺翻译 → en → zh-CN → key 字面量

---

## 1. Key 命名规范

### 约定

```
<domain>.<sub>.<name>[.<variant>]
```

- 全小写 + `.` 分隔
- **domain** 对应功能模块（`auth`, `lobby`, `room`, `friends`…）
- **sub** 组件或动作分类（`state`, `errors`, `button`…）
- **name** 是具体短语
- **variant** 仅在多形态时用（`_plural`, `_male` 等 — V1 不用）

### 例子

```json
{
  "auth.login": "登录",
  "auth.login_success": "欢迎回来",
  "auth.login_failed": "账号或密码错误",
  "room.state.lobby": "准备中",
  "room.capacity": "{{current}} / {{max}}",
  "errors.room_full": "房间已满"
}
```

### 禁止

- 中文当 key：`"登录": "Login"` ❌（破坏语义）
- 超过 4 级 domain：`a.b.c.d.e` ❌
- 句号或英文标点放 key 里：`"hi.world!": "..."` ❌

---

## 2. 命名空间（namespace）

V1 只有一个 `common.json`。**达到一定规模才分 namespace**：

| 条件 | 推荐 |
|---|---|
| 单 locale 文件 < 1000 key | 保持 `common.json` |
| 单 locale 文件 1000 ~ 3000 key | 按 domain 拆 `auth.json`, `lobby.json`, … |
| > 3000 key | 再按 page 细拆 |

拆分后在 `i18n/index.ts` 注册：

```ts
import lobby_zh from "./locales/zh-CN/lobby.json";
// ...
resources: {
  "zh-CN": { common: common_zh, lobby: lobby_zh },
}
```

组件用 `useTranslation("lobby")`。

---

## 3. 回退链

```
用户语言          第一查找     第二查找     最后回退
─────────────────────────────────────────────────────
zh-CN / zh-*     zh-CN       en          key 字面量
ja / ja-JP       ja          zh-CN       en
ko / ko-KR       ko (未来)   zh-CN       en
en / en-*        en          zh-CN       key 字面量
其他              navigator   zh-CN       key 字面量
```

`fallbackLng: ["zh-CN", "en"]` 在 `initI18n()` 中配置。

---

## 4. 类型安全（Type-Safe Keys）

### 方案 A — 纯手工（V1 默认）

没有 codegen，在 `src/i18n/types.d.ts` 里手写：

```ts
// auto-generated-ish — regen when locale JSON changes
import common from "./locales/zh-CN/common.json";

declare module "react-i18next" {
  interface CustomTypeOptions {
    defaultNS: "common";
    resources: { common: typeof common };
  }
}
```

之后 `t("auth.login")` IDE 自动补全，`t("auth.logn")` 编译报错。

### 方案 B — codegen（V2 再上）

```bash
pnpm dlx i18next-resources-for-ts toc-path src/i18n/locales/zh-CN --out src/i18n/resources.d.ts
```

写进 `package.json` 的 `postinstall` + `prebuild`。V1 不用，手工维护够了。

---

## 5. 插值、复数、格式化

### 插值（值替换）

```json
"room.capacity": "{{current}} / {{max}}"
```

```tsx
t("room.capacity", { current: 2, max: 4 })  // => "2 / 4"
```

### 复数（V2 再用 — 中文几乎不需要）

```json
"friends.count_one": "{{count}} friend",
"friends.count_other": "{{count}} friends"
```

中文/日文用单数即可：

```json
"friends.count": "{{count}} 位好友"
```

### 日期 / 数字

统一走 `Intl.DateTimeFormat` / `Intl.NumberFormat`，不写进 i18n：

```ts
new Intl.DateTimeFormat(i18n.language, { dateStyle: "short", timeStyle: "short" })
  .format(new Date(iso));
```

小工具放 `src/lib/format.ts`。

---

## 6. Claude-API 辅助翻译管线

### 思路

- `zh-CN` 是权威源。其他语言是它的派生。
- 当 `zh-CN/common.json` 新增/修改了 key，自动检测缺失，调 Claude API 批量翻译 → 写入 `en/common.json` / `ja/common.json` → 打开 PR → 人工审阅合并。
- 翻译质量 > 速度：prompt 明确语气、上下文、禁止项。

### CLI 脚本（`tools/i18n-sync.ts`）

V2 时加。V1 暂时可以只手工改 en/ja，zh-CN 为主。

```ts
// tools/i18n-sync.ts — run with `pnpm i18n:sync`
//
// 1. diff zh-CN/common.json vs en/common.json and ja/common.json
// 2. for each missing or outdated key, batch call Claude API
// 3. write back, format with prettier
// 4. print summary: N keys added, M keys updated
//
// Uses the system prompt below. Requires ANTHROPIC_API_KEY.
```

### 翻译系统 prompt（模板）

```
You are translating UI strings for TH-Platform, a desktop client for
multiplayer Touhou shoot-em-up games (similar to Discord + Parsec).

Voice guide:
- Calm, precise, Steam-tier professional (no kaomoji, no 卖萌)
- Error messages: state what happened, then state how to fix
- Use native punctuation (。 in Chinese, 。 in Japanese, . in English)

Glossary (translate consistently):
- Room      → 房间 / ルーム / Room
- Lobby     → 大厅 / ロビー / Lobby
- Host      → 房主 / ホスト / Host
- Spectator → 观战 / 観戦 / Spectator
- Handle    → 用户名(@) / ハンドル / handle

Placeholders ({{like_this}}) must not be translated or reordered.

I will give you Chinese source strings as JSON. Return ONLY a JSON
object with the same keys in the target language.

Target language: ${target}

Source JSON:
${sourceJson}
```

### 预算 / 限流

- 调 Claude Haiku 4.5，便宜 + 质量足够做 UI 字符串
- 单次批 50-200 key，避免上下文过长
- 每批输出后 diff vs 已有翻译，只写入**新增**或 `// TODO: retranslate` 标记的 key

---

## 7. 切换语言（运行时）

```ts
import i18n from "@/i18n";
await i18n.changeLanguage("ja");
localStorage.setItem("th.locale", "ja");
```

Settings 页做下拉：

```tsx
const LOCALES = ["zh-CN", "en", "ja"] as const;
```

改语言之后：
- React 自动重渲染（react-i18next 订阅）
- 前端写 `localStorage["th.locale"]`（持久化）
- 若已登录，同时 PATCH `/users/me` `{ locale: "ja" }`（跨设备同步）

---

## 8. 加一门新语言的操作步骤

```bash
# 1. 新建 locale 目录
mkdir -p src/i18n/locales/ko

# 2. 复制 zh-CN 当种子
cp src/i18n/locales/zh-CN/common.json src/i18n/locales/ko/common.json

# 3. （V2）运行 pnpm i18n:sync
#    或者手工翻完

# 4. 注册到 i18n/index.ts
#    - import common_ko from "./locales/ko/common.json";
#    - resources: { ..., ko: { common: common_ko } }
#    - SUPPORTED_LOCALES 加 'ko'
#    - LOCALE_LABELS 加 'ko': '한국어'

# 5. UI 走测试一遍，特别注意：
#    - 长文本是否溢出按钮
#    - 日期/数字格式
#    - 字体是否支持该语言（需要的话在 globals.css @import 字体）
```

---

## 9. 服务端 i18n（尽量少用）

**原则**：大部分用户可见的文字都在前端。后端只返回 error code，前端做翻译。

例：

```json
// 后端不返回 "Handle is taken"，而是：
{ "error": { "code": "handle_taken", "message": "Handle is taken." } }
```

前端：

```tsx
t(`errors.${err.code}`, { defaultValue: err.message })
```

这样加新语言零后端改动。

### 例外：邮件内容

邮件验证、密码重置这类是后端生成的文本，用户当前 locale 决定模板。
`internal/mailer/templates/<locale>/<template>.txt` 组织。
V1 先只有 `zh-CN` + `en` 两份。

---

## 10. 常见坑

| 坑 | 解决 |
|---|---|
| key 没翻译，前端显示 `"auth.login"` 字面量 | 在 `initI18n` 里 `returnEmptyString: false`，缺失时会走 `fallbackLng` |
| JSX 里想插 HTML/组件 | 用 `<Trans>` 而不是 `t()` |
| 同样一个短语"Save"，context 不同意思不同 | key 拆开：`settings.save` vs `room.save` |
| 长中文名溢出 | UI 预留 20% buffer，最长中文 8 字设计 |
| 日期在日语显示 "2026-04-25" 而不是 "2026年4月25日" | 用 `Intl.DateTimeFormat(locale, {dateStyle:'long'})` 而不是字符串拼接 |
| 动态字符串（服务端返回）没 i18n | 约定这类字段永远原文透传（如用户名），只有 UI 固定标签走 i18n |

---

## 11. 检查清单（PR 时)

- [ ] 新增 UI 字符串没有硬编码在 JSX 里
- [ ] 改了 `zh-CN/*.json` 的话，en/ja 的对应 key 也更新了（或留 `// TODO: retranslate`）
- [ ] 插值占位符在所有语言里拼写一致
- [ ] 长文本在最小窗口（1024px）下不溢出
- [ ] 错误消息走 error-code-to-key 模式而不是后端文本

---

*Last updated: 2026-04-25.*
