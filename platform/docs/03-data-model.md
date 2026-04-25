# Data Model — TH-Platform

> 所有表、字段、约束、索引的权威定义。
> DDL 在 `server/migrations/0001_init.up.sql`，这份文档是它的人读版。

---

## 0. 命名约定

- 表名：复数小写下划线（`users`, `room_members`）
- 主键：`id` 优先 UUIDv7（天然时间序），仅用户用自增 UID（见 §2）
- 外键：`<target>_id`（`user_id`, `room_id`）
- 时间戳：`created_at`, `updated_at`（timestamptz，UTC 存储）
- 软删除：`deleted_at NULL`（V1 仅 users/messages/rooms 用）
- JSON：jsonb 而非 json
- 枚举：CHECK 约束 + 常量串（不用 enum type，迁移痛苦）

---

## 1. 实体关系图（简版）

```
users ──< user_oauth            (一人多绑：QQ/Discord/GitHub)
users ──< user_sessions         (JWT revocation)
users ──< friendships >── users (自反 M:N)
users ──< blocks >── users      (自反 M:N)
users ──< dm_threads >── users  (两人开一条)
dm_threads ──< dm_messages
users ──< groups (owner)
groups ──< group_members >── users
groups ──< group_channels
group_channels ──< group_messages
users ──< rooms (host)
rooms ──< room_members >── users
rooms ──< room_invitations
rooms ──< match_history
games ──< rooms                 (游戏版本注册表)
users ──< notifications
users ──< lobby_messages        (大厅公屏，V1 无分类)
uid_vanity_pool                 (靓号待分配)
```

---

## 2. UID 分配策略（QQ 风，递增 + 靓号）

### 2.1 核心规则

1. 新用户的 UID 从一个**逻辑计数器**分配（`uid_seq` sequence，起始 `10000`，不 cycle）
2. 当计数器增长到某个阈值时，位数自然增长（10000 → 99999 → 100000 → ...）
3. 每次分配前**5% 概率**检查"靓号池"（`uid_vanity_pool` 表），如果命中 → 分配靓号并从池中移除
4. 靓号预生成规则：
   - AABB（1122, 3344 …）
   - ABAB（1212）
   - AAAA / AAAAAA
   - 顺子 123456
   - 带 8 多（888, 8888）
   - 带 66 / 666
5. 靓号池启动时生成前 1000 个，不够时批次补充

### 2.2 并发安全

- UID 分配走单条 SQL：`SELECT nextval('uid_seq')` + 可能的靓号替换在单个事务里
- 靓号池行级锁：`SELECT ... FOR UPDATE SKIP LOCKED LIMIT 1`

### 2.3 特殊号保留

- `10000` ~ `10099` 保留给管理员 / 团队成员
- `19961` 留给 ZUN（不可分配，致敬）
- `19940808` 保留（TH 系列 1994 开始 + 8 月 8 日）
- 这些保留号在 `uid_reserved` 表，注册时跳过

---

## 3. 表详解

### 3.1 `users`

```sql
uid           BIGINT       PRIMARY KEY        -- QQ 风递增，见 §2
username      TEXT         NOT NULL           -- 可改，可重复，display name
handle        CITEXT       NOT NULL UNIQUE    -- 可改，全局唯一（@handle）
email         CITEXT       UNIQUE             -- 邮箱注册才有，可为 NULL
email_verified_at timestamptz
password_hash TEXT                           -- argon2id，OAuth-only 用户可为 NULL
avatar_url    TEXT                           -- V1 用 Dicebear 或头像哈希色块
bio           TEXT                           -- <= 200 字
tags          TEXT[]        NOT NULL DEFAULT '{}'  -- 最多 10 个，自选
locale        TEXT          NOT NULL DEFAULT 'zh-CN'
country       TEXT                           -- ISO 3166-1 alpha-2
status        TEXT          NOT NULL DEFAULT 'active'
              CHECK (status IN ('active','suspended','banned','deleted'))
roles         TEXT[]        NOT NULL DEFAULT '{}'   -- 'admin','mod','supporter' etc
created_at    timestamptz   NOT NULL DEFAULT now()
updated_at    timestamptz   NOT NULL DEFAULT now()
deleted_at    timestamptz
last_seen_at  timestamptz                    -- 更新粒度 1 分钟，避免写放大
```

索引：
- `UNIQUE (handle)` — citext 大小写不敏感
- `UNIQUE (email) WHERE email IS NOT NULL`
- `(username)` — 模糊搜索用 pg_trgm

### 3.2 `user_oauth`

