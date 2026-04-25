import type { Config } from "tailwindcss";

const config: Config = {
  darkMode: "class",
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        border:              "hsl(var(--border))",
        "border-strong":     "hsl(var(--border-strong))",
        input:               "hsl(var(--input))",
        ring:                "hsl(var(--ring))",
        canvas:              "hsl(var(--canvas))",
        surface:             "hsl(var(--surface))",
        "surface-raised":    "hsl(var(--surface-raised))",
        "surface-bright":    "hsl(var(--surface-bright))",
        "surface-hover":     "hsl(var(--surface-hover))",
        background:          "hsl(var(--canvas))",
        foreground:          "hsl(var(--fg))",
        "fg-muted":          "hsl(var(--fg-muted))",
        "fg-subtle":          "hsl(var(--fg-subtle))",
        brand: {
          DEFAULT:            "hsl(var(--brand-primary))",
          foreground:         "hsl(0 0% 100%)",
          accent:             "hsl(var(--brand-accent))",
        },
        primary: {
          DEFAULT:            "hsl(var(--brand-primary))",
          foreground:         "hsl(0 0% 100%)",
        },
        secondary: {
          DEFAULT:            "hsl(var(--surface-bright))",
          foreground:         "hsl(var(--fg))",
        },
        muted: {
          DEFAULT:            "hsl(var(--surface))",
          foreground:         "hsl(var(--fg-muted))",
        },
        accent: {
          DEFAULT:            "hsl(var(--surface-hover))",
          foreground:         "hsl(var(--fg))",
        },
        destructive: {
          DEFAULT:            "hsl(var(--danger))",
          foreground:         "hsl(0 0% 100%)",
        },
        card: {
          DEFAULT:            "hsl(var(--surface-raised))",
          foreground:         "hsl(var(--fg))",
        },
        popover: {
          DEFAULT:            "hsl(var(--surface-bright))",
          foreground:         "hsl(var(--fg))",
        },
        success:             "hsl(var(--success))",
        warning:             "hsl(var(--warning))",
        danger:              "hsl(var(--danger))",
        info:                "hsl(var(--info))",
      },
      borderRadius: {
        sm: "4px",
        md: "6px",
        lg: "10px",
        xl: "14px",
      },
      fontFamily: {
        sans: ["var(--font-sans)"],
        mono: ["var(--font-mono)"],
        display: ["var(--font-display)"],
      },
      keyframes: {
        "fade-in":  { from: { opacity: "0", transform: "translateY(2px)" }, to: { opacity: "1", transform: "translateY(0)" } },
        "scale-in": { from: { opacity: "0", transform: "scale(0.98)" },     to: { opacity: "1", transform: "scale(1)" } },
      },
      animation: {
        "fade-in":  "fade-in 180ms cubic-bezier(0.2, 0, 0, 1) both",
        "scale-in": "scale-in 140ms cubic-bezier(0.2, 0, 0, 1) both",
      },
    },
  },
  plugins: [],
};

export default config;
