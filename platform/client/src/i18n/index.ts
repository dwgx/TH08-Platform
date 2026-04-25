// i18next bootstrap with:
//   - Lazy-loaded namespaces per page
//   - Fallback chain: zh-CN → en
//   - Browser language detection, overridable by user prefs
//   - Type-safe keys via `declare module` in types.d.ts
//
// Design intent (design constitution §13):
//   * Default zh-CN for 99% of users
//   * en for i18n accessibility (fallback)
//   * ja because Touhou is Japanese — add later when we have translators
import i18n from "i18next";
import { initReactI18next } from "react-i18next";
import LanguageDetector from "i18next-browser-languagedetector";

import common_zh from "./locales/zh-CN/common.json";
import common_en from "./locales/en/common.json";
import common_ja from "./locales/ja/common.json";

export const SUPPORTED_LOCALES = ["zh-CN", "en", "ja"] as const;
export type SupportedLocale = (typeof SUPPORTED_LOCALES)[number];

export const LOCALE_LABELS: Record<SupportedLocale, string> = {
  "zh-CN": "简体中文",
  en: "English",
  ja: "日本語",
};

export async function initI18n(preferredLocale?: SupportedLocale) {
  await i18n
    .use(LanguageDetector)
    .use(initReactI18next)
    .init({
      resources: {
        "zh-CN": { common: common_zh },
        en:      { common: common_en },
        ja:      { common: common_ja },
      },
      lng: preferredLocale,
      fallbackLng: ["zh-CN", "en"],
      defaultNS: "common",
      ns: ["common"],
      interpolation: { escapeValue: false },
      detection: {
        order: ["localStorage", "navigator"],
        lookupLocalStorage: "th.locale",
        caches: ["localStorage"],
      },
      returnNull: false,
    });
  return i18n;
}

export default i18n;
