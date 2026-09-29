# SPEC — Grok Bot Round LCD 2.1 (Waveshare) v0.1

**Projeto:** EspForge / George · **slug:** `grokbot-round-lcd-21`  
**Board:** Waveshare **ESP32-S3-Touch-LCD-2.1** (**SKU 30697 · ESP32-S3-Touch-LCD-2.1B** (vidro 2.5D; PCB igual ao flat))  
**HW pack:** [docs/HW-LEAD-PACK.md](./docs/HW-LEAD-PACK.md)  
**Status:** produto standalone. **Sem Home Assistant. Sem Awtrix.** Scaffold LVGL + stubs. **Não** é firmware completo.  
**Repo alvo:** GitHub **público** (Cursor IDE). Sem Origin neste projeto.  
**Flash:** proibido sem ok explícito do George.

---

## 0. Prompt de produto (aprimorado · binding)

Construir um **companheiro de mesa circular** para o Grok Bot:

1. **Entrada:** MQTT → UI no display (status da frota / bots, estados, animações).  
2. **Saída:** toques na UI (e opcionalmente botões BOOT/ações de sistema) → **webhook HTTP** para o Grok Bot.  
3. **Standalone:** maioria dos usuários **não** tem HA; zero Awtrix; zero dependência de automação doméstica.  
4. **Input primário:** **touch capacitivo** (CST820), não teclado MX.  
5. **Feedback:** **buzzer** onboard (EXIO8 via TCA9554) — clicks/confirm/error curtos.  
6. **UI:** redesenhar para **círculo 480×480**; explorar animações no estilo do app Grok Bot (idle / working / done / error) para monitorar bots.  
7. **DX:** PlatformIO, i18n `pt`/`en`/`es`, WiFiManager (portal 1º boot), secrets fora do git, repo público GitHub.  
8. **Hardware honesty:** GPIO livre escasso — não inventar pinos; citar LEAD-PACK / wiki Waveshare.

---

## 1. Requisitos (PO)

| ID | Requisito | Scaffold v0.1 |
|----|-----------|---------------|
| R1 | UI circular frota Grok Bot (slots, status, animações) | HOME LVGL stub + wireframes redondos |
| R2 | Touch → ações webhook (substitui MX) | Stub touch zones + debounce lógico |
| R3 | Buzzer feedback (tap / confirm / error) | Stub `buzzer.*` via EXIO8 |
| R4 | MQTT inbound → display | Mesmo contrato do macropad, tópicos `round` |
| R5 | Webhook outbound → Grok Bot | Paths por ação touch |
| R6 | WiFiManager + NVS | SPEC + stub |
| R7 | i18n `pt`/`en`/`es` | Stub tabelas |
| R8 | `pio run` SUCCESS no env Waveshare | Meta do scaffold |
| R9 | Repo GitHub público | Hub cria + URL |

**Fora de v0.1:** OTA, flash HW, case 3D, MQTT/HTTP reais, animações pixel-perfect do app, mic/speaker (placa não tem).

---

## 2. Hardware (oficial Waveshare — não inventar)

| Item | Valor |
|------|-------|
| SoC | ESP32-S3R8 · 240 MHz · PSRAM 8 MB · Flash 16 MB |
| Display | IPS round 2.1" · **480×480** · RGB **ST7701** · BL GPIO6 |
| Touch | **CST820** I2C (SDA15/SCL7) · INT GPIO16 · RST EXIO2 |
| Buzzer | **EXIO8** (TCA9554 — I2C 0x20) |
| BAT_ADC | GPIO4 |
| Expandidos | EXIO1–8 todos usados (LCD/TF/IMU/RTC/buzzer) — **não** trazer para fora |
| Header livre | GPIO0 · UART 43/44 · USB D± 19/20 (se não usar USB nativo) |
| Envelope | Ø **75** mm · PCB Ø65 · ~9.5 mm · 4×M2 |

I2C ocupado: `0x15, 0x20, 0x51, 0x6B, 0x7E`.

---

