import { Outlet } from "react-router-dom";
import { Group as PanelGroup, Panel, Separator as PanelResizeHandle } from "react-resizable-panels";

import { NavRail } from "./NavRail";
import { TitleBar } from "./TitleBar";
import { StatusBar } from "./StatusBar";

// AppShell = three-column resizable layout + TitleBar + StatusBar,
// mirroring design-constitution §5. Route content mounts into <Outlet/>.
// Right dock is optional per-page (pages call `useRightDock(...)` to
// provide content); AppShell shows it when non-null.
export function AppShell() {
  return (
    <div className="flex h-screen w-screen flex-col overflow-hidden">
      <TitleBar />

      <div className="flex min-h-0 flex-1">
        <NavRail />

        <div className="flex-1 min-w-0">
          <PanelGroup direction="horizontal">
            <Panel defaultSize={75} minSize={40}>
              <main className="h-full w-full overflow-hidden">
                <Outlet />
              </main>
            </Panel>

            <PanelResizeHandle className="w-[3px] bg-border/40 hover:bg-brand/60 active:bg-brand transition-colors cursor-col-resize" />

            <Panel defaultSize={25} minSize={18} maxSize={40} collapsible>
              <div id="right-dock" className="h-full border-l border-border bg-surface" />
            </Panel>
          </PanelGroup>
        </div>
      </div>

      <StatusBar />
    </div>
  );
}
