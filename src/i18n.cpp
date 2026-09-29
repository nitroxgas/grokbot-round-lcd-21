#include "i18n.h"

static Locale g_locale = Locale::Pt;

static const char* const kPt[] = {
    "Grok Bot", "parado", "BOOT", "INICIO", "FROTA", "CONFIRMA", "TRABALHO",
    "PRONTO", "ERRO", "Slot A", "Slot B", "Slot C", "Confirmar", "Voltar", "Wi‑Fi",
};
static const char* const kEn[] = {
    "Grok Bot", "idle", "BOOT", "HOME", "FLEET", "CONFIRM", "WORKING",
    "DONE", "ERROR", "Slot A", "Slot B", "Slot C", "Confirm", "Back", "Wi‑Fi",
};
static const char* const kEs[] = {
    "Grok Bot", "en espera", "BOOT", "INICIO", "FLOTA", "CONFIRMA", "TRABAJO",
    "LISTO", "ERROR", "Slot A", "Slot B", "Slot C", "Confirmar", "Atras", "Wi‑Fi",
};

void i18nSetLocale(Locale locale) { g_locale = locale; }
Locale i18nLocale() { return g_locale; }

const char* i18nStr(I18nId id) {
  const auto i = static_cast<uint16_t>(id);
  if (i >= static_cast<uint16_t>(I18nId::Count)) {
    return "";
  }
  switch (g_locale) {
    case Locale::Es:
      return kEs[i];
    case Locale::En:
      return kEn[i];
    case Locale::Pt:
    default:
      return kPt[i];
  }
}
