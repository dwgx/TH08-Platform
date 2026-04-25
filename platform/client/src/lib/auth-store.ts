// Zustand store for auth state + persistence through Tauri's store plugin
// (so tokens survive restarts). Keeping tokens in Tauri rather than
// localStorage because we don't want a renderer-process XSS (however
// unlikely) to leak them.
import { create } from "zustand";
import { load, type Store } from "@tauri-apps/plugin-store";
import { api, type UserProfile } from "@/lib/api";

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

let storePromise: Promise<Store> | null = null;
async function secureStore(): Promise<Store> {
  if (!storePromise) {
    storePromise = load("auth.dat", { autoSave: false });
  }
  return storePromise;
}

export const useAuthStore = create<AuthState>((set, get) => ({
  accessToken: null,
  refreshToken: null,
  user: null,
  isReady: false,

  async hydrate() {
    try {
      const s = await secureStore();
      const accessToken = (await s.get<string>("accessToken")) ?? null;
      const refreshToken = (await s.get<string>("refreshToken")) ?? null;
      const user = (await s.get<UserProfile>("user")) ?? null;
      set({ accessToken, refreshToken, user, isReady: true });
    } catch {
      set({ isReady: true });
    }
  },

  async login(tokens, user) {
    const s = await secureStore();
    await s.set("accessToken", tokens.access_token);
    await s.set("refreshToken", tokens.refresh_token);
    await s.set("user", user);
    await s.save();
    set({ accessToken: tokens.access_token, refreshToken: tokens.refresh_token, user });
  },

  async logout() {
    const s = await secureStore();
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
      const s = await secureStore();
      await s.set("accessToken", res.access_token);
      await s.set("refreshToken", res.refresh_token);
      await s.save();
      set({ accessToken: res.access_token, refreshToken: res.refresh_token });
      return true;
    } catch {
      // Refresh failed → force logout so UI sends the user to /login.
      await get().logout();
      return false;
    }
  },

  async setUser(u) {
    const s = await secureStore();
    await s.set("user", u);
    await s.save();
    set({ user: u });
  },
}));
