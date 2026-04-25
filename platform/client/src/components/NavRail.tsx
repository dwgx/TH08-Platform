import { NavLink } from "react-router-dom";
import { useTranslation } from "react-i18next";
import {
  Compass, Hash, Users, MessageCircle, Gamepad2, Bell, Settings, User,
} from "lucide-react";

import { cn } from "@/lib/utils";

// 72px fixed-width icon rail — see design-constitution §5.
export function NavRail() {
  const { t } = useTranslation();

  const items = [
    { to: "/lobby",         icon: Compass,       label: t("nav.lobby") },
    { to: "/channels",      icon: Hash,          label: t("nav.channels") },
    { to: "/friends",       icon: Users,         label: t("nav.friends") },
    { to: "/messages",      icon: MessageCircle, label: t("nav.messages") },
    { to: "/rooms",         icon: Gamepad2,      label: t("nav.rooms") },
  ];

  const footer = [
    { to: "/profile",       icon: User,     label: t("nav.profile") },
    { to: "/settings",      icon: Settings, label: t("nav.settings") },
  ];

  return (
    <nav className="flex h-full w-[72px] shrink-0 flex-col items-center justify-between border-r border-border bg-canvas py-3">
      <div className="flex flex-col items-center gap-1">
        {items.map(({ to, icon: Icon, label }) => (
          <NavLink
            key={to}
            to={to}
            title={label}
            className={({ isActive }) =>
              cn(
                "relative flex h-11 w-11 items-center justify-center rounded-md text-fg-muted transition-all",
                "hover:bg-surface-hover hover:text-fg",
                isActive && "bg-surface-raised text-fg",
              )
            }
          >
            {({ isActive }) => (
              <>
                {isActive && (
                  <span className="absolute -left-3 top-1/2 h-6 w-1 -translate-y-1/2 rounded-r bg-brand" />
                )}
                <Icon size={20} strokeWidth={1.75} />
              </>
            )}
          </NavLink>
        ))}
      </div>

      <div className="flex flex-col items-center gap-1">
        <NotificationButton />
        {footer.map(({ to, icon: Icon, label }) => (
          <NavLink
            key={to}
            to={to}
            title={label}
            className={({ isActive }) =>
              cn(
                "flex h-11 w-11 items-center justify-center rounded-md text-fg-muted transition-all",
                "hover:bg-surface-hover hover:text-fg",
                isActive && "bg-surface-raised text-fg",
              )
            }
          >
            <Icon size={20} strokeWidth={1.75} />
          </NavLink>
        ))}
      </div>
    </nav>
  );
}

function NotificationButton() {
  return (
    <button
      type="button"
      className="relative flex h-11 w-11 items-center justify-center rounded-md text-fg-muted transition-all hover:bg-surface-hover hover:text-fg"
      title="Notifications"
    >
      <Bell size={20} strokeWidth={1.75} />
      {/* TODO: badge dot when unread > 0 */}
    </button>
  );
}
