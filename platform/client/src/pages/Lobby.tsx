import { useTranslation } from "react-i18next";

export default function LobbyPage() {
  const { t } = useTranslation();
  return (
    <div className="flex h-full flex-col">
      <div className="border-b border-border bg-surface px-6 py-4">
        <h1 className="text-[20px] font-semibold">{t("lobby.title")}</h1>
      </div>
      <div className="th-scroll flex-1 overflow-auto px-6 py-5 text-sm text-fg-muted">
        {/*
          TODO: Replace this placeholder with the Claude-Design-generated
          Lobby UI. See docs/05-prompts-claude-design.md #1 (Lobby page).
          Wire data from GET /api/v1/rooms + GET /api/v1/lobby/messages +
          WebSocket topic "lobby".
        */}
        <p>Lobby placeholder — awaiting Claude Design output.</p>
      </div>
    </div>
  );
}
