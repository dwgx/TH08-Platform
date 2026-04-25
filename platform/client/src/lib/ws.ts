// WebSocket client with auto-reconnect + topic subscription + typed
// event dispatch. Wired to auth-store so re-login reconnects.
//
// Usage pattern in components:
//
//   const off = ws.on("friends.status", (payload) => ...);
//   useEffect(() => off, [off]);
//
// Topics (from API spec §15) you'd subscribe to:
//   - lobby                (always, when on /lobby route)
//   - room:<uuid>          (when inside a room)
//   - channel:<uuid>       (when inside a group channel)
//   - dm:<thread_id>       (when inside a DM)

import { useAuthStore } from "@/lib/auth-store";
import { api } from "@/lib/api";

type Handler = (payload: any) => void;

class WSClient {
  private sock: WebSocket | null = null;
  private url: string;
  private handlers = new Map<string, Set<Handler>>();
  private subscribed = new Set<string>();
  private reconnectDelay = 1_000;
  private closed = false;

  constructor() {
    const base = api.baseURL().replace(/^http/, "ws");
    this.url = `${base}/api/v1/ws`;
  }

  connect() {
    const token = useAuthStore.getState().accessToken;
    if (!token) return;
    this.closed = false;
    const u = new URL(this.url);
    u.searchParams.set("token", token);
    const sock = new WebSocket(u.toString());
    this.sock = sock;

    sock.onopen = () => {
      this.reconnectDelay = 1_000;
      // resubscribe
      if (this.subscribed.size > 0) {
        this.send({ type: "subscribe", topics: Array.from(this.subscribed) });
      }
    };

    sock.onmessage = (ev) => {
      try {
        const msg = JSON.parse(ev.data);
        this.dispatch(msg.type, msg);
      } catch {}
    };

    sock.onclose = () => {
      this.sock = null;
      if (!this.closed) {
        setTimeout(() => this.connect(), this.reconnectDelay);
        this.reconnectDelay = Math.min(this.reconnectDelay * 2, 30_000);
      }
    };

    sock.onerror = () => { sock.close(); };

    // heartbeat
    const heartbeat = setInterval(() => {
      if (this.sock?.readyState === WebSocket.OPEN) {
        this.send({ type: "ping" });
      } else {
        clearInterval(heartbeat);
      }
    }, 30_000);
  }

  close() {
    this.closed = true;
    this.sock?.close();
  }

  subscribe(...topics: string[]) {
    topics.forEach((t) => this.subscribed.add(t));
    if (this.sock?.readyState === WebSocket.OPEN) {
      this.send({ type: "subscribe", topics });
    }
  }

  unsubscribe(...topics: string[]) {
    topics.forEach((t) => this.subscribed.delete(t));
    if (this.sock?.readyState === WebSocket.OPEN) {
      this.send({ type: "unsubscribe", topics });
    }
  }

  on(type: string, h: Handler): () => void {
    if (!this.handlers.has(type)) this.handlers.set(type, new Set());
    this.handlers.get(type)!.add(h);
    return () => this.handlers.get(type)?.delete(h);
  }

  private dispatch(type: string, payload: any) {
    this.handlers.get(type)?.forEach((h) => {
      try { h(payload); } catch (err) { console.error("ws handler", err); }
    });
  }

  private send(msg: unknown) {
    if (this.sock?.readyState === WebSocket.OPEN) {
      this.sock.send(JSON.stringify(msg));
    }
  }
}

export const ws = new WSClient();
