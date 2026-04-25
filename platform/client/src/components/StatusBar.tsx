import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";

// 24px status bar at the bottom. V1 surface: WS connection state,
// current presence, version. Expand later with throughput / ping.
export function StatusBar() {
  const { t } = useTranslation();
  const [connected, setConnected] = useState(false);

  useEffect(() => {
    const interval = setInterval(() => {
      // naive — later we'll expose a proper `ws.state` observable
      setConnected((s) => s); // placeholder
    }, 1000);
    return () => clearInterval(interval);
  }, []);

  return (
    <div className="flex h-6 items-center justify-between border-t border-border bg-surface-raised px-3 text-[11px] text-fg-muted select-none">
      <div className="flex items-center gap-3">
        <span className="th-dot" data-status={connected ? "online" : "offline"} />
        <span>{connected ? t("presence.online") : t("presence.offline")}</span>
      </div>
      <div className="flex items-center gap-3">
        <span className="font-mono">v0.1.0</span>
      </div>
    </div>
  );
}
