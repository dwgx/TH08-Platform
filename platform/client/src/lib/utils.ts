import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

// shadcn's standard cn helper.
export function cn(...inputs: ClassValue[]): string {
  return twMerge(clsx(inputs));
}

export function formatUID(uid: string | number): string {
  const s = typeof uid === "number" ? String(uid) : uid;
  // Space every 4 from the right — easier to read long UIDs without
  // changing the canonical value.
  return s.replace(/\B(?=(\d{4})+(?!\d))/g, " ");
}

export function relativeTime(iso: string, now: Date = new Date()): string {
  const then = new Date(iso);
  const deltaSec = Math.round((now.getTime() - then.getTime()) / 1000);
  if (deltaSec < 60)   return "just now";
  if (deltaSec < 3600) return `${Math.floor(deltaSec / 60)}m ago`;
  if (deltaSec < 86400) return `${Math.floor(deltaSec / 3600)}h ago`;
  if (deltaSec < 2592000) return `${Math.floor(deltaSec / 86400)}d ago`;
  return then.toLocaleDateString();
}
