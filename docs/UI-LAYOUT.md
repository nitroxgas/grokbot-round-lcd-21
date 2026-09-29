# UI Layout — round 480×480 (v0.2)

Adaptado dos mocks macropad (HOME / STATUS / CONFIRMED / ERROR), estilo dark `#0C0C0E`, texto `#F5F5F7`, accent vermelho `#E11D2E`, verde `#38D996`, âmbar working `#E8A838`.

## Zonas (círculo, safe r≈210)

| Zona | Região | Conteúdo |
|------|--------|----------|
| Header | topo ~y 40–70 | label de estado (`HOME` / `STATUS` / …) |
| Anim | centro disco Ø≈176, cy≈220 | marca Grok Bot + anim idle/working/done/error |
| Mensagens | y≈300–340 | título (22) + mensagem interação (14 muted) |
| Botões | arco inferior | pills touch Ø lógico ~78×36 |

## Telas

1. **HOME** — anim idle (anel azul), título `round · idle`, msg MQTT/toque, pill `online`, botões A/B/C.
2. **FLEET_STATUS** — 3 cards em arco (slot letter vermelho, nome, status colorido); toque → webhook slot.
3. **TOUCH_CONFIRM** — check verde no disco, `Webhook sent` / `Action completed`, Back + OK.
4. **WORKING** — anel âmbar + arcs, nome do bot + progresso na msg, Back.
5. **DONE** — marca verde, msg sucesso, Home.
6. **ERROR** — borda vermelha, `!`, título erro + hint, Back + Retry.

## Touch → webhook (inalterado SPEC)

Z_SLOT_A/B/C, Z_ACTION_PRIMARY, Z_ACTION_BACK, Z_WIFI (hold).

## Mocks PNG

`docs/ui-mocks/grokbot-round-*-v0.2.png` — gerar com `python3 generate_mocks.py` (precisa Pillow).

## Implementação LVGL

- Substituir HOME stub único por state machine que **mostra/esconde** ou reconstrói screens por `UiState`.
- Botões = `lv_btn` (ou `lv_obj` clickable) no arco; hit-test também via zonas touch CST820.
- Anim mínima v0.2: `lv_anim` no border do disco (breathe idle, pulse working); sem assets bitmap pesados.
- Manter i18n pt/en/es nos strings de UI.
- `pio run -e waveshare_round_21` deve continuar SUCCESS. **Sem flash.**

## Anim polish v0.3 (alvo firmware)

| Estado | Animação |
|--------|----------|
| HOME / idle | breathe: border opacity + leve scale do disco (~1.4s) + halo azul |
| WORKING | pulse rápido (~0.45s) + 3 arcs rotativos âmbar |
| DONE / CONFIRM | pop scale overshoot no disco + check |
| ERROR | rim vermelho buzz (espessura) + leve shake do ícone |
| FLEET | card do slot `working` com border pulse |

Projeção visual: `docs/ui-mocks/proj-*.gif` + `proj-contact-v0.3.png`.

## Visual language v0.4 — Grok Bot App avatars (PO 2026-09-29)

**Não** usar o pentágono vermelho geométrico. Usar blobs do app:

| Bot | Forma | Cor aprox. |
|-----|-------|------------|
| Groku CEO (HOME default) | círculo + bolinha inferior-direita | `#7AD35A` / dot `#4FA83A` |
| EspForge | hexágono | `#2EC4B6` |
| EngDir | gota/teardrop | `#E23B3B` |
| PrintMaker | squircle | `#F0A020` |
| Fab QA | pill horizontal | `#6EC8F0` |
| Token Maxxing | gota | laranja |
| Fab DFM | círculo | `#F05AA8` |

Olhos: dois traços inclinados escuros (\\ \\), não pontos.
Anims app-like: idle bob+blink; working bounce+anel; confirm pop+badge check; error shake+badge X; fleet mini-blobs nos cards.

Ref screenshot: `docs/ui-mocks/ref-grokbot-app-avatars.jpg`
Projeção: `proj-app-*-v0.4` / `proj-app-contact-v0.4.png`
