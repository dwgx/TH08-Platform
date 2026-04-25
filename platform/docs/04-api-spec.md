# API Spec — TH-Platform

> 所有 REST 路由 + WebSocket 消息的权威定义。
> 前后端协作的合同。改一行都要回来同步这份。

---

## 0. 通用约定

### Base URL

- Production: `https://api.th-platform.example.com`
- Staging:    `https://api-staging.th-platform.example.com`
- Local dev:  `http://localhost:8080`

### 版本化

- 所有路径以 `/api/v1/` 开头
- Breaking change 时走 `/api/v2/`，v1 并行保留至少 6 个月

### 认证

- 登录后拿到 `access_token` (JWT, 15min) + `refresh_token` (30d)
- 所有受保护接口需 `Authorization: Bearer <access_token>`
- WebSocket 也带 token：`wss://.../ws?token=<access>`（握手时校验，之后不重校直到 disconnect）
- Access token 过期 → 前端用 refresh 换新的。refresh token 一次性（轮换），重复使用视为泄漏 → 服务端强制下线

### 请求/响应格式

- `Content-Type: application/json; charset=utf-8`
- 所有时间 ISO 8601 UTC 带 Z 后缀：`"2026-04-25T09:30:00Z"`
- UID 字段 JSON 中序列化为**字符串**（JS 精度问题）：`"uid": "10328"`

### 错误格式（统一）

```json
{
  "error": {
    "code": "room_full",
    "message": "This room is at player capacity.",
    "detail": { "room_id": "...", "max_players": 2 },
    "request_id": "01JZ..."
  }
}
```

| HTTP | 场景 |
|---|---|
| 400 | 参数错误 / validation failed |
| 401 | 未登录 / token 过期 |
| 403 | 登录但无权限 |
| 404 | 资源不存在 |
| 409 | 冲突（handle 占用、已经是好友） |
| 410 | 资源已删除 |
| 422 | 语义错误（如房间满员） |
| 429 | 限流 |
| 500 | 服务端错误 |

### 分页

游标分页，**不用 offset**：

```
GET /resource?limit=50&before=<opaque_cursor>
GET /resource?limit=50&after=<opaque_cursor>
```

响应：
```json
{
  "items": [...],
  "next_cursor": "...",
  "prev_cursor": "..."
}
```

### 限流

- IP: 100 req/min
- User: 60 req/min（敏感 endpoint 另有更严限流，如 `/auth/login` 10/min）
- 超限响应 429 + `Retry-After` header

---

## 1. Auth

### `POST /api/v1/auth/register-email`

```json
// request
{
  "email": "foo@bar.com",
  "password": "at-least-8-chars",
  "username": "dwgx",
  "handle": "dwgx",          // 可选；没给就从 username 规范化
  "locale": "zh-CN"
}

// response 201
{
  "uid": "10328",
  "handle": "dwgx",
  "verification_sent": true    // 邮箱验证邮件已发
}
```

### `POST /api/v1/auth/login-email`

```json
// request
{ "identifier": "foo@bar.com OR dwgx", "password": "..." }

// response 200
{
  "access_token": "eyJ...",
  "refresh_token": "eyJ...",
  "expires_in": 900,
  "user": { /* UserProfile */ }
}
```

### `POST /api/v1/auth/refresh`

```json
// request
{ "refresh_token": "..." }

// response 200
{ "access_token": "...", "refresh_token": "...", "expires_in": 900 }
```

### `POST /api/v1/auth/logout`

撤销当前 session 的 refresh token。

### `POST /api/v1/auth/logout-all`

撤销所有 session（改密码后自动触发）。

### `POST /api/v1/auth/verify-email`

```json
{ "token": "..." }   // 从邮件链接里的 token
```

### `POST /api/v1/auth/request-password-reset`

```json
{ "email": "..." }
```

### `POST /api/v1/auth/reset-password`

```json
{ "token": "...", "new_password": "..." }
```

### OAuth flow — 以 GitHub 为例