```sql
id              UUID          PRIMARY KEY DEFAULT gen_random_uuid()
user_uid        BIGINT        NOT NULL REFERENCES users(uid) ON DELETE CASCADE
provider        TEXT          NOT NULL CHECK (provider IN ('qq','discord','github','steam'))
provider_user_id TEXT         NOT NULL                 -- 对方家的 ID
provider_username TEXT                                 -- 可选，快照
access_token    TEXT                                   -- 加密存，用完即弃更好
refresh_token   TEXT
scope           TEXT
bound_at        timestamptz   NOT NULL DEFAULT now()
UNIQUE (provider, provider_user_id)                    -- 同一 provider 账号只能绑一个 TH 账号
UNIQUE (user_uid, provider)                            -- 一个 TH 账号同 provider 只绑一个
```

### 3.3 `user_sessions`

```sql
id            UUID          PRIMARY KEY DEFAULT gen_random_uuid()
user_uid      BIGINT        NOT NULL REFERENCES users(uid) ON DELETE CASCADE
refresh_token_hash TEXT     NOT NULL UNIQUE    -- SHA256 of refresh token
issued_at     timestamptz   NOT NULL DEFAULT now()
expires_at    timestamptz   NOT NULL
last_used_at  timestamptz   NOT NULL DEFAULT now()
user_agent    TEXT
ip            INET
revoked_at    timestamptz                       -- 登出或强制下线
```

策略：每次 refresh 轮换 token（detect 复用 = 全家下线 = 可能被盗）。

### 3.4 `friendships`

```sql
-- 存两条相反方向的记录，保持查询简单
user_uid      BIGINT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE
friend_uid    BIGINT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE
status        TEXT        NOT NULL
              CHECK (status IN ('pending_out','pending_in','accepted','blocked_out'))
note          TEXT                         -- 我给对方的备注
tags          TEXT[]      NOT NULL DEFAULT '{}'
created_at    timestamptz NOT NULL DEFAULT now()
updated_at    timestamptz NOT NULL DEFAULT now()
PRIMARY KEY (user_uid, friend_uid)
CHECK (user_uid <> friend_uid)
```

说明：
- A 给 B 发申请 → 插 `(A,B,'pending_out')` 和 `(B,A,'pending_in')`
- B 同意 → 两行都更新为 `accepted`
- B 拒绝 → 两行删除
- A 拉黑 B → `(A,B,'blocked_out')` 保留，`(B,A,*)` 删除

### 3.5 `blocks`

（独立表，不和 friendships 混）

```sql
user_uid       BIGINT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE
blocked_uid    BIGINT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE
reason         TEXT
created_at     timestamptz NOT NULL DEFAULT now()
PRIMARY KEY (user_uid, blocked_uid)
```

查询："当前用户能否看/收 B 的消息" = `NOT EXISTS (SELECT 1 FROM blocks WHERE user_uid=me AND blocked_uid=B)`

### 3.6 `dm_threads` & `dm_messages`

```sql
CREATE TABLE dm_threads (
  id           UUID        PRIMARY KEY DEFAULT gen_random_uuid(),
  user_a_uid   BIGINT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  user_b_uid   BIGINT      NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  created_at   timestamptz NOT NULL DEFAULT now(),
  last_message_at timestamptz,
  UNIQUE (LEAST(user_a_uid, user_b_uid), GREATEST(user_a_uid, user_b_uid)),
  CHECK (user_a_uid <> user_b_uid)
);

CREATE TABLE dm_messages (
  id          BIGSERIAL    PRIMARY KEY,   -- 顺序分配便于分页
  thread_id   UUID         NOT NULL REFERENCES dm_threads(id) ON DELETE CASCADE,
  sender_uid  BIGINT       NOT NULL REFERENCES users(uid),
  content     TEXT         NOT NULL,
  kind        TEXT         NOT NULL DEFAULT 'text'
              CHECK (kind IN ('text','emoji','room_invite','system')),
  meta        JSONB        NOT NULL DEFAULT '{}',
  created_at  timestamptz  NOT NULL DEFAULT now(),
  edited_at   timestamptz,
  deleted_at  timestamptz
);
CREATE INDEX ON dm_messages (thread_id, id DESC);  -- 倒序分页
```

每个 thread 最多两人，unique 约束用 LEAST/GREATEST 保证 (A,B) = (B,A)。

### 3.7 `groups`, `group_members`, `group_channels`, `group_messages`

