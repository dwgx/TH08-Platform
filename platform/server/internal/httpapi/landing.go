package httpapi

import (
	"net/http"
	"strings"
)

// Landing serves a single self-contained HTML page at "/". It fetches
// /demo/snapshot via JS and renders a live view of the platform state.
// Strictly a deploy preview — production should ship a real SPA.
func Landing(w http.ResponseWriter, r *http.Request) {
	if !(r.URL.Path == "/" || r.URL.Path == "") {
		http.NotFound(w, r); return
	}
	w.Header().Set("Content-Type", "text/html; charset=utf-8")
	w.Header().Set("Cache-Control", "no-store")
	_, _ = w.Write([]byte(landingHTML))
}

// Compile-time check — keeps editor happy on typos in landingHTML.
var _ = strings.HasPrefix

const landingHTML = `<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8" />
<title>TH-Platform · Live Preview</title>
<meta name="viewport" content="width=device-width, initial-scale=1" />
<style>
  :root {
    --canvas: hsl(232 15% 9%);
    --surface: hsl(232 14% 12%);
    --raised: hsl(232 13% 15%);
    --bright: hsl(232 12% 19%);
    --border: hsl(232 12% 18%);
    --border-s: hsl(232 10% 26%);
    --fg: hsl(220 15% 92%);
    --muted: hsl(220 10% 65%);
    --subtle: hsl(220 10% 48%);
    --brand: hsl(258 86% 66%);
    --accent: hsl(340 82% 68%);
    --success: hsl(142 72% 50%);
    --warning: hsl(38 92% 58%);
    --danger: hsl(0 72% 58%);
    --info: hsl(210 90% 60%);
    --reimu: hsl(350 75% 58%);
    --mono: 'JetBrains Mono', ui-monospace, Consolas, Menlo, monospace;
    --sans: 'Inter', 'Noto Sans SC', 'Noto Sans JP', system-ui, sans-serif;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  html, body { height: 100%; }
  body {
    background: radial-gradient(1200px 600px at 30% -10%, hsl(258 86% 66% / 0.10), transparent 60%),
                radial-gradient(1000px 500px at 100% 100%, hsl(340 82% 68% / 0.08), transparent 60%),
                var(--canvas);
    color: var(--fg);
    font-family: var(--sans);
    -webkit-font-smoothing: antialiased;
    line-height: 1.5;
    min-height: 100%;
  }
  a { color: var(--brand); text-decoration: none; }
  a:hover { text-decoration: underline; }
  .wrap { max-width: 1280px; margin: 0 auto; padding: 32px 24px 56px; }
  header {
    display: flex; align-items: center; gap: 16px;
    padding: 20px 24px; margin-bottom: 24px;
    border: 1px solid var(--border);
    border-radius: 14px;
    background: linear-gradient(180deg, var(--bright), var(--raised));
  }
  .logo {
    width: 44px; height: 44px;
    border-radius: 50%;
    background: conic-gradient(from 90deg, var(--brand), var(--accent), var(--brand));
    box-shadow: inset 0 0 0 2px var(--canvas);
    flex-shrink: 0;
  }
  h1 { font-size: 22px; font-weight: 700; letter-spacing: 0.02em; }
  .tag { font-size: 12px; color: var(--muted); }
  .status-row { margin-left: auto; display: flex; gap: 16px; align-items: center; font-size: 12px; color: var(--muted); }
  .status-row .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--success); box-shadow: 0 0 10px var(--success); display: inline-block; margin-right: 6px; }

  .grid {
    display: grid;
    grid-template-columns: 1.4fr 1fr;
    gap: 24px;
  }
  @media (max-width: 940px) { .grid { grid-template-columns: 1fr; } }

  .panel {
    background: var(--surface);
    border: 1px solid var(--border);
    border-radius: 14px;
    overflow: hidden;
  }
  .panel header {
    margin: 0; border: 0; border-bottom: 1px solid var(--border);
    border-radius: 0;
    padding: 14px 18px;
    background: var(--raised);
    font-size: 13px; font-weight: 600; color: var(--fg);
    letter-spacing: 0.04em; text-transform: uppercase;
  }

  .stats {
    display: grid; grid-template-columns: repeat(4, 1fr); gap: 1px;
    background: var(--border);
  }
  @media (max-width: 720px) { .stats { grid-template-columns: repeat(2, 1fr); } }
  .stat {
    background: var(--surface); padding: 18px 20px;
  }
  .stat .label { font-size: 11px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.08em; }
  .stat .value { font-size: 26px; font-weight: 700; margin-top: 4px; font-family: var(--mono); }
  .stat .sub { font-size: 11px; color: var(--subtle); margin-top: 2px; }

  ul.rooms { list-style: none; padding: 4px; }
  ul.rooms li {
    display: grid;
    grid-template-columns: 56px 1fr auto;
    gap: 14px; align-items: center;
    padding: 12px;
    border-radius: 10px;
    border: 1px solid transparent;
    transition: background 120ms, border-color 120ms;
  }
  ul.rooms li:hover { background: var(--bright); border-color: var(--border-s); }
  .cover {
    width: 56px; height: 56px;
    border-radius: 8px;
    display: grid; place-items: center;
    font-family: var(--mono); font-weight: 700; font-size: 13px;
    background: linear-gradient(135deg, hsl(258 60% 32%), hsl(340 60% 32%));
    color: hsl(0 0% 100%);
    letter-spacing: 0.04em;
  }
  .room-name { font-size: 14px; font-weight: 600; }
  .room-meta { font-size: 12px; color: var(--muted); margin-top: 2px; display: flex; gap: 8px; flex-wrap: wrap; }
  .pill {
    display: inline-flex; align-items: center; gap: 4px;
    height: 18px; padding: 0 8px;
    border-radius: 999px;
    font-size: 10px; font-weight: 600; letter-spacing: 0.04em;
    border: 1px solid var(--border-s);
    color: var(--muted);
    background: var(--bright);
    text-transform: uppercase;
  }
  .pill.brand { background: hsl(258 86% 66% / 0.12); border-color: hsl(258 86% 66% / 0.4); color: hsl(258 86% 80%); }
  .pill.success { background: hsl(142 72% 50% / 0.12); border-color: hsl(142 72% 50% / 0.4); color: hsl(142 72% 70%); }
  .pill.warning { background: hsl(38 92% 58% / 0.12); border-color: hsl(38 92% 58% / 0.4); color: hsl(38 92% 75%); }
  .pill.danger { background: hsl(0 72% 58% / 0.12); border-color: hsl(0 72% 58% / 0.4); color: hsl(0 72% 75%); }

  .right-side { font-size: 12px; color: var(--muted); text-align: right; }
  .right-side .cap { font-family: var(--mono); font-size: 13px; color: var(--fg); }

  .messages { padding: 8px 12px; max-height: 480px; overflow-y: auto; }
  .messages::-webkit-scrollbar { width: 8px; }
  .messages::-webkit-scrollbar-thumb { background: hsl(232 10% 26%); border-radius: 4px; }
  .msg { padding: 6px 10px; border-radius: 8px; }
  .msg + .msg { margin-top: 2px; }
  .msg .head { display: flex; align-items: baseline; gap: 8px; }
  .msg .name { font-size: 13px; font-weight: 600; color: var(--fg); }
  .msg .uid { font-family: var(--mono); font-size: 11px; color: var(--subtle); }
  .msg .time { margin-left: auto; font-size: 11px; color: var(--subtle); }
  .msg .body { font-size: 13px; color: var(--muted); margin-top: 2px; }
  .msg.system .body { color: var(--brand); font-style: italic; }
  .msg.invite .body { color: var(--accent); }

  .users-grid {
    display: grid; grid-template-columns: repeat(auto-fill, minmax(140px, 1fr)); gap: 8px;
    padding: 14px;
  }
  .user-chip {
    background: var(--bright);
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 10px 12px;
    font-size: 12px;
  }
  .user-chip .name { font-weight: 600; color: var(--fg); }
  .user-chip .h { font-family: var(--mono); color: var(--muted); font-size: 11px; }

  footer {
    margin-top: 32px; padding: 16px;
    text-align: center; font-size: 11px; color: var(--subtle);
    border-top: 1px solid var(--border);
  }
  code, .mono { font-family: var(--mono); }

  .empty { padding: 32px; text-align: center; color: var(--subtle); font-size: 13px; }

  /* Tab strip */
  .games {
    display: flex; gap: 6px; padding: 12px;
    border-bottom: 1px solid var(--border);
    overflow-x: auto;
  }
  .games .game {
    border: 1px solid var(--border-s);
    background: var(--bright);
    color: var(--muted);
    border-radius: 8px;
    padding: 6px 10px;
    font-size: 12px;
    white-space: nowrap;
  }
  .games .game.alpha { border-color: hsl(38 92% 58% / 0.5); color: hsl(38 92% 75%); }
  .games .game.active { border-color: hsl(142 72% 50% / 0.5); color: hsl(142 72% 70%); }
</style>
</head>
<body>
  <div class="wrap">
    <header>
      <div class="logo"></div>
      <div>
        <h1>TH-Platform <span class="tag">v0.1.0-alpha · Live Preview</span></h1>
        <div class="tag">东方系列联机平台 · Touhou Multiplayer · 多人匹配</div>
      </div>
      <div class="status-row">
        <span><span class="dot"></span><span id="status">Connecting…</span></span>
        <span class="mono" id="server-time">—</span>
      </div>
    </header>

    <div class="panel" style="margin-bottom: 24px;">
      <div class="stats" id="stats">
        <div class="stat"><div class="label">用户 Users</div><div class="value">—</div></div>
        <div class="stat"><div class="label">活跃房间 Rooms</div><div class="value">—</div></div>
        <div class="stat"><div class="label">公屏消息 Lobby msgs</div><div class="value">—</div></div>
        <div class="stat"><div class="label">群组 Groups</div><div class="value">—</div></div>
      </div>
    </div>

    <div class="panel" style="margin-bottom: 24px;">
      <header>已注册游戏 Games</header>
      <div class="games" id="games"></div>
    </div>

    <div class="grid">
      <div>
        <div class="panel" style="margin-bottom: 24px;">
          <header>大厅房间 Active Rooms</header>
          <ul class="rooms" id="rooms"><li class="empty">Loading…</li></ul>
        </div>
        <div class="panel">
          <header>最近用户 Recent Users</header>
          <div class="users-grid" id="users"></div>
        </div>
      </div>

      <div>
        <div class="panel">
          <header>大厅聊天 Lobby Chat</header>
          <div class="messages" id="messages"><div class="empty">Loading…</div></div>
        </div>
      </div>
    </div>

    <footer>
      Powered by Go · Postgres · Redis · WebSocket. Source: this VPS.
      &nbsp;·&nbsp; <code>GET /api/v1/games</code> · <code>GET /demo/snapshot</code> · <code>GET /healthz</code>
    </footer>
  </div>

<script>
function fmt(n) { return new Intl.NumberFormat().format(n); }
function escape(s) { return String(s ?? '').replace(/[&<>"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c])); }
function rel(iso) {
  const t = new Date(iso); const s = (Date.now() - t.getTime()) / 1000;
  if (s < 60)   return Math.max(1, s|0) + 's ago';
  if (s < 3600) return (s/60|0) + 'm ago';
  if (s < 86400) return (s/3600|0) + 'h ago';
  return (s/86400|0) + 'd ago';
}
function joinModePill(m) {
  return m === 'open' ? '<span class="pill success">Open</span>'
       : m === 'ask'  ? '<span class="pill warning">Ask</span>'
       :                '<span class="pill danger">Closed</span>';
}
function statePill(s) {
  return s === 'playing' ? '<span class="pill brand">Playing</span>'
       : s === 'paused'  ? '<span class="pill warning">Paused</span>'
       :                   '<span class="pill">Lobby</span>';
}
function gameMonogram(gameId) {
  const fam = (gameId || '').split('-')[0].toUpperCase();
  return fam || 'TH';
}
function render(snap) {
  document.getElementById('status').textContent = 'Online · ' + snap.stats.version;
  document.getElementById('server-time').textContent = 'started ' + new Date(snap.stats.server_start_utc).toLocaleString();
  document.getElementById('stats').innerHTML = [
    ['用户 Users',           fmt(snap.stats.users)],
    ['活跃房间 Rooms',       fmt(snap.stats.active_rooms)],
    ['公屏消息 Lobby msgs',  fmt(snap.stats.lobby_messages)],
    ['群组 Groups',          fmt(snap.stats.groups)],
  ].map(([l, v]) =>
    '<div class="stat"><div class="label">' + l + '</div><div class="value">' + v + '</div></div>'
  ).join('');

  document.getElementById('games').innerHTML = (snap.games || []).map(g =>
    '<div class="game ' + escape(g.status) + '">' + escape(g.display_name) + ' · <span class="mono">' + escape(g.id) + '</span></div>'
  ).join('') || '<div class="empty">No games registered.</div>';

  const rooms = snap.rooms || [];
  document.getElementById('rooms').innerHTML = rooms.length ? rooms.map(r => {
    const cap = r.player_count + ' / ' + r.max_players;
    const spec = r.spectator_count > 0 ? '<span class="pill">观战 ' + r.spectator_count + '</span>' : '';
    const visible = r.visibility === 'private' ? '<span class="pill danger">Private</span>' : '<span class="pill">Public</span>';
    return '<li>' +
      '<div class="cover">' + gameMonogram(r.game_id) + '</div>' +
      '<div>' +
        '<div class="room-name">' + escape(r.name) + '</div>' +
        '<div class="room-meta">' +
          '<span class="mono" style="color:var(--subtle)">' + escape(r.code) + '</span>' +
          '<span>·</span>' +
          '<span>' + escape(r.host.username) + ' <span class="uid mono">@' + escape(r.host.handle) + '</span></span>' +
          '<span>·</span>' +
          '<span>' + escape(r.region || '—') + '</span>' +
        '</div>' +
        '<div style="margin-top:6px;display:flex;gap:6px;flex-wrap:wrap;">' +
          statePill(r.state) +
          joinModePill(r.join_mode) +
          visible +
          spec +
        '</div>' +
      '</div>' +
      '<div class="right-side"><div class="cap">' + cap + '</div><div>' + rel(r.last_active_at) + '</div></div>' +
    '</li>';
  }).join('') : '<li class="empty">No active rooms. Be the first to host!</li>';

  document.getElementById('users').innerHTML = (snap.online_samples || []).map(u =>
    '<div class="user-chip"><div class="name">' + escape(u.username) + '</div>' +
    '<div class="h">@' + escape(u.handle) + (u.country ? ' · ' + escape(u.country) : '') + '</div></div>'
  ).join('') || '<div class="empty">No users yet.</div>';

  const msgs = snap.lobby_messages || [];
  const messagesEl = document.getElementById('messages');
  messagesEl.innerHTML = msgs.length ? msgs.map(m => {
    const cls = m.kind === 'system' ? 'system' : (m.kind === 'room_invite' ? 'invite' : '');
    return '<div class="msg ' + cls + '">' +
      '<div class="head">' +
        '<span class="name">' + escape(m.sender.username) + '</span>' +
        '<span class="uid">#' + escape(m.sender.uid) + '</span>' +
        '<span class="time">' + rel(m.created_at) + '</span>' +
      '</div>' +
      '<div class="body">' + escape(m.content) + '</div>' +
    '</div>';
  }).join('') : '<div class="empty">Lobby is quiet.</div>';
  messagesEl.scrollTop = messagesEl.scrollHeight;
}

async function refresh() {
  try {
    const res = await fetch('/demo/snapshot', { cache: 'no-store' });
    if (!res.ok) throw new Error('snapshot ' + res.status);
    render(await res.json());
  } catch (e) {
    document.getElementById('status').textContent = 'Connection error';
    console.error(e);
  }
}
refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>`
