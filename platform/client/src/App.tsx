import { useEffect } from "react";
import { Routes, Route, Navigate } from "react-router-dom";

import { useAuthStore } from "@/lib/auth-store";
import { ws } from "@/lib/ws";
import { AppShell } from "@/components/AppShell";

import LoginPage from "@/pages/Login";
import RegisterPage from "@/pages/Register";
import LobbyPage from "@/pages/Lobby";
import ChannelsPage from "@/pages/Channels";
import FriendsPage from "@/pages/Friends";
import MessagesPage from "@/pages/Messages";
import RoomsPage from "@/pages/Rooms";
import RoomDetailPage from "@/pages/RoomDetail";
import ProfilePage from "@/pages/Profile";
import SettingsPage from "@/pages/Settings";

function RequireAuth({ children }: { children: React.ReactNode }) {
  const { accessToken, isReady } = useAuthStore();
  if (!isReady) return null;
  if (!accessToken) return <Navigate to="/login" replace />;
  return <>{children}</>;
}

export default function App() {
  const accessToken = useAuthStore((s) => s.accessToken);

  // Connect/disconnect WS based on auth state. Also reconnect when
  // tokens get rotated (refresh flow).
  useEffect(() => {
    if (accessToken) {
      ws.connect();
      return () => ws.close();
    }
  }, [accessToken]);

  return (
    <Routes>
      <Route path="/login"    element={<LoginPage />} />
      <Route path="/register" element={<RegisterPage />} />

      <Route element={<RequireAuth><AppShell /></RequireAuth>}>
        <Route index element={<Navigate to="/lobby" replace />} />
        <Route path="/lobby"          element={<LobbyPage />} />
        <Route path="/channels"       element={<ChannelsPage />} />
        <Route path="/channels/:id"   element={<ChannelsPage />} />
        <Route path="/friends"        element={<FriendsPage />} />
        <Route path="/messages"       element={<MessagesPage />} />
        <Route path="/messages/:id"   element={<MessagesPage />} />
        <Route path="/rooms"          element={<RoomsPage />} />
        <Route path="/rooms/:id"      element={<RoomDetailPage />} />
        <Route path="/profile/:handle?" element={<ProfilePage />} />
        <Route path="/settings/*"     element={<SettingsPage />} />
      </Route>

      <Route path="*" element={<Navigate to="/lobby" replace />} />
    </Routes>
  );
}