```sql
CREATE TABLE groups (
  id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  owner_uid   BIGINT       NOT NULL REFERENCES users(uid),
  name        TEXT         NOT NULL,
  icon_url    TEXT,
  description TEXT,
  visibility  TEXT         NOT NULL DEFAULT 'private'
              CHECK (visibility IN ('public','private','unlisted')),
  member_count INT         NOT NULL DEFAULT 1,  -- 反规范化，避免 count(*)
  created_at  timestamptz  NOT NULL DEFAULT now(),
  updated_at  timestamptz  NOT NULL DEFAULT now(),
  deleted_at  timestamptz
);

CREATE TABLE group_members (
  group_id    UUID         NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
  user_uid    BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  role        TEXT         NOT NULL DEFAULT 'member'
              CHECK (role IN ('owner','admin','member')),
  nickname    TEXT,
  joined_at   timestamptz  NOT NULL DEFAULT now(),
  muted_until timestamptz,
  PRIMARY KEY (group_id, user_uid)
);

CREATE TABLE group_channels (
  id          UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  group_id    UUID         NOT NULL REFERENCES groups(id) ON DELETE CASCADE,
  name        TEXT         NOT NULL,
  slug        TEXT         NOT NULL,   -- URL-safe
  topic       TEXT,
  position    INT          NOT NULL DEFAULT 0,
  created_at  timestamptz  NOT NULL DEFAULT now(),
  UNIQUE (group_id, slug)
);

CREATE TABLE group_messages (
  id          BIGSERIAL    PRIMARY KEY,
  channel_id  UUID         NOT NULL REFERENCES group_channels(id) ON DELETE CASCADE,
  sender_uid  BIGINT       NOT NULL REFERENCES users(uid),
  content     TEXT         NOT NULL,
  kind        TEXT         NOT NULL DEFAULT 'text'
              CHECK (kind IN ('text','emoji','room_invite','system')),
  meta        JSONB        NOT NULL DEFAULT '{}',
  reply_to_id BIGINT       REFERENCES group_messages(id) ON DELETE SET NULL,
  created_at  timestamptz  NOT NULL DEFAULT now(),
  edited_at   timestamptz,
  deleted_at  timestamptz
);
CREATE INDEX ON group_messages (channel_id, id DESC);
```

### 3.8 `games` — 游戏版本注册表

```sql
CREATE TABLE games (
  id            TEXT         PRIMARY KEY,     -- 'th08-1.00d', 'th07-1.00b' ...
  family        TEXT         NOT NULL,        -- 'th08', 'th07'
  display_name  TEXT         NOT NULL,        -- '永夜抄 (TH08) v1.00d'
  version_label TEXT         NOT NULL,        -- '1.00d'
  expected_exe_sha256 TEXT   NOT NULL,        -- 验证玩家本地 .exe
  supported_modes TEXT[]     NOT NULL DEFAULT '{}',  -- 'coop','versus','spectate'
  max_players   INT          NOT NULL DEFAULT 2,
  characters    JSONB        NOT NULL DEFAULT '[]',  -- [{id,label_cn,label_ja,label_en,color}]
  config_schema JSONB        NOT NULL DEFAULT '{}',  -- 开房间时可调参数的 JSON schema
  dll_version_min TEXT       NOT NULL,        -- 要求客户端 DLL 版本 >= this
  status        TEXT         NOT NULL DEFAULT 'active'
                CHECK (status IN ('planned','alpha','active','deprecated')),
  created_at    timestamptz  NOT NULL DEFAULT now()
);
```

**为什么把游戏元数据上数据库**：前端要根据游戏版本动态渲染角色选择、模式下拉、房间参数表单。放后端 = 加新游戏不用发客户端更新。

### 3.9 `rooms`

```sql
CREATE TABLE rooms (
  id            UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  code          TEXT         NOT NULL UNIQUE,   -- 短码 'TH8-2F9A' 供分享
  host_uid      BIGINT       NOT NULL REFERENCES users(uid),
  game_id       TEXT         NOT NULL REFERENCES games(id),
  name          TEXT         NOT NULL,
  description   TEXT,
  kind          TEXT         NOT NULL DEFAULT 'personal'
                CHECK (kind IN ('official','personal','community')),
  visibility    TEXT         NOT NULL DEFAULT 'public'
                CHECK (visibility IN ('public','private','unlisted')),
  join_mode     TEXT         NOT NULL DEFAULT 'open'
                CHECK (join_mode IN ('open','ask','closed')),
  password_hash TEXT,                             -- 可选房间密码
  max_players   INT          NOT NULL DEFAULT 2,
  max_spectators INT         NOT NULL DEFAULT 8,
  config        JSONB        NOT NULL DEFAULT '{}',  -- 当前生效的游戏参数
  config_dirty  BOOLEAN      NOT NULL DEFAULT false,  -- §5 房间生命周期需要
  state         TEXT         NOT NULL DEFAULT 'lobby'
                CHECK (state IN ('lobby','playing','paused','closed')),
  player_count  INT          NOT NULL DEFAULT 0,
  spectator_count INT        NOT NULL DEFAULT 0,
  region        TEXT,                             -- 'cn-sh', 'jp-tk', 'us-la'
  created_at    timestamptz  NOT NULL DEFAULT now(),
  last_active_at timestamptz NOT NULL DEFAULT now(),
  closed_at     timestamptz
);
CREATE INDEX ON rooms (state, visibility, kind) WHERE closed_at IS NULL;
CREATE INDEX ON rooms (host_uid) WHERE closed_at IS NULL;
```

