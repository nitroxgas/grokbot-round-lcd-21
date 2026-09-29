# MQTT e webhook — grokbot-round-lcd-21

Standalone. Broker/Grok Bot → MQTT → ecrã. Touch → HTTP → Grok Bot. Sem HA. Sem Awtrix.
Sem segredos neste ficheiro.

## Entrada (`mqttApply`)

| Tópico | Efeito |
|--------|--------|
| `grokbot/round/ui/home` | `title`/`message` → HOME |
| `grokbot/round/ui/state` | `state` |
| `grokbot/round/status` | FLEET_STATUS |
| `grokbot/round/anim` | anima `idle`/`working`/`done`/`error` |

Estados: `BOOT`, `HOME`, `FLEET_STATUS`, `TOUCH_CONFIRM`, `WORKING`, `DONE`, `ERROR`.

### Exemplos

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

## Saída (`webhookFire`)

```json
{"source":"round-lcd-21","event":"SLOT_A","slot":"A"}
```

Paths: `/slot/a` `/slot/b` `/slot/c` `/action/primary` `/action/back` `/action/wifi`.
