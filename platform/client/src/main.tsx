import React from "react";
import ReactDOM from "react-dom/client";
import { QueryClientProvider } from "@tanstack/react-query";
import { BrowserRouter } from "react-router-dom";
import { Toaster } from "sonner";

import App from "./App";
import { initI18n } from "./i18n";
import { queryClient } from "./lib/queryClient";
import { useAuthStore } from "./lib/auth-store";

import "./styles/globals.css";

async function bootstrap() {
  await initI18n();
  await useAuthStore.getState().hydrate();

  ReactDOM.createRoot(document.getElementById("root")!).render(
    <React.StrictMode>
      <QueryClientProvider client={queryClient}>
        <BrowserRouter>
          <App />
          <Toaster
            theme="dark"
            position="bottom-right"
            toastOptions={{
              style: {
                background: "hsl(var(--surface-bright))",
                border: "1px solid hsl(var(--border))",
                color: "hsl(var(--fg))",
              },
            }}
          />
        </BrowserRouter>
      </QueryClientProvider>
    </React.StrictMode>,
  );
}

bootstrap();
