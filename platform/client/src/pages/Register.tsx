import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { AlertCircle, Loader2 } from "lucide-react";

import { api, type LoginResponse } from "@/lib/api";
import { useAuthStore } from "@/lib/auth-store";
import { cn } from "@/lib/utils";

export default function RegisterPage() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const login = useAuthStore((s) => s.login);

  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [username, setUsername] = useState("");
  const [handle, setHandle] = useState("");
  const [loading, setLoading] = useState(false);
  const [err, setErr] = useState<string | null>(null);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    if (!email || password.length < 8 || !username) {
      setErr("邮箱、密码（≥8位）、用户名必填"); return;
    }
    setLoading(true); setErr(null);
    try {
      await api.post("/api/v1/auth/register-email", {
        email, password, username,
        handle: handle || undefined,
        locale: "zh-CN",
      });
      // Auto-login right after.
      const res = await api.post<LoginResponse>("/api/v1/auth/login-email", {
        identifier: email, password,
      });
      await login({ access_token: res.access_token, refresh_token: res.refresh_token }, res.user);
      toast.success(`欢迎 ${res.user.username}，UID #${res.user.uid}`);
      navigate("/lobby", { replace: true });
    } catch (e: any) {
      const code = e?.code as string | undefined;
      const map: Record<string, string> = {
        handle_taken:      t("errors.handle_taken"),
        email_taken:       t("errors.email_taken"),
        validation_failed: "输入不合法（邮箱格式？密码太短？）",
        invalid_credentials: "注册成功但自动登录失败，请手动登录",
      };
      setErr(map[code ?? ""] ?? (e?.message || t("errors.server")));
    } finally {
      setLoading(false);
    }
  }

  return (
    <div className="flex h-screen w-screen items-center justify-center bg-canvas">
      <div className="w-[460px] rounded-xl border border-border bg-surface-raised p-10 shadow-lg">
        <div className="mb-6 flex items-center gap-3">
          <div className="h-10 w-10 rounded-full"
               style={{ background: "conic-gradient(from 90deg, hsl(var(--brand-primary)), hsl(var(--brand-accent)), hsl(var(--brand-primary)))" }} />
          <div>
            <div className="text-[12px] text-fg-muted">{t("app.name")}</div>
            <div className="text-[11px] text-fg-subtle">{t("app.tagline")}</div>
          </div>
        </div>

        <h1 className="font-display text-[28px] font-bold">{t("auth.register")}</h1>
        <p className="mt-2 text-[12px] text-fg-muted">
          邮箱注册 · Sign up with email. 后续可绑定 OAuth 账号。
        </p>

        {err && (
          <div className="mt-4 flex items-start gap-2 rounded-md border border-danger/40 bg-danger/10 px-3 py-2 text-[12px] text-danger">
            <AlertCircle size={14} strokeWidth={1.75} className="mt-[2px] shrink-0" />
            <span>{err}</span>
          </div>
        )}

        <form className="mt-6 flex flex-col gap-3" onSubmit={submit}>
          <Field label={t("auth.email")}>
            <input type="email" value={email} onChange={(e) => setEmail(e.target.value)}
              placeholder="you@example.com" className={inputCls} disabled={loading} autoFocus />
          </Field>
          <Field label={t("auth.password")} hint="至少 8 位">
            <input type="password" value={password} onChange={(e) => setPassword(e.target.value)}
              placeholder="••••••••" className={inputCls} disabled={loading} />
          </Field>
          <Field label={t("auth.username")} hint="显示名，后面可改">
            <input type="text" value={username} onChange={(e) => setUsername(e.target.value)}
              placeholder="灵梦" className={inputCls} disabled={loading} />
          </Field>
          <Field label={t("auth.handle")} hint={t("auth.handle_hint") + " · 留空将自动生成"}>
            <input type="text" value={handle} onChange={(e) => setHandle(e.target.value.toLowerCase())}
              placeholder="reimu" className={cn(inputCls, "font-mono")} disabled={loading} />
          </Field>

          <button type="submit" disabled={loading} className={primaryBtnCls}>
            {loading ? <Loader2 size={14} className="animate-spin" /> : null}
            {t("auth.register")}
          </button>
        </form>

        <p className="mt-6 text-center text-[12px] text-fg-muted">
          已有账号？ <Link to="/login" className="text-brand hover:underline">{t("auth.login")} →</Link>
        </p>
      </div>
    </div>
  );
}

function Field({ label, hint, children }: { label: string; hint?: string; children: React.ReactNode }) {
  return (
    <label className="flex flex-col gap-1.5">
      <span className="flex items-baseline justify-between">
        <span className="text-[12px] text-fg-muted">{label}</span>
        {hint && <span className="text-[11px] text-fg-subtle">{hint}</span>}
      </span>
      {children}
    </label>
  );
}

const inputCls =
  "h-10 w-full rounded-md border border-border bg-surface px-3 text-[13px] outline-none transition-all " +
  "placeholder:text-fg-subtle focus:border-brand focus:ring-4 focus:ring-brand/20 disabled:opacity-50";

const primaryBtnCls =
  "mt-2 flex h-10 items-center justify-center gap-2 rounded-md bg-brand font-medium text-[13px] text-white " +
  "transition-all hover:brightness-110 disabled:opacity-60 disabled:cursor-not-allowed";
