import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { toast } from "sonner";
import { AlertCircle, Eye, EyeOff, Loader2 } from "lucide-react";

import { api, type LoginResponse } from "@/lib/api";
import { useAuthStore } from "@/lib/auth-store";
import { cn } from "@/lib/utils";

export default function LoginPage() {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const login = useAuthStore((s) => s.login);

  const [identifier, setIdentifier] = useState("");
  const [password, setPassword] = useState("");
  const [showPw, setShowPw] = useState(false);
  const [loading, setLoading] = useState(false);
  const [err, setErr] = useState<string | null>(null);

  async function submit(e: React.FormEvent) {
    e.preventDefault();
    if (!identifier || !password) { setErr(t("errors.validation")); return; }
    setLoading(true); setErr(null);
    try {
      const res = await api.post<LoginResponse>("/api/v1/auth/login-email", {
        identifier, password,
      });
      await login({ access_token: res.access_token, refresh_token: res.refresh_token }, res.user);
      toast.success(t("auth.login_success"));
      navigate("/lobby", { replace: true });
    } catch (e: any) {
      const code = e?.code as string | undefined;
      const msg =
        code === "invalid_credentials" ? t("errors.invalid_credentials")
        : code === "rate_limited"      ? t("errors.validation")
        : (e?.message || t("errors.server"));
      setErr(msg);
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

        <h1 className="font-display text-[28px] font-bold">{t("auth.login")}</h1>
        <p className="mt-2 text-[12px] text-fg-muted">
          用邮箱或 @用户名登录 · Sign in with email or @handle
        </p>

        {err && (
          <div className="mt-4 flex items-start gap-2 rounded-md border border-danger/40 bg-danger/10 px-3 py-2 text-[12px] text-danger">
            <AlertCircle size={14} strokeWidth={1.75} className="mt-[2px] shrink-0" />
            <span>{err}</span>
          </div>
        )}

        <form className="mt-6 flex flex-col gap-3" onSubmit={submit}>
          <label className="flex flex-col gap-1.5">
            <span className="text-[12px] text-fg-muted">{t("auth.email")} / {t("auth.handle")}</span>
            <input
              autoFocus
              type="text"
              value={identifier}
              onChange={(e) => setIdentifier(e.target.value)}
              placeholder="dwgx 或 you@example.com"
              className={inputCls}
              disabled={loading}
            />
          </label>
          <label className="flex flex-col gap-1.5">
            <div className="flex justify-between">
              <span className="text-[12px] text-fg-muted">{t("auth.password")}</span>
              <a className="text-[11px] text-fg-subtle hover:text-brand" href="#">
                {t("auth.forgot_password")}
              </a>
            </div>
            <div className="relative">
              <input
                type={showPw ? "text" : "password"}
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                placeholder="••••••••"
                className={cn(inputCls, "pr-10")}
                disabled={loading}
              />
              <button type="button" onClick={() => setShowPw((v) => !v)}
                className="absolute right-2 top-1/2 -translate-y-1/2 rounded p-1 text-fg-muted hover:text-fg">
                {showPw ? <EyeOff size={14} /> : <Eye size={14} />}
              </button>
            </div>
          </label>

          <button type="submit" disabled={loading} className={primaryBtnCls}>
            {loading ? <Loader2 size={14} className="animate-spin" /> : null}
            {t("auth.login")}
          </button>
        </form>

        <div className="my-5 flex items-center gap-3">
          <div className="h-px flex-1 bg-border" />
          <div className="text-[11px] text-fg-subtle">{t("auth.or_continue_with")}</div>
          <div className="h-px flex-1 bg-border" />
        </div>

        <div className="grid grid-cols-3 gap-2">
          <button className={oauthBtnCls} disabled>GitHub</button>
          <button className={oauthBtnCls} disabled>Discord</button>
          <button className={oauthBtnCls} disabled>QQ</button>
        </div>
        <p className="mt-2 text-[11px] text-fg-subtle text-center">OAuth 尚未配置（V1 稍后）</p>

        <p className="mt-6 text-center text-[12px] text-fg-muted">
          没有账号？ <Link to="/register" className="text-brand hover:underline">{t("auth.register")} →</Link>
        </p>
      </div>
    </div>
  );
}

const inputCls =
  "h-10 w-full rounded-md border border-border bg-surface px-3 text-[13px] outline-none transition-all " +
  "placeholder:text-fg-subtle focus:border-brand focus:ring-4 focus:ring-brand/20 disabled:opacity-50";

const primaryBtnCls =
  "flex h-10 items-center justify-center gap-2 rounded-md bg-brand font-medium text-[13px] text-white " +
  "transition-all hover:brightness-110 disabled:opacity-60 disabled:cursor-not-allowed";

const oauthBtnCls =
  "h-10 rounded-md border border-border bg-surface text-[12px] text-fg-muted transition-all " +
  "hover:bg-surface-hover disabled:opacity-50 disabled:cursor-not-allowed";
