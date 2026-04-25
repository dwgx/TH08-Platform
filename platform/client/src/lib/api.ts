// Thin fetch wrapper. Handles:
//   * base URL prefix
//   * Bearer token injection (from auth store)
//   * JSON request/response
//   * Automatic refresh on 401 "token_expired" (single retry)
//   * Uniform APIError thrown for non-2xx
import { useAuthStore } from "@/lib/auth-store";

const BASE_URL =
  (import.meta.env.VITE_API_BASE_URL as string | undefined) ??
  "http://localhost:8080";

export interface APIError {
  code: string;
  message: string;
  detail?: unknown;
  request_id?: string;
}

class APIErrorImpl extends Error {
  status: number;
  code: string;
  detail?: unknown;
  requestId?: string;
  constructor(status: number, body: { error: APIError }) {
    super(body.error.message);
    this.status = status;
    this.code = body.error.code;
    this.detail = body.error.detail;
    this.requestId = body.error.request_id;
  }
}
export { APIErrorImpl as APIError };

async function raw<T>(
  path: string,
  init: RequestInit = {},
  { retryOnAuth = true }: { retryOnAuth?: boolean } = {},
): Promise<T> {
  const headers = new Headers(init.headers);
  headers.set("Content-Type", "application/json");

  const access = useAuthStore.getState().accessToken;
  if (access) headers.set("Authorization", `Bearer ${access}`);

  const res = await fetch(`${BASE_URL}${path}`, {
    ...init,
    headers,
    credentials: "omit",
  });

  if (res.status === 204) return undefined as T;

  const text = await res.text();
  const body = text ? JSON.parse(text) : undefined;

  if (!res.ok) {
    // Refresh flow on access-token expiry
    if (retryOnAuth && res.status === 401 && body?.error?.code === "token_expired") {
      const refreshed = await useAuthStore.getState().refresh();
      if (refreshed) return raw<T>(path, init, { retryOnAuth: false });
    }
    throw new APIErrorImpl(res.status, body ?? { error: { code: "unknown", message: "Unknown error" } });
  }
  return body as T;
}

export const api = {
  get:    <T>(path: string, init?: RequestInit) => raw<T>(path, { ...init, method: "GET" }),
  post:   <T>(path: string, data?: unknown, init?: RequestInit) =>
    raw<T>(path, { ...init, method: "POST", body: data !== undefined ? JSON.stringify(data) : undefined }),
  patch:  <T>(path: string, data?: unknown, init?: RequestInit) =>
    raw<T>(path, { ...init, method: "PATCH", body: data !== undefined ? JSON.stringify(data) : undefined }),
  put:    <T>(path: string, data?: unknown, init?: RequestInit) =>
    raw<T>(path, { ...init, method: "PUT", body: data !== undefined ? JSON.stringify(data) : undefined }),
  delete: <T>(path: string, init?: RequestInit) => raw<T>(path, { ...init, method: "DELETE" }),
  baseURL: () => BASE_URL,
};

export type UserProfile = {
  uid: string;
  username: string;
  handle: string;
  avatar_url?: string | null;
  bio?: string | null;
  tags: string[];
  locale: string;
  country?: string | null;
  roles: string[];
  created_at: string;
  last_seen_at?: string | null;
};

export type LoginResponse = {
  access_token: string;
  refresh_token: string;
  expires_in: number;
  user: UserProfile;
};