## 3. Arquitetura software

```
MQTT broker / Grok Bot ──► mqttApply() ──► UI state machine ──► LVGL (round)
Touch CST820 ──► hit-test zones ──► webhookFire() + buzzerPulse()
WiFiManager ──► NVS creds ──► MQTT client (sprint seguinte)
```

Framework scaffold (PO escolhe): **Arduino-PIO** (preferência default = alinhado ao macropad) **ou** ESP-IDF Waveshare demo. Default hub se George não escolher: **Arduino-PIO + LVGL 8.x**.

---

## 4. UI states (circular)

| State | Visual (v0.1) | Anima (alvo) |
|-------|---------------|--------------|
| BOOT | logo + spinner no anel | fade-in |
| HOME | título + slots A/B/C resumidos | idle breathe (anel) |
| FLEET_STATUS | lista/radial status bots | working pulse por slot |
| TOUCH_CONFIRM | ação selecionada | confirm chirp + flash |
| WORKING | bot ativo | anim “working” estilo app |
| DONE | check | done pop |
| ERROR | borda vermelha + msg | error buzz |

Touch zones (lógicos, não GPIO):

| Zone id | Ação webhook | Nota |
|---------|--------------|------|
| Z_SLOT_A/B/C | `/slot/a` … | selecionar bot |
| Z_ACTION_PRIMARY | `/action/primary` | confirmar |
| Z_ACTION_BACK | `/action/back` | voltar |
| Z_WIFI | `/action/wifi` | reabrir portal (hold) |

---

## 5. MQTT (inbound) — tópicos `round`

Prefixo: `grokbot/round/` (não reutilizar `macropad` para não colidir).

| Tópico | Efeito |
|--------|--------|
| `grokbot/round/ui/home` | `title`/`message` → HOME |
| `grokbot/round/ui/state` | `state` enum |
| `grokbot/round/status` | FLEET_STATUS · `slot`+`text` |
| `grokbot/round/anim` | `anim` = `idle\|working\|done\|error` (+ `slot` opcional) |

Estados: `BOOT`, `HOME`, `FLEET_STATUS`, `TOUCH_CONFIRM`, `WORKING`, `DONE`, `ERROR`.  
Limites de string: iguais ao macropad (title 39, message 79, …).  
Tópicos e paths **não** traduzidos (EN técnico).

---

## 6. Webhook (outbound)

Base: `CFG_WEBHOOK_BASE` (secrets). Corpo JSON:

```json
{"source":"round-lcd-21","event":"<id>","slot":"<optional>"}
```

| Event | Path |
|-------|------|
| SLOT_A/B/C | `/slot/a` … |
| PRIMARY | `/action/primary` |
| BACK | `/action/back` |
| WIFI | `/action/wifi` |

---

## 7. Buzzer

| Pattern | Uso |
|---------|-----|
| TAP | toque aceito |
| CONFIRM | webhook disparado |
| ERROR | MQTT ERROR / touch rejeitado |
| BOOT | 1 beep curto no boot |

Duty curto; silenciável por NVS `buzzer_enabled`.

---

## 8. i18n / WiFiManager

Igual macropad: locales `pt` (default) / `en` / `es`; WiFiManager portal 1º boot; reconfig por zone WIFI hold; sem HA/Awtrix.

---

## 9. Definition of Done scaffold v0.1

- [ ] `SPEC.md` + `README.md` + `docs/mqtt-webhook.md` + HW pack  
- [ ] `platformio.ini` env Waveshare 2.1  
- [ ] stubs: display LVGL HOME circular, touch zones, buzzer, mqtt, webhook, i18n  
- [ ] `pio run` SUCCESS  
- [ ] GitHub público + URL no README  
- [ ] Sem `secrets.h` no git · sem upload sem ok George  

---

## 10. Decisões PO pendentes (hub)

1. SKU: **flat 28169** vs **2.1B 30697**  
2. Framework scaffold: **Arduino-PIO** (default) vs **ESP-IDF**  
3. GitHub org/user destino do repo público  
