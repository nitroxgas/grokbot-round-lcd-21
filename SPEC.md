# SPEC — Grok Bot Round LCD 2.1 (Waveshare) v0.1

**Projeto:** EspForge / George · **slug:** `grokbot-round-lcd-21`  
**Board:** Waveshare **ESP32-S3-Touch-LCD-2.1** (**SKU 30697 · ESP32-S3-Touch-LCD-2.1B** (vidro 2.5D; PCB igual ao flat))  
**HW pack:** [docs/HW-LEAD-PACK.md](./docs/HW-LEAD-PACK.md)  
**Status:** produto standalone. **Sem Home Assistant. Sem Awtrix.** UI LVGL + comunicação real (MQTT 1 tópico / webhook POST / WiFiManager). **Não** é firmware completo.  
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
| R4 | MQTT inbound → display | **Um** tópico por pessoa (só em `secrets.h`); 4 corpos JSON distinguidos por campos |
| R5 | Webhook outbound → Grok Bot | POST HTTP real; base/token só em `secrets.h`; paths por ação touch |
| R6 | WiFiManager + NVS | SPEC + stub |
| R7 | i18n `pt`/`en`/`es` | Stub tabelas |
| R8 | `pio run` SUCCESS no env Waveshare | Meta do scaffold |
| R9 | Repo GitHub público | Hub cria + URL |

**Fora de v0.1:** OTA, flash HW, case 3D, animações pixel-perfect do app, mic/speaker (placa não tem).  
**v0.3:** MQTT (1 subscribe) e webhook (POST) reais; WiFiManager com portal no 1º boot.

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
MQTT broker / Grok Bot ──► 1 subscribe (MQTT_TOPIC) ──► mqttApply(json) ──► UI state machine ──► LVGL (round)
Touch CST820 ──► hit-test zones ──► webhookFire() → HTTP POST (WEBHOOK_BASE) + buzzerPulse()
WiFiManager (portal 1º boot) ──► creds WiFi em NVS ──► mqttLoop() connect/subscribe/reconnect
secrets.h (gitignored) ──► WEBHOOK_BASE/TOKEN · MQTT_HOST/PORT/TLS/USER/PASS/TOPIC
```

Sem `secrets.h` (ou campos vazios) o binário compila, mas não faz POST nem liga ao broker. Não há URL nem tópico embutidos no código.

Framework scaffold: **Arduino-PIO + LVGL 8.x** (PO 2026-09-29).

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

## 5. MQTT (inbound) — um tópico por pessoa

O dispositivo faz **um** `subscribe` no tópico `MQTT_TOPIC` de `secrets.h`. Não há prefixo nem tópico fixo no firmware.  
Broker (`MQTT_HOST`/`MQTT_PORT`), `MQTT_TLS` (1 = `WiFiClientSecure`, 0 = `WiFiClient`; sem CA embutida), `MQTT_USER`/`MQTT_PASS` vêm só de `secrets.h`. Se host, user, pass ou tópico estiver vazio, **não liga** (sem broker anónimo). Reconnect automático a cada 5 s enquanto houver WiFi.

O payload é um dos quatro corpos JSON já existentes, distinguidos pelos campos:

| Campos no JSON | Efeito |
|----------------|--------|
| `title` / `message` | HOME (`title` 39, `message` 79 chars) |
| `state` | enum de estado |
| `slot` + `text` (ou só `text`) | FLEET_STATUS |
| `anim` (+ `slot` opcional) | `anim` = `idle\|working\|done\|error` |

Ordem de deteção: `anim` → `state` → `text` → `title`/`message`. Sem envelope novo.

Estados: `BOOT`, `HOME`, `FLEET_STATUS`, `TOUCH_CONFIRM`, `WORKING`, `DONE`, `ERROR`.  
Paths e chaves JSON **não** traduzidos (EN técnico).

Reserva (desligado por defeito, **não** é o caminho normal): poll HTTP ao webhook em intervalo longo, só se MQTT não for viável para alguém.

---

## 6. Webhook (outbound)

POST HTTP(S) real (`HTTPClient`). URL = `WEBHOOK_BASE` + path; `WEBHOOK_BASE` e `WEBHOOK_TOKEN` (opcional, `Authorization: Bearer`) vêm **só** de `secrets.h`. Base vazia → **não há POST**. `https://` usa `WiFiClientSecure` sem CA embutida. Corpo JSON:

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

Igual macropad: locales `pt` (default) / `en` / `es`; WiFiManager portal 1º boot (AP `GrokBot-Round`, não bloqueante, password WiFi fica em NVS — nunca em `secrets.h`); reconfig por zone WIFI hold; sem HA/Awtrix.

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

1. ~~SKU~~ → **30697 2.1B**  
2. ~~Framework~~ → **Arduino-PIO + LVGL**  
3. GitHub org/user destino do repo público (em criação)  
