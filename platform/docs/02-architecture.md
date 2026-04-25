# Architecture — TH-Platform

> 全系统拓扑 + 服务边界 + 数据流。修改架构前请先改这份。

---

## 0. 目标再述

- 桌面 launcher 客户端（Tauri 2）
- 3 类服务器（官方 / 个人 / 社区）的统一匹配和房间生命周期
- 5000+ MAU 可撑（单 VPS 起步，可横向扩）
- 游戏帧同步走 UDP，不经过 Web 层（性能+成本）

---

## 1. 全景图

```
                            ┌─────────────────────────────┐
                            │      DNS / Cloudflare        │
                            │   th-platform.example.com    │
                            └──────┬──────────────┬────────┘
                                   │              │
                      (WSS/HTTPS)  │              │  (UDP, 为中继预留子域)
                                   │              │
                  ┌────────────────▼──┐      ┌────▼───────────────┐
                  │   Caddy (TLS)      │      │  Relay Edge (UDP)   │
                  │   反向代理          │      │  pion/turn + 自研   │
                  └─────────┬──────────┘      └─────────────────────┘
                            │                       ▲        ▲
          ┌─────────────────┼────────────┐          │        │
          │                 │            │          │        │
   ┌──────▼──────┐  ┌───────▼──────┐ ┌──▼────────┐  │        │
   │   Gateway   │  │  REST API    │ │  WS Hub   │  │        │
   │   /ws       │  │  /api/v1/*   │ │  (fanout) │  │        │
   └──────┬──────┘  └───────┬──────┘ └──┬────────┘  │        │
          │                 │            │          │        │
          └────────┬────────┴────────────┘          │        │
                   │                                │        │
           ┌───────▼─────┐                          │        │
           │   Business  │                          │        │
           │   Services  │                          │        │
           │ (auth/users/│                          │        │
           │  rooms/chat)│                          │        │
           └───┬─────────┘                          │        │
               │                                    │        │
     ┌─────────┼───────────┐                        │        │
     │         │           │                        │        │
┌────▼───┐ ┌───▼──┐ ┌──────▼────┐                   │        │
│Postgres│ │Redis │ │S3/MinIO   │                   │        │
│  16    │ │  7   │ │ (V2, 头像)│                   │        │
└────────┘ └──────┘ └───────────┘                   │        │
                                                     │        │
                          ┌──────────────────────────┴───┐    │
                          │   Tauri 桌面客户端             │    │
                          │   (React 19 + DLL injector)   │    │
                          └──────────────────┬────────────┘    │
                                             │                 │
                                       (UDP game traffic) ─────┘
                                             │
                                             ▼
                                    ┌─────────────────┐
                                    │ 其他玩家 Tauri    │
                                    │ (P2P / 中继)      │
                                    └─────────────────┘
```

---

## 2. 服务清单（monorepo 内切分，但各自是独立 binary）

### 2.1 `server/cmd/gateway`

- 职责：WebSocket hub + REST 路由
- 端口：内部 `:8080`，Caddy 代理对外 443
- 关键依赖：`chi`（REST），`nhooyr.io/websocket`（WS），`go-redis`，`pgx`
- 启动要求：PG 就绪、Redis 就绪

### 2.2 `server/cmd/migrate`

- 职责：跑 `server/migrations/*.sql`
- 一次性执行，不是常驻进程
- 幂等：基于 `schema_migrations` 表

### 2.3 `relay/`

- 职责：UDP 中继 + NAT 打洞协调
- 端口：UDP `:3478`（STUN）+ UDP `:49152-65535`（中继端口池）
- 用 `pion/turn` 起步，长期自研（TURN 协议开销大）
- **独立进程**：可以部署多台，客户端按延迟选

### 2.4 `client/`

- 桌面客户端（Tauri + React）
- 输出：`th-platform.exe`（单文件）+ `th-platform.msi`（安装器）

---

## 3. 数据流 — 玩家开一局的全流程

```
1. 玩家 A 打开客户端，登录（OAuth 或邮箱密码）
   → POST /api/v1/auth/login → JWT (access 15min + refresh 30day)
   → 前端保存到 secure storage (Tauri keyring plugin)

2. 玩家 A 升级 WebSocket 连接
   → GET /ws?token=<access>
   → 服务端把 user_id 加入 Redis in-memory presence set
   → 服务端推送 friends.online 事件给 A 的所有好友

3. 玩家 A 创建房间
   → POST /api/v1/rooms { game_version: "th08-1.00d", mode: "coop",
                          visibility: "public", join_mode: "open" }
   → 服务端：
     - 生成 room_id (UUIDv7)
     - 写 PG rooms 表
     - 在 Redis 建 room:<id> hash (实时状态)
     - 广播 ws "lobby.room.created" 给大厅订阅者
   → 返回 room_id + relay hints

4. 玩家 B 在大厅看到房间，点击"加入"
   → POST /api/v1/rooms/<id>/join
   → 服务端：
     - 检查 B 是否被 ban、是否好友（私密房要求）、是否满员
     - 更新 Redis room:<id>.members 集合
     - 推送 "room.member_joined" 给 A 和观战者
   → 返回 { room_info, relay_ticket }

5. A 和 B 都有 relay_ticket
   → 客户端打开 UDP socket
   → 各自向 relay.example.com:3478 发送 PING + ticket
   → Relay 互相告知 (ip, port) 尝试 P2P 打洞
   → 15s 内打洞失败 → fallback 中继转发

6. Tauri Rust 内核调用 DLL 注入模块
   → 启动 th08.exe + 注入 th08_platform.dll
   → DLL 通过 localhost pipe 向 Tauri 报告 frame tick
   → Tauri 通过 UDP socket 与对端交换帧

7. 对局结束 → DLL 通过 pipe 上报结果
   → Tauri POST /api/v1/rooms/<id>/match-result
   → 服务端写 match_history，更新玩家统计
   → 房主选择重开 / 退出 / 修改设置
```