### 3.10 `room_members`

```sql
CREATE TABLE room_members (
  room_id     UUID         NOT NULL REFERENCES rooms(id) ON DELETE CASCADE,
  user_uid    BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  role        TEXT         NOT NULL DEFAULT 'player'
              CHECK (role IN ('host','player','spectator')),
  character_id TEXT,                       -- 引用 games.characters[].id
  slot        SMALLINT,                    -- P1/P2 等位置
  joined_at   timestamptz  NOT NULL DEFAULT now(),
  PRIMARY KEY (room_id, user_uid)
);
CREATE INDEX ON room_members (user_uid);
```

### 3.11 `room_invitations`

```sql
CREATE TABLE room_invitations (
  id           UUID         PRIMARY KEY DEFAULT gen_random_uuid(),
  room_id      UUID         NOT NULL REFERENCES rooms(id) ON DELETE CASCADE,
  inviter_uid  BIGINT       NOT NULL REFERENCES users(uid),
  invitee_uid  BIGINT       NOT NULL REFERENCES users(uid),
  status       TEXT         NOT NULL DEFAULT 'pending'
               CHECK (status IN ('pending','accepted','declined','expired','revoked')),
  expires_at   timestamptz  NOT NULL DEFAULT (now() + interval '5 minutes'),
  created_at   timestamptz  NOT NULL DEFAULT now(),
  responded_at timestamptz
);
CREATE INDEX ON room_invitations (invitee_uid, status);
```

### 3.12 `match_history`

```sql
CREATE TABLE match_history (
  id            BIGSERIAL    PRIMARY KEY,
  room_id       UUID         NOT NULL REFERENCES rooms(id) ON DELETE CASCADE,
  game_id       TEXT         NOT NULL REFERENCES games(id),
  started_at    timestamptz  NOT NULL,
  ended_at      timestamptz  NOT NULL,
  duration_ms   INT          NOT NULL,
  result        JSONB        NOT NULL,      -- {winner_uid, scores, deaths, cause}
  replay_hash   TEXT,                        -- V2: 指向对象存储的回放
  dll_version   TEXT         NOT NULL,
  created_at    timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX ON match_history (room_id, id DESC);
```

### 3.13 `notifications`

```sql
CREATE TABLE notifications (
  id            BIGSERIAL    PRIMARY KEY,
  user_uid      BIGINT       NOT NULL REFERENCES users(uid) ON DELETE CASCADE,
  kind          TEXT         NOT NULL,      -- 'friend_request','room_invite','mention' ...
  title         TEXT         NOT NULL,
  body          TEXT,
  link          TEXT,                        -- 客户端内跳转，比如 '/rooms/<id>'
  meta          JSONB        NOT NULL DEFAULT '{}',
  read_at       timestamptz,
  created_at    timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX ON notifications (user_uid, id DESC);
CREATE INDEX ON notifications (user_uid, read_at) WHERE read_at IS NULL;
```

### 3.14 `lobby_messages`

大厅公屏，全平台一份。

```sql
CREATE TABLE lobby_messages (
  id          BIGSERIAL    PRIMARY KEY,
  sender_uid  BIGINT       NOT NULL REFERENCES users(uid),
  content     TEXT         NOT NULL,
  kind        TEXT         NOT NULL DEFAULT 'text'
              CHECK (kind IN ('text','emoji','room_invite','system')),
  meta        JSONB        NOT NULL DEFAULT '{}',
  created_at  timestamptz  NOT NULL DEFAULT now()
);
CREATE INDEX ON lobby_messages (id DESC);
```

按月分区（`PARTITION BY RANGE (created_at)`），V1 可不分区，5000 MAU 一年 9000 万行 PG 单表扛得住。

### 3.15 `uid_vanity_pool`

