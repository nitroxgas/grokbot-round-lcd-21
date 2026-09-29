#include "i18n.h"

static Locale g_locale = Locale::Pt;

// Order must match I18nId
static const char* const kPt[] = {
    "Grok Bot",
    "parado",
    "BOOT",
    "INICIO",
    "STATUS",
    "CONFIRMADO",
    "TRABALHO",
    "PRONTO",
    "ERRO",
    "A",
    "B",
    "C",
    "OK",
    "Voltar",
    "Wi-Fi",
    "OK",
    "Retry",
    "Inicio",
    "online",
    "frota",
    "toque um slot",
    "toque um slot ou espere MQTT",
    "round · idle",
    "Webhook enviado",
    "Acao concluida",
    "Pronto",
    "v0.1 scaffold OK",
    "toque Retry",
    "WiFi timeout",
    "PrintMaker",
    "Fab CAD",
    "EspForge",
    "Idle",
    "Working",
    "Stand by",
};

static const char* const kEn[] = {
    "Grok Bot",
    "idle",
    "BOOT",
    "HOME",
    "STATUS",
    "CONFIRMED",
    "WORKING",
    "DONE",
    "ERROR",
    "A",
    "B",
    "C",
    "OK",
    "Back",
    "Wi-Fi",
    "OK",
    "Retry",
    "Home",
    "online",
    "fleet",
    "tap a slot",
    "tap a slot or wait for MQTT",
    "round · idle",
    "Webhook sent",
    "Action completed",
    "Ready",
    "v0.1 scaffold OK",
    "tap Retry",
    "WiFi timeout",
    "PrintMaker",
    "Fab CAD",
    "EspForge",
    "Idle",
    "Working",
    "Stand by",
};

static const char* const kEs[] = {
    "Grok Bot",
    "en espera",
    "BOOT",
    "INICIO",
    "STATUS",
    "CONFIRMADO",
    "TRABAJO",
    "LISTO",
    "ERROR",
    "A",
    "B",
    "C",
    "OK",
    "Atras",
    "Wi-Fi",
    "OK",
    "Retry",
    "Inicio",
    "online",
    "flota",
    "toque un slot",
    "toque un slot o espere MQTT",
    "round · idle",
    "Webhook enviado",
    "Accion completada",
    "Listo",
    "v0.1 scaffold OK",
    "toque Retry",
    "WiFi timeout",
    "PrintMaker",
    "Fab CAD",
    "EspForge",
    "Idle",
    "Working",
    "Stand by",
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
