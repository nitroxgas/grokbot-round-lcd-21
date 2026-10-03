# MQTT e webhook — grokbot-round-lcd-21

Standalone. Broker/Grok Bot → MQTT → ecrã. Touch → HTTP POST → Grok Bot. Sem HA. Sem Awtrix.
Sem segredos neste ficheiro: tudo o que identifica a pessoa (broker, tópico, URL, token) vive em
`include/secrets.h` (gitignored, compilado no binário dessa pessoa). WiFi **não** está em
`secrets.h`: WiFiManager abre o portal no 1º boot e a password fica em NVS.

## `secrets.h`

```bash
cp include/secrets.h.example include/secrets.h
```

| Campo | Uso |
|-------|-----|
| `WEBHOOK_BASE` | Base da URL do POST (sem `/` final). Vazio → **não há POST**. |
| `WEBHOOK_TOKEN` | Opcional. Enviado como `Authorization: Bearer <token>`. |
| `MQTT_HOST` / `MQTT_PORT` | Broker. |
| `MQTT_TLS` | `1` = `WiFiClientSecure`, `0` = `WiFiClient`. Sem CA embutida (encriptação, sem pinning). |
| `MQTT_USER` / `MQTT_PASS` | Credenciais. |
| `MQTT_TOPIC` | O **único** tópico subscrito. |

Se `MQTT_HOST`, `MQTT_USER`, `MQTT_PASS` ou `MQTT_TOPIC` estiver vazio o dispositivo **não liga**
ao broker (sem broker anónimo). Sem `secrets.h` o firmware compila com tudo vazio (avisa no build)
e fica só com UI/touch/buzzer.

## Entrada (`mqttLoop` → `mqttApply`)

Um `subscribe` em `MQTT_TOPIC`; connect/subscribe/reconnect automáticos (5 s) enquanto houver WiFi.
O payload é um dos quatro corpos abaixo. Não há envelope: o corpo é reconhecido pelos campos,
na ordem `anim` → `state` → `text` → `title`/`message`.

| Campos | Efeito |
|--------|--------|
| `title` / `message` | HOME |
| `state` | muda estado |
| `slot` + `text` (ou só `text`) | FLEET_STATUS |
| `anim` (+ `slot` opcional) | anima `idle`/`working`/`done`/`error` |

Estados: `BOOT`, `HOME`, `FLEET_STATUS`, `TOUCH_CONFIRM`, `WORKING`, `DONE`, `ERROR`.

### Exemplos (publicar em `MQTT_TOPIC`)

```json
{"title":"Grok Bot","message":"idle"}
```

```json
{"state":"WORKING"}
```

```json
{"slot":"A","text":"online"}
```

```json
{"anim":"working","slot":"B"}
```

## Saída (`webhookFire` → `webhookPost`)

`POST <WEBHOOK_BASE><path>` com `Content-Type: application/json` (e `Authorization: Bearer` se houver token),
timeout 3 s. Só dispara com WiFi ligado e `WEBHOOK_BASE` não vazio.

```json
{"source":"round-lcd-21","event":"SLOT_A","slot":"A"}
```

Paths: `/slot/a` `/slot/b` `/slot/c` `/action/primary` `/action/back` `/action/wifi`.

## Reserva

Poll HTTP ao webhook em intervalo longo: **desligado por defeito**, não é o caminho normal. Só a
considerar se MQTT não for viável para alguém.