```
POST /api/v1/auth/oauth/github/start
  → 返回 { authorize_url, state }  前端打开这个 URL

回调 URL: /api/v1/auth/oauth/github/callback?code=...&state=...
  → 服务端用 code 换 GitHub token
  → 查 user_oauth 表：
     有绑定 → 直接签发 JWT
     无绑定 → 返回 { signup_ticket, github_profile }，前端引导用户"绑定现有账号"或"新建账号"
```

QQ / Discord / Steam 同模式，区别只在 provider ID。

### `POST /api/v1/auth/oauth/bind`

已登录状态下追加绑定新的 provider。

---

## 2. Users

### `GET /api/v1/users/me`

返回当前用户完整 profile。

```json
{
  "uid": "10328",
  "username": "dwgx",
  "handle": "dwgx",
  "email": "foo@bar.com",
  "email_verified": true,
  "avatar_url": "https://...",
  "bio": "Touhou Enjoyer since 2004",
  "tags": ["th08-main","coop-friendly"],
  "locale": "zh-CN",
  "country": "CN",
  "roles": [],
  "created_at": "2026-03-01T00:00:00Z",
  "oauth": [
    { "provider": "github", "provider_username": "dwgx", "bound_at": "..." },
    { "provider": "discord", "provider_username": "dwgx#1234", "bound_at": "..." }
  ]
}
```

### `PATCH /api/v1/users/me`

```json
{
  "username": "dwgx",              // optional
  "handle": "dwgx-new",            // optional; 409 if taken
  "bio": "...",
  "tags": [...],
  "locale": "ja",
  "country": "JP"
}
```

### `PUT /api/v1/users/me/avatar`

Multipart: `avatar` 字段，限制 2MB，仅 jpg/png/webp。返回新的 `avatar_url`。
V1 直接写本地磁盘，V2 切对象存储。

### `GET /api/v1/users/:uid`

返回公开 profile（不含 email、不含 oauth 列表）。

### `GET /api/v1/users/by-handle/:handle`

同上，按 handle 查。

### `GET /api/v1/users/search?q=<query>&limit=20`

模糊搜 username/handle，走 pg_trgm。

---

## 3. Friends

### `GET /api/v1/friends`

```json
{
  "items": [
    {
      "friend": { "uid": "10329", "username": "嗯呐", "handle": "nnna",
                  "avatar_url": "...", "last_seen_at": "..." },
      "status": "accepted",
      "note": null,
      "tags": [],
      "online_status": "online",          // 实时从 presence 算出
      "current_room": { "id": "...", "name": "..." } // 可选
    }
  ]
}
```

### `GET /api/v1/friends/pending`

收到的未决好友请求（`pending_in`）。

### `POST /api/v1/friends/requests`

```json
{ "target_uid": "10329" }
// or { "target_handle": "nnna" }
```

- 对方 block 了自己 → 403 silently（实际返回和成功无差别，防止侦测）
- 已经是好友 → 409

### `POST /api/v1/friends/requests/:uid/accept`
### `POST /api/v1/friends/requests/:uid/decline`

### `DELETE /api/v1/friends/:uid`

### `PATCH /api/v1/friends/:uid`

```json
{ "note": "幻想乡 VIP", "tags": ["main-team"] }
```

### `GET /api/v1/blocks`
### `POST /api/v1/blocks` body: `{ "uid": "...", "reason": "..." }`
### `DELETE /api/v1/blocks/:uid`

---

## 4. DMs (Direct Messages)

### `GET /api/v1/dms`

已有会话列表（按 `last_message_at` 倒序）。

### `POST /api/v1/dms`

Idempotent：对同一对用户始终返回同一个 thread。

```json
{ "peer_uid": "10329" }
// response
{ "thread_id": "...", "peer": { /*UserProfile*/ } }
```

### `GET /api/v1/dms/:thread_id/messages?limit=50&before=<id>`

### `POST /api/v1/dms/:thread_id/messages`