```sql
CREATE TABLE uid_vanity_pool (
  uid         BIGINT       PRIMARY KEY,
  pattern     TEXT         NOT NULL,      -- 'AABB','ABAB','REPEAT4' etc
  rarity      INT          NOT NULL DEFAULT 1,   -- 1..5
  reserved_at timestamptz
);

CREATE TABLE uid_reserved (
  uid         BIGINT       PRIMARY KEY,
  note        TEXT
);

CREATE SEQUENCE uid_seq START 10100;    -- 10000-10099 管理员，10100 起给普通用户
```

### 3.16 `reports`

```sql
CREATE TABLE reports (
  id           BIGSERIAL    PRIMARY KEY,
  reporter_uid BIGINT       NOT NULL REFERENCES users(uid),
  target_kind  TEXT         NOT NULL CHECK (target_kind IN ('user','message','room')),
  target_id    TEXT         NOT NULL,        -- UID 或 UUID 字符串
  reason       TEXT         NOT NULL,
  detail       TEXT,
  status       TEXT         NOT NULL DEFAULT 'open'
               CHECK (status IN ('open','reviewing','resolved','dismissed')),
  created_at   timestamptz  NOT NULL DEFAULT now(),
  resolved_at  timestamptz,
  moderator_uid BIGINT      REFERENCES users(uid)
);
```

---

## 4. Redis 键规划

```
presence:user:<uid>             HASH  { status, last_ping, current_room }, TTL 90s
presence:all                    SET   所有在线用户 uid
presence:friends:<uid>          SET   该用户的好友在线 uid 子集（推送优化）

room:<room_id>                  HASH  { host_uid, state, player_count, ... }
room:<room_id>:members          SET   所有成员 uid
room:<room_id>:spectators       SET

lobby:online                    SET   正在大厅订阅消息的 uid（用于 fanout）
lobby:stream                    LIST  最近 100 条公屏消息（缓存，持久化在 PG）

ratelimit:ip:<ip>:<window>      STRING token bucket
ratelimit:user:<uid>:<window>   STRING

revocation:jti:<jti>            STRING 被 revoke 的 JWT，TTL = token 剩余寿命

match_queue:<game_id>:<mode>    LIST  V1 不用（没匹配），V2 再加
```

---

## 5. 房间生命周期状态机

```
             ┌──────────────┐
             │    lobby     │ (房间刚创建 / 对局结束回到这)
             └──┬─────┬─────┘
                │     │
    所有人离开    │     │  房主开始游戏
     last_active│     │
     > 30min    │     │
                │     ▼
                │  ┌───────────┐       pause       ┌───────────┐
                │  │  playing  │ ───────────────▶  │  paused   │
                │  └──┬────────┘                   └─────┬─────┘
                │     │                                  │
                │     │ 游戏结束                          │ 恢复
                │     ▼                                  │
                │  回 lobby ◀────────────────────────────┘
                │
                ▼
             ┌──────────────┐
             │   closed     │
             └──────────────┘
```

触发器：
- `last_active_at < now() - 30min AND state = 'lobby' AND player_count = 0` → 后台 job 标记 closed
- 关闭后，同名短码可以被复用（`code` 唯一性仅约束 active 房间 — 用 partial UNIQUE index）

### 配置脏位

- 房主改设置 → `config_dirty = true`
- 所有人离开（player_count = 0）且 dirty → 下次有人进房自动 reset 回默认（空 config）
- 所有人离开但 clean → 下次进房保留上次配置

---

## 6. 数据保留策略

| 表 | 保留策略 |
|---|---|
| `lobby_messages` | 永久（用户要求）— V2 按月分区后方便归档 |
| `dm_messages` | 永久 |
| `group_messages` | 永久 |
| `match_history` | 永久 |
| `notifications` | 已读的 60 天后删除，未读永久 |
| `user_sessions` | revoked 或 expired 30 天后删除 |
| `room_invitations` | 过期 7 天后删除 |

后台 job（V1 用 pg_cron 或手写 goroutine）每天跑一次清理。

---

## 7. 性能预估（5000 MAU）

- users 表：~5000 行 × 20 字段 ≈ 2MB，全在内存
- lobby_messages：50条/人/天 × 5000 × 365 = 9100 万行/年 × ~500B ≈ 45GB/年
  → 3 年内一台 200GB SSD 的 VPS 吃得下
- group_messages：假设 1/3 用户在群里活跃，量级同上
- match_history：50 场/人/天 × 5000 × 365 = 9100 万行/年 × 200B ≈ 18GB/年

**需要分区的时机**：lobby_messages 到 3GB 左右（约 600 万行）开始感受到维护痛，那时候做按月分区。

---

*Last updated: 2026-04-25.*
