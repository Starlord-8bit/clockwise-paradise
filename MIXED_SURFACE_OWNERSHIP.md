# Mixed Surface Ownership

Use this note when one feature touches both the web UI surface and firmware behavior.

## Decision Rule

- `frontend` owns HTML, JavaScript, and CSS in `SettingsWebPage.h` and `CWWebUI.h`.
- `coder` owns local device state, preferences, HTTP endpoints, and non-network backend logic.
- `connectivity` owns WiFi, MQTT, Home Assistant discovery, OTA, and any change where network semantics or integration contracts change.

Short version: frontend for presentation, coder for local device state and API wiring, connectivity when the task changes network behavior instead of only exposing an existing local setting.

## Normal Sequence

1. If the feature needs new backend, endpoint, preference, validation, or connectivity semantics, dispatch `coder` or `connectivity` first.
2. Dispatch `frontend` after the backend contract is approved and the UI can wire to a stable setting or endpoint shape.
3. Run the reviewer flow for each specialist handoff that requires review.
4. Land the approved scopes on one branch and one PR.

## How To Split A Mixed Task

- Keep one feature branch and one coordinating Task Contract.
- Split acceptance criteria by specialist so each agent has a testable slice.
- Split in-scope files by specialist so ownership is explicit.
- Do not collapse firmware, connectivity, and UI work into one vague checklist item.

## Routing Examples

- New local setting with a settings card: `coder` first, then `frontend`.
- Existing local setting exposed in a new UI card only: `frontend`.
- New MQTT or WiFi behavior with a settings card: `connectivity` first, then `frontend`.
- UI copy or layout change with no backend effect: `frontend`.