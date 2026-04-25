import { Link } from "react-router-dom";
import { useTranslation } from "react-i18next";

export default function RegisterPage() {
  const { t } = useTranslation();
  return (
    <div className="flex h-screen w-screen items-center justify-center bg-canvas text-fg">
      <div className="w-[380px] rounded-xl border border-border bg-surface-raised p-8 shadow-md">
        <h1 className="font-display text-[28px] font-bold">{t("auth.register")}</h1>
        <p className="mt-8 text-xs text-fg-subtle">
          [ Claude Design replaces this — see prompt #5. ]
        </p>
        <p className="mt-6 text-sm">
          <Link to="/login" className="text-brand hover:underline">
            {t("auth.login")}
          </Link>
        </p>
      </div>
    </div>
  );
}