```json
{
  "content": "看看这个房间",
  "kind": "text",              // 'text' | 'emoji' | 'room_invite'
  "meta": { }                  // room_invite 时 { room_id }
}
```

### `PATCH /api/v1/dms/messages/:id` body: `{ content }` — 5 分钟内可编辑

### `DELETE /api/v1/dms/messages/:id` — 软删，content 替换为 `[deleted]`

### `POST /api/v1/dms/:thread_id/read` body: `{ "up_to_message_id": 12345 }`

---

## 5. Groups (频道/群组)

### `GET /api/v1/groups` — 我参与的群组
### `POST /api/v1/groups` — 创建群组

```json
{
  "name": "TH08 天朝联机群",
  "visibility": "private",
  "description": "..."
}
```

### `GET /api/v1/groups/:id` — 群组详情
### `PATCH /api/v1/groups/:id` — 只有 owner/admin
### `DELETE /api/v1/groups/:id` — 只有 owner

### `POST /api/v1/groups/:id/invite` body: `{ "uid": "10329" }`
### `POST /api/v1/groups/:id/join` body: `{ "invite_token": "..." }` — 接受邀请进群
### `POST /api/v1/groups/:id/leave`
### `DELETE /api/v1/groups/:id/members/:uid` — kick，owner/admin 权限
### `PATCH /api/v1/groups/:id/members/:uid` body: `{ "role": "admin", "nickname": "" }`

### `GET /api/v1/groups/:id/channels`
### `POST /api/v1/groups/:id/channels`
### `PATCH /api/v1/channels/:id`
### `DELETE /api/v1/channels/:id`

### `GET /api/v1/channels/:id/messages?limit=50&before=<id>`
### `POST /api/v1/channels/:id/messages`

```json
{
  "content": "开一局",
  "kind": "text",
  "reply_to_id": 12345,   // 可选
  "meta": {}
}
```

### `POST /api/v1/channels/:id/read` body: `{ "up_to_message_id": 12345 }`

---

## 6. Games (游戏版本元数据)

### `GET /api/v1/games` — 所有可用游戏版本
### `GET /api/v1/games/:id` — 包含 characters + config_schema

---

## 7. Rooms (核心)

### `GET /api/v1/rooms` — 浏览大厅房间列表

Query params:
- `kind=official,personal,community` 筛选类型
- `game=th08-1.00d` 筛选游戏
- `state=lobby` 默认只列可加入的
- `host_uid=<uid>` 按房主
- `limit` `before`/`after` 分页

响应每条：
```json
{
  "id": "...",
  "code": "TH8-2F9A",
  "name": "新手局随便来",
  "kind": "personal",
  "game_id": "th08-1.00d",
  "host": { "uid":"10328", "username":"dwgx", "avatar_url":"..." },
  "player_count": 1,
  "max_players": 2,
  "spectator_count": 0,
  "max_spectators": 8,
  "visibility": "public",
  "join_mode": "open",
  "region": "cn-sh",
  "created_at": "...",
  "has_password": false,
  "friends_inside": 0    // 我的好友在此房间数量（个性化）
}
```

### `POST /api/v1/rooms`

```json
{
  "name": "新手局随便来",
  "game_id": "th08-1.00d",
  "kind": "personal",
  "visibility": "public",       // 'public'|'private'|'unlisted'
  "join_mode": "open",           // 'open'|'ask'|'closed'
  "password": "optional-string",
  "max_players": 2,
  "max_spectators": 8,
  "region": "cn-sh",
  "config": { /* 参考 games.config_schema 填的参数 */ },
  "description": ""
}
// response 201
{
  "id": "...",
  "code": "TH8-2F9A",
  "relay_hints": [
    { "host": "relay-cn.th-platform.example.com", "port": 3478, "region": "cn-sh", "rtt_ms_est": 18 }
  ],
  /* ...full room ... */
}
```

### `GET /api/v1/rooms/:id`

完整房间状态 + 成员列表 + 观战列表。

### `PATCH /api/v1/rooms/:id`

