// Real Lobby page. Three columns:
//   LEFT   server/game category list (static filter buttons + room counts)
//   CENTER flex: active rooms card list
//   RIGHT  lobby public chat with live WS updates + composer
//
// Data sources:
//   GET  /api/v1/rooms          — initial room list, plus refetch on WS events
//   GET  /api/v1/lobby/messages — initial chat history
//   POST /api/v1/lobby/messages — send chat
//   WS   topic "lobby"          — lobby.message event pushes new chat in real-time
import { useEffect, useRef, useState } from "react";
import { useQuery, useQueryClient } from "@tanstack/react-query";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import {
  Send, Users, Gamepad2, Lock, CheckCircle2, CircleDot, Plus, Hash,
} from "lucide-react";

import { api } from "@/lib/api";
import { ws } from "@/lib/ws";
import { useAuthStore } from "@/lib/auth-store";
import { cn, relativeTime } from "@/lib/utils";

type Room = {
  id: string; code: string; name: string; kind: string; visibility: string;
  join_mode: string; state: string; region: string | null; game_id: string;
  host: { uid: string; username: string; handle: string; country?: string };
  player_count: number; max_players: number;
  spectator_count: number; max_spectators: number;
  has_password: boolean;
  created_at: string; last_active_at: string;
};
type LobbyMsg = {
  id: number;
  sender: { uid: string; username: string; handle: string };
  content: string; kind: string; created_at: string;
};

