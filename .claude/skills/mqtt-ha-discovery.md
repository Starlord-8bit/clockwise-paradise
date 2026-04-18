---
description: Reference for MQTT + Home Assistant Discovery conventions in Clockwise Paradise. Covers topic shape, availability / LWT, entity mapping (switch / number / select / sensor), discovery payload schema, and the rules for adding or changing an entity without breaking the HACS integration.
---

# MQTT + HA Discovery — Clockwise Paradise Edition

Wrappers:
- [firmware/lib/cw-commons/connectivity/CWMqtt.h](../../firmware/lib/cw-commons/connectivity/CWMqtt.h) — broker client, pump, publish, subscribe
- [firmware/lib/cw-commons/connectivity/CWMqttDiscovery.h](../../firmware/lib/cw-commons/connectivity/CWMqttDiscovery.h) — HA Discovery payloads
- [firmware/lib/cw-commons/connectivity/CWMqttCommands.h](../../firmware/lib/cw-commons/connectivity/CWMqttCommands.h) — command topic dispatch

Companion integration: [Starlord-8bit/ha-clockwise](https://github.com/Starlord-8bit/ha-clockwise). The firmware is the source of truth for entities; the HACS integration mirrors what firmware advertises.

## Topic shape

| Role | Topic pattern | Retained | Notes |
|---|---|---|---|
| Availability | `<prefix>/<mac>/availability` | yes | `online` / `offline`; LWT publishes `offline` |
| State (per property) | `<prefix>/<mac>/<property>/state` | yes | JSON or scalar per entity |
| Command (per property) | `<prefix>/<mac>/set/<property>` | no | Payload shape depends on entity |
| Widget control | `<prefix>/<mac>/set/widget` | no | `clock`, `timer:120`, … |
| Discovery | `homeassistant/<component>/<uniq_id>/config` | yes | Published once on connect |

Defaults:
- `<prefix>` = `clockwise` (user-configurable)
- `<mac>` = device MAC (lowercase, no separators)
- Discovery prefix = `homeassistant` (HA default)

## Availability + LWT — always

Every MQTT session registers an LWT on `<prefix>/<mac>/availability` = `offline`. On connect, the firmware publishes `online` retained. Every discovery payload references the same availability topic. Without this, HA shows entities as available even after the device crashes.

## Adding an entity — checklist

1. Pick an HA component that fits the data shape:
   - `switch` — 2-state on/off
   - `binary_sensor` — read-only 2-state
   - `number` — integer/float with range
   - `select` — enum
   - `sensor` — read-only scalar/string
   - `button` — stateless trigger
2. `uniq_id` = `clockwise_<mac>_<property>` — stable across reboots and firmware upgrades.
3. Command topic if writable: `<prefix>/<mac>/set/<property>`
4. State topic: `<prefix>/<mac>/<property>/state`, retained, updated whenever the value changes.
5. Availability: reuse the device availability topic.
6. Device block: must match the device advertised by all other entities so HA groups them under one device:
   ```json
   "dev": { "ids": ["clockwise_<mac>"], "name": "Clockwise Paradise", "sw": "<fw-version>" }
   ```

## Example payloads

### `number` — display brightness
```json
{
  "name": "Brightness",
  "uniq_id": "clockwise_<mac>_brightness",
  "cmd_t":  "clockwise/<mac>/set/brightness",
  "stat_t": "clockwise/<mac>/brightness/state",
  "avty_t": "clockwise/<mac>/availability",
  "pl_avail": "online",
  "pl_not_avail": "offline",
  "min": 0, "max": 255, "step": 1,
  "dev": { "ids": ["clockwise_<mac>"], "name": "Clockwise Paradise", "sw": "3.0.4" }
}
```

### `select` — clockface
```json
{
  "name": "Clockface",
  "uniq_id": "clockwise_<mac>_clockface",
  "cmd_t":  "clockwise/<mac>/set/clockface",
  "stat_t": "clockwise/<mac>/clockface/state",
  "avty_t": "clockwise/<mac>/availability",
  "pl_avail": "online",
  "pl_not_avail": "offline",
  "options": ["mario", "pacman", "worldmap", "castlevania", "pokedex", "canvas"],
  "dev": { "ids": ["clockwise_<mac>"], "name": "Clockwise Paradise", "sw": "3.0.4" }
}
```

### `switch` — MQTT + HA toggle
```json
{
  "name": "HA Discovery",
  "uniq_id": "clockwise_<mac>_ha_disc",
  "cmd_t":  "clockwise/<mac>/set/haDiscovery",
  "stat_t": "clockwise/<mac>/haDiscovery/state",
  "avty_t": "clockwise/<mac>/availability",
  "pl_avail": "online",
  "pl_not_avail": "offline",
  "pl_on": "1", "pl_off": "0",
  "dev": { "ids": ["clockwise_<mac>"], "name": "Clockwise Paradise", "sw": "3.0.4" }
}
```

## Backward compatibility

- `uniq_id` is a durable contract with HA. Do not rename it.
- If a property's semantics change (e.g., brightness range 0-100 → 0-255), add a migration note and bump the firmware version so HA refreshes the discovery payload.
- Removing an entity leaves a ghost in HA. Publish an empty retained payload on the discovery topic to unregister.

## Secrets

Never publish broker username/password, WiFi password, or OTA tokens on any topic. Never include them in a discovery payload. Never log them.

## Rate limiting

- Do not publish on every loop tick. Publish on change only.
- Discovery payloads publish once per connect, not per loop.
- LDR / brightness readings should publish at most once per second.

## Checklist before merging an MQTT change

- [ ] Availability topic + LWT is configured for the session.
- [ ] Any new entity has a matching command handler if writable.
- [ ] `uniq_id` is stable and unique.
- [ ] Device block matches existing entities so HA groups them.
- [ ] HA integration ([ha-clockwise](https://github.com/Starlord-8bit/ha-clockwise)) still picks up the entity — check with `hass` logs after flashing.
- [ ] State publishes only on change.
- [ ] Topic length within broker limits (most brokers accept up to ~65535; keep well under 256 for legibility).