---

## 4. 模块边界（为后期微服务化预留，V1 是单进程多包）

```
internal/
├── auth/          JWT + OAuth + 邮件验证
├── users/         用户 CRUD + UID 分配 + 头像
├── friends/       好友关系 + 私信（消息存 Postgres）
├── rooms/         房间生命周期 + 参数版本管理
├── channels/      群组频道 + 群聊
├── lobby/         大厅公屏 + 房间发现
├── presence/      在线状态（写 Redis）
├── notifications/ 通知中心
├── relay/         下发 relay ticket + 选最近中继
├── gameregistry/  游戏版本元数据（th06/th07/th08/th09 的参数）
├── ws/            WebSocket hub + 频道订阅 + fanout
├── httpapi/       HTTP handler 组装
├── config/        环境变量 + 配置文件加载
└── db/            连接池 + 迁移 + sqlc 生成代码
```

**规则**：
- 包之间只能单向依赖（业务 → db，业务 → config，不可逆）
- `httpapi` 和 `ws` 只做协议编解码，业务逻辑必须在对应 internal 包内
- 跨包调用走**接口**而不是具体实现（方便 V2 拆成 gRPC 服务）

---

## 5. 为什么选这个架构

| 选项 | 选 | 原因 |
|---|---|---|
| 单体 Go 进程 V1 | ✅ | 5000 MAU 绰绰有余，运维简单 |
| 直接微服务 K8s | ❌ | 5000 MAU 根本用不上，运维成本/月 > $200 |
| gRPC 内部通信 | ❌（V1） | 单进程直调函数更快 |
| GraphQL | ❌ | REST + WS 足够，GraphQL 的 overhead 不值 |
| MQ（NATS/Kafka） | ❌（V1） | Redis Pub/Sub 顶 5000 MAU 无压力 |
| ORM（GORM） | ❌ | sqlc 生成代码 + 手写 SQL，零运行时魔法 |
| 分布式 session | ❌ | JWT 无状态，Redis 只做 revocation list |

---

## 6. 部署拓扑

### V1（5000 MAU 以下，单 VPS 够）

```
1 台 VPS (4C8G 200GB SSD, 5Mbps) — 中国或香港（低延迟）
├── systemd: caddy.service            (TLS 反代)
├── systemd: th-gateway.service       (Go 主服务)
├── systemd: th-relay.service         (UDP 中继)
├── systemd: postgresql@16-main
└── systemd: redis-server

预估成本：阿里云 4C8G = ¥300/月，流量额外按量
```

### V2（5000-30000 MAU）

```
数据库和中继先分离，Gateway 横向两台
├── DB VPS   (8C16G, PG + Redis cluster)
├── Relay 节点 × 2（上海 + 东京）
├── App VPS × 2（后面接负载均衡）
└── 对象存储：阿里云 OSS 或 Cloudflare R2 (存头像/附件，V1 没用)
```

### V3（自家机房 / 多区域）

到时再说，不预先设计。

---

## 7. 安全底线

- 所有 API 强制 HTTPS（Caddy auto-cert）
- JWT 用 RS256，密钥放 `/etc/th-platform/keys/` 600 权限
- Password 用 argon2id，`memory=64MB, time=3, threads=4`
- Rate limit：IP 级 100req/min + 用户级 60req/min（Redis token bucket）
- CSRF：SameSite=Strict + 自定义 header
- SQL：sqlc 生成的 prepared statement，无拼接
- 所有用户输入走 zod（前端）+ server-side 再次校验
- 敏感操作（删除账号、改邮箱）需二次确认 + 邮件通知

---

## 8. 可观测性

V1 极简：
- 日志：`log/slog` 结构化 JSON，输出到 stdout，systemd-journald 自动收集
- 健康：`GET /healthz` + `GET /readyz`
- 指标：`GET /metrics`（Prometheus 格式），但 V1 不部署 Prometheus，先留接口

V2 加 Grafana + Loki 的日志聚合。

---

## 9. 扩展点（为后期留的门）

- 游戏版本 plugin：`gameregistry` 读 `games/*.yaml`，加新游戏不改代码
- 第三方登录 provider：`auth/oauth` 接口化，加 Steam / 网易 / Twitter 只写 adapter
- 中继策略：`relay.Picker` 接口，后期可以接 GeoIP / 加速卡
- 前端主题：CSS 变量全隔离，V2 做亮色主题改 globals.css 即可

---

*Last updated: 2026-04-25.*
