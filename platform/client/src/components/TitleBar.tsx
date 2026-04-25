import { useLocation } from "react-router-dom";
import { useTranslation } from "react-i18next";
import { Search } from "lucide-react";

import { useAuthStore } from "@/lib/auth-store";

// 36px title bar with window-drag region + breadcrumb + global search.
// When we enable `tauri.conf.json#decorations: false` the whole bar
// becomes the drag handle (data-tauri-drag-region).
export function TitleBar() {
  const { t } = useTranslation();
  const loc = useLocation();
  const user = useAuthStore((s) => s.user);

  const crumb = humanizePath(loc.pathname, t);

  return (
    <div
      data-tauri-drag-region
      className="flex h-9 items-center gap-4 border-b border-border bg-surface-raised px-3 text-[12px] text-fg-muted select-none"
    >
      <div className="font-display text-[12px] font-semibold tracking-wide text-fg">
        TH-Platform
      </div>
      <span className="text-border-strong">/</span>
      <div>{crumb}</div>

      <div className="flex-1" />

      <button
        type="button"
        className="flex h-6 items-center gap-2 rounded-md bg-surface px-2 text-[11px] text-fg-muted hover:text-fg"
        title={t("common.search")}
      >
        <Search size={12} strokeWidth={1.75} />
        <span>{t("common.search")}</span>
        <kbd className="ml-4 rounded bg-canvas px-1 py-[1px] text-[10px] font-mono text-fg-subtle">
          Ctrl K
        </kbd>
      </button>

      {user && (
        <div className="flex items-center gap-2">
          <div className="h-6 w-6 rounded-full bg-brand/40 ring-1 ring-border" />
          <span className="font-mono text-[11px] text-fg-muted">#{user.uid}</span>
        </div>
      )}
    </div>
  );
}

function humanizePath(pathname: string, t: (key: string) => string): string {
  const seg = pathname.split("/")[1] ?? "lobby";
  const map: Record<string, string> = {
    lobby:    t("nav.lobby"),
    channels: t("nav.channels"),
    friends:  t("nav.friends"),
    messages: t("nav.messages"),
    rooms:    t("nav.rooms"),
    profile:  t("nav.profile"),
    settings: t("nav.settings"),
  };
  return map[seg] ?? seg;
}
