// Zustand store for auth state with dual persistence backend:
// - In Tauri: @tauri-apps/plugin-store (keychain-adjacent, survives
//   restarts, harder to leak via renderer XSS).
// - In a browser (pnpm dev preview, demo URL): localStorage fallback.
//
// The shim exposes the same minimal surface the rest of the store uses:
// get / set / delete / save. Keeps the rest of the code storage-agnostic.
import { create } from "zustand";
import { api, type UserProfile } from "@/lib/api";

type KVStore = {
  get<T>(key: string): Promise<T | null>;
  set(key: string, value: unknown): Promise<void>;
  delete(key: string): Promise<void>;
  save(): Promise<void>;
};

async function openTauriStore(): Promise<KVStore | null> {
  try {
    // Dynamic import so running in a plain browser (Vite dev) doesn't
    // require the Tauri plugin at module-load time.
    const mod = await import("@tauri-apps/plugin-store");
    const s = await mod.load("auth.dat", { autoSave: false });
    return {
      async get<T>(key) { return (await s.get<T>(key)) ?? null; },
      async set(key, v) { await s.set(key, v); },
      async delete(key) { await s.delete(key); },
      async save() { await s.save(); },
    };
  } catch {
    return null;
  }
}

function makeLocalStorageShim(): KVStore {
  const prefix = "th.auth.";
  return {
    async get<T>(key) {
      const raw = localStorage.getItem(prefix + key);
      if (raw == null) return null;
      try { return JSON.parse(raw) as T; } catch { return null; }
    },
    async set(key, v) { localStorage.setItem(prefix + key, JSON.stringify(v)); },
    async delete(key) { localStorage.removeItem(prefix + key); },
    async save() { /* noop */ },
  };
}

let storePromise: Promise<KVStore> | null = null;
async function backend(): Promise<KVStore> {
  if (!storePromise) {
    storePromise = (async () => (await openTauriStore()) ?? makeLocalStorageShim())();
  }
  return storePromise;
}

interface AuthState {
  accessToken: string | null;
  refreshToken: string | null;
  user: UserProfile | null;
  isReady: boolean;

  hydrate: () => Promise<void>;
  login: (tokens: { access_token: string; refresh_token: string }, user: UserProfile) => Promise<void>;
  logout: () => Promise<void>;
  refresh: () => Promise<boolean>;
  setUser: (u: UserProfile) => Promise<void>;
}

export const useAuthStore = create<AuthState>((set, get) => ({
  accessToken: null,
  refreshToken: null,
  user: null,
  isReady: false,

  async hydrate() {
    try {
      const s = await backend();
      const accessToken = (await s.get<string>("accessToken")) ?? null;
      const refreshToken = (await s.get<string>("refreshToken")) ?? null;
      const user = (await s.get<UserProfile>("user")) ?? null;
      set({ accessToken, refreshToken, user, isReady: true });
    } catch {
      set({ isReady: true });
    }
  },

  async login(tokens, user) {
    const s = await backend();
    await s.set("accessToken", tokens.access_token);
    await s.set("refreshToken", tokens.refresh_token);
    await s.set("user", user);
    await s.save();
    set({ accessToken: tokens.access_token, refreshToken: tokens.refresh_token, user });
  },

  async logout() {
    const s = await backend();
    await s.delete("accessToken");
    await s.delete("refreshToken");
    await s.delete("user");
    await s.save();
    set({ accessToken: null, refreshToken: null, user: null });
  },

  async refresh() {
    const refreshToken = get().refreshToken;
    if (!refreshToken) return false;
    try {
      const res = await api.post<{ access_token: string; refresh_token: string }>(
        "/api/v1/auth/refresh",
        { refresh_token: refreshToken },
      );
      const s = await backend();
      await s.set("accessToken", res.access_token);
      await s.set("refreshToken", res.refresh_token);
      await s.save();
      set({ accessToken: res.access_token, refreshToken: res.refresh_token });
      return true;
    } catch {
      await get().logout();
      return false;
    }
  },

  async setUser(u) {
    const s = await backend();
    await s.set("user", u);
    await s.save();
    set({ user: u });
  },
}));