export default function LobbyPage() {
  const { t } = useTranslation();
  const qc = useQueryClient();
  const me = useAuthStore((s) => s.user);

  // ── rooms ────────────────────────────────────────────────────
  const roomsQuery = useQuery({
    queryKey: ["rooms"],
    queryFn: () => api.get<{ items: Room[] }>("/api/v1/rooms?limit=50"),
    refetchInterval: 15_000,
  });
  const rooms = roomsQuery.data?.items ?? [];

  // ── chat ─────────────────────────────────────────────────────
  const chatQuery = useQuery({
    queryKey: ["lobby", "messages"],
    queryFn: () => api.get<{ items: LobbyMsg[] }>("/api/v1/lobby/messages?limit=80"),
  });
  const messages = chatQuery.data?.items ?? [];

  // Subscribe to lobby topic on mount; append new messages live.
  useEffect(() => {
    ws.subscribe("lobby");
    const off = ws.on("lobby.message", (payload: { message: LobbyMsg }) => {
      qc.setQueryData<{ items: LobbyMsg[] }>(["lobby", "messages"], (prev) => ({
        items: [...(prev?.items ?? []), payload.message].slice(-200),
      }));
    });
    return () => { off(); ws.unsubscribe("lobby"); };
  }, [qc]);

  // Auto-scroll chat to bottom on new messages.
  const scrollRef = useRef<HTMLDivElement | null>(null);
  useEffect(() => {
    const el = scrollRef.current;
    if (el) el.scrollTop = el.scrollHeight;
  }, [messages.length]);

  // Composer
  const [draft, setDraft] = useState("");
  const [sending, setSending] = useState(false);
  async function send() {
    const content = draft.trim();
    if (!content) return;
    setSending(true);
    try {
      // Optimistic add (WS will also deliver; dedupe by id).
      await api.post<LobbyMsg>("/api/v1/lobby/messages", { content, kind: "text" });
      setDraft("");
    } catch (e: any) {
      toast.error(e?.message || t("errors.server"));
    } finally {
      setSending(false);
    }
  }

  const filteredRoomsByKind = (kind: string) =>
    rooms.filter((r) => r.kind === kind);

  return (
    <div className="flex h-full">
      {/* LEFT — categories + counts */}
      <aside className="flex h-full w-[260px] shrink-0 flex-col border-r border-border bg-surface">
        <div className="flex items-center justify-between px-4 py-3 border-b border-border">
          <div className="text-[13px] font-semibold">{t("lobby.title")}</div>
          <button title={t("lobby.create_room")}
            className="flex h-7 w-7 items-center justify-center rounded-md bg-brand/15 text-brand hover:bg-brand/25 transition-all">
            <Plus size={14} />
          </button>
        </div>
        <div className="th-scroll flex-1 overflow-y-auto px-2 py-3">
          <Section title={t("lobby.servers_official")} rooms={filteredRoomsByKind("official")} />
          <Section title={t("lobby.servers_personal")} rooms={filteredRoomsByKind("personal")} />
          <Section title={t("lobby.servers_community")} rooms={filteredRoomsByKind("community")} />
        </div>
        <div className="border-t border-border px-4 py-3 text-[11px] text-fg-subtle">
          {rooms.length} 个房间 · {rooms.reduce((a, r) => a + r.player_count, 0)} 玩家
        </div>
      </aside>

      {/* CENTER — room cards */}
      <section className="flex flex-1 flex-col min-w-0">
        <header className="flex items-center justify-between border-b border-border bg-surface px-6 py-4">
          <div>
            <h1 className="text-[20px] font-semibold">活跃房间</h1>
            <div className="text-[12px] text-fg-muted">
              {roomsQuery.isLoading ? "加载中…" : `共 ${rooms.length} 个房间，${rooms.filter(r => r.state === "playing").length} 场进行中`}
            </div>
          </div>
          <div className="flex items-center gap-2">
            <button className="flex h-8 items-center gap-2 rounded-md bg-brand px-3 text-[12px] font-medium text-white hover:brightness-110">
              <Plus size={14} /> {t("lobby.create_room")}
            </button>
          </div>
        </header>
        <div className="th-scroll flex-1 overflow-auto p-6">
          {rooms.length === 0 && !roomsQuery.isLoading && (
            <div className="rounded-xl border border-dashed border-border bg-surface-raised/40 p-16 text-center">
              <div className="mx-auto mb-4 h-12 w-12 rounded-full border-2 border-brand/40" />
              <div className="text-[14px] text-fg-muted">{t("lobby.empty_rooms")}</div>
            </div>
          )}
          <div className="grid grid-cols-1 gap-3 lg:grid-cols-2 xl:grid-cols-3">
            {rooms.map((r) => (
              <RoomCard key={r.id} room={r} />
            ))}
          </div>
        </div>
      </section>

      {/* RIGHT — lobby chat */}
      <aside className="flex h-full w-[340px] shrink-0 flex-col border-l border-border bg-surface">
        <header className="flex items-center justify-between border-b border-border px-4 py-3">
          <div className="flex items-center gap-2 text-[13px] font-semibold">
            <Hash size={14} strokeWidth={1.75} className="text-fg-muted" />
            {t("lobby.public_chat")}
          </div>
          <div className="text-[11px] text-fg-subtle">{messages.length} 条</div>
        </header>
        <div ref={scrollRef} className="th-scroll flex-1 overflow-y-auto px-2 py-2">
          {chatQuery.isLoading ? (
            <div className="p-6 text-center text-[12px] text-fg-subtle">载入中…</div>
          ) : messages.length === 0 ? (
            <div className="p-6 text-center text-[12px] text-fg-subtle">大厅现在很安静…</div>
          ) : (
            messages.map((m, i) => {
              const prev = messages[i - 1];
              const grouped = prev && prev.sender.uid === m.sender.uid
                && Date.parse(m.created_at) - Date.parse(prev.created_at) < 5 * 60_000;
              return <ChatMsg key={m.id} msg={m} grouped={!!grouped} />;
            })
          )}
        </div>
        <div className="border-t border-border p-2">
          <form onSubmit={(e) => { e.preventDefault(); send(); }} className="flex items-end gap-2">
            <textarea
              value={draft}
              onChange={(e) => setDraft(e.target.value)}
              placeholder={me ? "在大厅说点什么…" : "请先登录"}
              rows={1}
              onKeyDown={(e) => {
                if (e.key === "Enter" && !e.shiftKey) { e.preventDefault(); send(); }
              }}
              disabled={!me || sending}
              className="flex-1 min-h-[36px] max-h-[120px] resize-none rounded-md border border-border bg-surface-raised px-3 py-2 text-[13px] outline-none placeholder:text-fg-subtle focus:border-brand focus:ring-4 focus:ring-brand/20 disabled:opacity-50"
            />
            <button
              type="submit" disabled={!draft.trim() || sending || !me}
              className="flex h-9 w-9 shrink-0 items-center justify-center rounded-md bg-brand text-white transition-all hover:brightness-110 disabled:opacity-40 disabled:cursor-not-allowed"
              title="发送（Enter）"
            >
              <Send size={14} />
            </button>
          </form>
          <div className="mt-1.5 text-[10px] text-fg-subtle px-1">
            Enter 发送 · Shift+Enter 换行
          </div>
        </div>
      </aside>
    </div>
  );
}

function Section({ title, rooms }: { title: string; rooms: Room[] }) {
  return (
    <div className="mb-3">
      <div className="flex items-center justify-between px-2 pb-1 text-[10px] font-semibold uppercase tracking-wider text-fg-subtle">
        <span>{title}</span>
        <span className="font-mono">{rooms.length}</span>
      </div>
      <div className="flex flex-col gap-0.5">
        {rooms.map((r) => (
          <button key={r.id} className="group flex items-center gap-2 rounded-md px-2 py-1.5 text-left transition-all hover:bg-surface-hover">
            <Gamepad2 size={14} className="shrink-0 text-fg-muted group-hover:text-fg" />
            <div className="flex-1 min-w-0">
              <div className="truncate text-[13px] text-fg">{r.name}</div>
              <div className="truncate text-[11px] text-fg-subtle">
                {r.host.username} · {r.player_count}/{r.max_players}
              </div>
            </div>
            {r.state === "playing" && <CircleDot size={12} className="text-brand" />}
          </button>
        ))}
      </div>
    </div>
  );
}

