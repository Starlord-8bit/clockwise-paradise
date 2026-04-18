---
description: Reference for the widget runtime contract in Clockwise Paradise. Covers the CWWidgetManager lifecycle, widget spec strings, activation via HTTP and MQTT, auto-return, and the rules a new widget must follow to avoid blocking loop() or leaking heap.
---

# Widget Contract — Clockwise Paradise Edition

Authoritative files:
- [firmware/lib/cw-commons/widgets/clockface/CWWidgetManager.h](../../firmware/lib/cw-commons/widgets/clockface/CWWidgetManager.h)
- [firmware/lib/cw-commons/widgets/clockface/IClockface.h](../../firmware/lib/cw-commons/widgets/clockface/IClockface.h)
- [firmware/src/app/widgets/ClockCore.cpp](../../firmware/src/app/widgets/ClockCore.cpp) — reference widget

## The widget model

A widget is a small state machine:
- `onEnter()` — called once when this widget becomes active. Allocate resources here (safely, once).
- `tick()` — called every `loop()` cycle. Must not block, must not allocate. Short draw + state updates.
- `onExit()` — called once when a different widget is about to be activated. Release transient resources.

`CWWidgetManager` owns the active widget pointer. `appState.widgetManager.update()` ticks from `loop()`.

## Widget spec string

Activation goes through a string: `<id>` or `<id>:<arg>`.
- `clock` — resume the configured clockface
- `timer:120` — run a 120-second timer
- (planned) `weather`, `stocks:AAPL`, `notification:<text>`

The parser lives in the widget manager / command dispatch. When you add a new widget:
1. Pick a stable short `id`. Lowercase, no spaces, no `:`.
2. If the widget takes an argument, the dispatch code must parse it safely (length-bounded, type-checked).
3. Register the widget so the manager can map `id` → instance.

## Activation surface

Two surfaces, identical semantics:

- **HTTP:** `POST /api/widget/show?spec=<id[:arg]>` and legacy `POST /set?activeWidget=<id[:arg]>`
- **MQTT:** `<prefix>/<mac>/set/widget` with payload `<id[:arg]>`

Do not invent a third. A new widget is reachable via both without additional plumbing, so long as the widget manager's spec parser knows its id.

State readout:
- `GET /api/widgets` — list registered widgets.
- `GET /api/widget-state` — current widget id, remaining time (if timed), last-command status.

## Auto-return

Some widgets are ephemeral (timer, notification). When they expire, the manager switches back to the current clockface widget (configured in NVS). This is the contract: widgets do not delete themselves; they tell the manager to swap.

Pattern:
```cpp
if (isExpired()) {
  manager.showClockface(appState.currentFace);
  return;
}
```

Do not leave an expired widget running but hiding its output. It pollutes the heap over time.

## Hard rules for new widgets

- **RT-1** — `tick()` must not block. If your widget fetches data, the fetch is initiated with a bounded timeout and polled non-blockingly. Long synchronous HTTP inside `tick()` is rejected.
- **RT-2** — No heap growth in `tick()`. Allocate in `onEnter()`, release in `onExit()`.
- **Widget-owned state** — do not park widget state in `AppState` globals beyond what already exists. If your widget has nontrivial state, hold it on the widget instance.
- **Render only to the shared panel** — call through `state.display` or `Locator`; never construct a second `MatrixPanel_I2S_DMA`.
- **Failure modes** — if your widget cannot do its job (no data, bad config), render a clear visible fallback (e.g., a small error glyph) and auto-return after a short delay. Silent failure is worse than visible failure.

## Testing a widget

- Pure state-machine logic (timers, parsing specs, auto-return conditions) belongs in `cw-logic/` as pure C++, with Unity tests in `firmware/test/test_native/`.
- Rendering is hardware-only verification: document what the user should see on the panel after activation.
- Always test activation round-trip (HTTP + MQTT) for a new widget before marking the Task Contract done.

## Deleting / renaming a widget

- Removing an id breaks anyone who had it bookmarked or automated. Deprecate: accept both old and new id for at least one minor version.
- Changing the auto-return behavior (e.g., "returns to clock after 10 s" → "sticks until replaced") is a semantic change and warrants a mention in the PR body. HA automations may rely on the previous behavior.

## Pitfalls seen in practice

- Widget keeps an `HTTPClient` instance as a member and never calls `.end()` — heap leak after hundreds of ticks.
- Widget uses `String` concatenation for the label redrawn every tick — fragmentation after minutes.
- Widget pumps its own mini "network task" via `delay()` loops — blocks `loop()`, triggers watchdog.
- Widget assumes `state.display` is non-null before `setup()` completes — crashes during a rare early activation.
