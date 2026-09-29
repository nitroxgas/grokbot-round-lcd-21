# grokbot-round-lcd-21

Companheiro circular standalone para o **Grok Bot** na Waveshare **ESP32-S3-Touch-LCD-2.1** (Ø75 · 480×480 touch).

- MQTT → UI · touch → webhook · buzzer feedback  
- **Sem Home Assistant · sem Awtrix**  
- Spec: [SPEC.md](./SPEC.md) · HW: [docs/HW-LEAD-PACK.md](./docs/HW-LEAD-PACK.md)

Scaffold v0.1 — não é firmware completo. **Não** fazer `pio run -t upload` sem ok do George.

## Abrir no Cursor

File → Open Folder nesta raiz (`platformio.ini`, `SPEC.md`, `src/`).

## Build (quando o env existir)

```bash
pio run -e waveshare_round_21
```

## Secrets

```bash
cp include/secrets.h.example include/secrets.h
```

`secrets.h` no `.gitignore`.

## Repo

GitHub público — URL a preencher após create.