仅 host，更新 name/visibility/join_mode/password/max_\* /config。触发 `config_dirty=true`。

### `POST /api/v1/rooms/:id/join`

```json
// request (optional fields)
{ "password": "...", "as": "player" }   // 'player'|'spectator'

// response 200
{
  "room": { ... },
  "role": "player",
  "slot": 2,                    // 玩家槽位 P1/P2
  "relay_ticket": {             // 给客户端建 UDP 用
    "token": "<signed-blob>",
    "expires_at": "...",
    "peer_hints": [             // 同房其他成员的候选端点
      { "uid":"10328", "public_ip":"1.2.3.4", "port":49155 }
    ]
  }
}
```

错误情况：
- 满员 → 422 `room_full`
- join_mode=closed → 403 `closed_room`
- join_mode=ask → 202 返回 `{ "request_id": "..." }`，等房主审批

### `POST /api/v1/rooms/:id/leave`

### `POST /api/v1/rooms/:id/kick` body: `{ "uid":"..." }` 仅 host

### `POST /api/v1/rooms/:id/invite` body: `{ "uid":"..." }` 发邀请

### `POST /api/v1/rooms/:id/approve` body: `{ "request_id":"..." }` 房主批准 ask-join

### `POST /api/v1/rooms/:id/start`

```json
// request
{ "countdown_seconds": 5 }

// 状态机 lobby → playing
// 广播 room.state_changed
```

### `POST /api/v1/rooms/:id/pause`
### `POST /api/v1/rooms/:id/resume`
### `POST /api/v1/rooms/:id/end`

### `POST /api/v1/rooms/:id/match-result`

对局结束后客户端（DLL 上报给 Tauri，Tauri 再 POST）提交结果。

```json
{
  "started_at": "...",
  "ended_at": "...",
  "duration_ms": 180000,
  "result": {
    "winner_uid": "10328",
    "scores": { "10328": 823400, "10329": 698000 },
    "deaths":  { "10328": 2, "10329": 5 },
    "cause": "cleared_stage_6"
  },
  "dll_version": "0.1.0"
}
```

### `POST /api/v1/rooms/:id/character` body: `{ "character_id":"reimu_yukari" }`

改角色（只在 lobby 状态允许）。

---

## 8. Lobby 公屏

### `GET /api/v1/lobby/messages?limit=100&before=<id>`

### `POST /api/v1/lobby/messages`

```json
{ "content": "..", "kind": "text" }
```

限流：2 条/5秒/用户。

---

## 9. Notifications

### `GET /api/v1/notifications?unread_only=true&limit=50`
### `POST /api/v1/notifications/:id/read`
### `POST /api/v1/notifications/read-all`

---

## 10. Reports

### `POST /api/v1/reports`

```json
{
  "target_kind": "user",
  "target_id": "10329",
  "reason": "harassment",
  "detail": "..."
}
```

---

# WebSocket Protocol

## 11. 连接 + 握手

```
Client:  GET wss://api.../ws?token=<access>
Server:  101 Switching Protocols
Server:  { "type": "hello", "session_id": "...", "heartbeat_interval_ms": 30000 }
Client:  { "type": "subscribe", "topics": ["lobby", "friends"] }
Server:  { "type": "subscribed", "topics": ["lobby", "friends"] }
```

心跳：Client 每 `heartbeat_interval_ms` 发 `{ "type": "ping" }`，Server 回 `{ "type": "pong" }`。
30s 无心跳 Server 主动关闭。

## 12. 消息信封

所有消息都是 JSON 对象，**顶层必有 `type` 字段**。可选 `id`（客户端发的请求用于匹配 response）。

## 13. 客户端 → 服务端

| type | 作用 | payload |
|---|---|---|
| `ping` | 心跳 | – |
| `subscribe` | 订阅 topic | `{ topics: ["lobby","room:<id>","channel:<id>","dm:<thread>"] }` |
| `unsubscribe` | 取消订阅 | `{ topics: [...] }` |
| `typing.start` / `typing.stop` | 正在输入 | `{ target: "channel:<id>" / "dm:<id>" }` |
| `presence.update` | 手动改状态 | `{ status: "idle"\|"dnd"\|"online" }` |
| `room.ready` / `room.unready` | 房间内就绪态 | `{ room_id: "..." }` |