function RoomCard({ room }: { room: Room }) {
  const fam = room.game_id.split("-")[0].toUpperCase();
  const statePill = {
    lobby:   { label: "准备中",   cls: "bg-surface-bright text-fg-muted" },
    playing: { label: "进行中",   cls: "bg-brand/15 text-brand" },
    paused:  { label: "已暂停",   cls: "bg-warning/15 text-warning" },
  }[room.state] ?? { label: room.state, cls: "bg-surface-bright text-fg-muted" };
  const joinModePill = {
    open:   { label: "自由加入",  cls: "bg-success/15 text-success" },
    ask:    { label: "需要审批",  cls: "bg-warning/15 text-warning" },
    closed: { label: "锁定",     cls: "bg-danger/15 text-danger" },
  }[room.join_mode] ?? { label: room.join_mode, cls: "bg-surface-bright text-fg-muted" };

  return (
    <div className="th-card group flex flex-col gap-3 p-4">
      <div className="flex items-start gap-3">
        <div className="flex h-12 w-12 shrink-0 items-center justify-center rounded-md bg-gradient-to-br from-brand/40 to-brand-accent/30 font-mono text-[13px] font-bold text-white">
          {fam}
        </div>
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2">
            <h3 className="flex-1 truncate text-[14px] font-semibold">{room.name}</h3>
            {room.has_password && <Lock size={12} className="text-fg-muted" />}
          </div>
          <div className="mt-1 truncate text-[12px] text-fg-muted">
            <span className="font-mono text-fg-subtle">{room.code}</span>
            <span className="mx-1.5">·</span>
            <span>{room.host.username}</span>
            <span className="text-fg-subtle"> @{room.host.handle}</span>
          </div>
        </div>
      </div>
      <div className="flex flex-wrap items-center gap-1.5">
        <span className={cn("rounded px-2 py-0.5 text-[10px] font-semibold uppercase tracking-wider", statePill.cls)}>{statePill.label}</span>
        <span className={cn("rounded px-2 py-0.5 text-[10px] font-semibold uppercase tracking-wider", joinModePill.cls)}>{joinModePill.label}</span>
        {room.region && (
          <span className="rounded px-2 py-0.5 text-[10px] font-semibold uppercase tracking-wider bg-surface-bright text-fg-muted">
            {room.region}
          </span>
        )}
        <div className="flex-1" />
        <div className="flex items-center gap-1 text-[11px] text-fg-muted">
          <Users size={11} />
          {room.player_count}/{room.max_players}
          {room.spectator_count > 0 && (
            <span className="text-fg-subtle"> · 观战 {room.spectator_count}</span>
          )}
        </div>
      </div>
      <div className="flex items-center justify-between border-t border-border pt-3">
        <div className="text-[11px] text-fg-subtle">{relativeTime(room.last_active_at)}</div>
        <button className="flex items-center gap-1.5 rounded-md bg-brand/15 px-3 py-1 text-[12px] font-medium text-brand opacity-0 transition-all group-hover:opacity-100 hover:bg-brand/25">
          <CheckCircle2 size={12} /> {room.state === "playing" ? "观战" : "加入"}
        </button>
      </div>
    </div>
  );
}

function ChatMsg({ msg, grouped }: { msg: LobbyMsg; grouped: boolean }) {
  const isSystem = msg.kind === "system";
  const isInvite = msg.kind === "room_invite";
  if (isSystem) {
    return (
      <div className="px-3 py-1 text-center text-[11px] italic text-fg-subtle">— {msg.content} —</div>
    );
  }
  return (
    <div className={cn("rounded-md px-2 py-1 transition-all hover:bg-surface-hover/60", grouped && "-mt-0.5")}>
      {!grouped && (
        <div className="flex items-baseline gap-2">
          <span className="text-[13px] font-semibold text-fg">{msg.sender.username}</span>
          <span className="font-mono text-[11px] text-fg-subtle">#{msg.sender.uid}</span>
          <span className="ml-auto text-[11px] text-fg-subtle">{relativeTime(msg.created_at)}</span>
        </div>
      )}
      <div className={cn(
        "text-[13px] leading-relaxed",
        isInvite ? "text-brand-accent" : "text-fg",
      )}>
        {isInvite && <Gamepad2 size={12} className="inline mr-1 -translate-y-[1px]" />}
        {msg.content}
      </div>
    </div>
  );
}