## 14. 服务端 → 客户端

### 14.1 通用

| type | 触发 | payload |
|---|---|---|
| `hello` | 握手成功 | `{ session_id, user: UserProfile }` |
| `pong` | 心跳回应 | – |
| `error` | 协议错误或业务错 | `{ code, message, request_id? }` |
| `subscribed` | 订阅确认 | `{ topics: [...] }` |

### 14.2 好友 / Presence

| type | 含义 |
|---|---|
| `friends.online` | 好友上线 `{ uid, username, avatar_url }` |
| `friends.offline` | 好友下线 `{ uid }` |
| `friends.status` | 状态变化 `{ uid, status, current_room? }` |
| `friends.request.new` | 新好友请求 `{ from: UserProfile }` |
| `friends.request.accepted` | 我的申请被接受 |

### 14.3 聊天

| type | 含义 |
|---|---|
| `lobby.message` | 大厅新消息 `{ message: LobbyMessage }` |
| `channel.message` | 频道新消息 `{ channel_id, message }` |
| `channel.typing` | 某人正在频道输入 `{ channel_id, user_uid }` |
| `dm.message` | DM 新消息 `{ thread_id, message }` |
| `dm.typing` | DM 对方正在输入 |

### 14.4 房间

| type | 含义 |
|---|---|
| `lobby.room.created` | 大厅新房间 |
| `lobby.room.updated` | 房间信息变化（仍在列表中） |
| `lobby.room.closed` | 房间关闭（从列表移除） |
| `room.member_joined` | `{ room_id, member: RoomMember }` |
| `room.member_left` | `{ room_id, uid, reason }` |
| `room.member_updated` | 角色/槽位变化 |
| `room.state_changed` | lobby ↔ playing ↔ paused |
| `room.config_changed` | `{ room_id, config }` |
| `room.message` | 房间内聊天（V1 暂不做，用 DM/channel 替代） |
| `room.invitation.new` | 收到房间邀请 `{ invitation: RoomInvitation }` |
| `room.join_request.new` | 房主收到加入申请（join_mode=ask） |
| `room.relay.hint_update` | 中继端点变化（补发 relay_ticket） |

### 14.5 通知

| type | 含义 |
|---|---|
| `notifications.new` | 新通知（需要在侧边栏小红点） |

---

## 15. 订阅规则

客户端登录后默认订阅 `user:<self_uid>`（服务器推私人事件用）。
进入大厅页 → subscribe `lobby`。
打开频道 → subscribe `channel:<id>`。
进入房间 → subscribe `room:<id>`。
离开页面时 unsubscribe，减少服务器 fanout 压力。

---

## 16. 错误码一览

```
auth_required
invalid_credentials
token_expired
token_revoked
email_not_verified
rate_limited
handle_taken
email_taken
user_not_found
friend_already
friend_blocked
blocked_by_target
room_not_found
room_full
room_closed
room_password_required
room_password_wrong
room_ask_pending
room_invitation_expired
game_not_supported
dll_version_mismatch
validation_failed
internal_error
```

---

## 17. Relay Ticket 细节

Relay ticket 是服务端签名的短寿命 token（5 分钟），客户端拿去连中继：

```
POST <relay-host>:<port>/udp-associate
Headers:
  X-Relay-Ticket: <ticket-token>

Body (CBOR):
  { session_id: "...", room_id: "...", peer_uids: [...] }
```

Relay 验签（用 gateway 同样的 RS256 pubkey）→ 分配端口 → 返回 `{ allocated_port, peer_ips }`。

详见 `docs/05-relay-protocol.md`（后续单独文档，V1 skeleton 够用）。

---

*Last updated: 2026-04-25.*
