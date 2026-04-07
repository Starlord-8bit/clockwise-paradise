---
description: Reference for Clockwise Paradise web UI design patterns. Use /frontend-design when adding or modifying settings cards, JS logic, or HTML in SettingsWebPage.h.
---

# Clockwise Paradise — Frontend Design Guidelines

## Architecture Reminder

No build step. No npm. No bundler. The entire UI is a single raw string constant in
`firmware/lib/cw-commons/SettingsWebPage.h`:

```cpp
const char SETTINGS_PAGE[] PROGMEM = R""""(
<!DOCTYPE html>
...all HTML, CSS, JS in one string...
)"""";
```

**Do not use `"""`** anywhere inside the string — it terminates the C++ raw string delimiter.
Use `&quot;` for HTML attribute quotes if needed, or single quotes `'`.

---

## Styling

| Need | Use |
|------|-----|
| Layout | W3.CSS grid classes (`w3-row`, `w3-col`, `w3-half`) |
| Cards | `.w3-card-4`, `.w3-padding` |
| Colors | W3.CSS color classes (`w3-blue`, `w3-green`, `w3-red`) |
| Icons | FontAwesome 4.x (`<i class="fa fa-icon-name"></i>`) |
| Custom styles | **Not allowed** — no new `<style>` blocks |
| New CDN libs | **Not allowed** — only W3.CSS and FontAwesome |

---

## Card System

Every setting is a JavaScript object in the `createCards()` array:

```js
{
  title:       "Setting Name",
  description: "One sentence explaining what this does.",
  formInput:   "<input type='range' id='myId' min='0' max='100'>",
  icon:        "fa-sliders",           // FontAwesome icon class
  save:        "updatePreference('nvs_key', myId.value)",
  property:    "nvs_key"               // must exactly match the API JSON key
}
```

All six fields are required. Missing any one will break the card renderer.

### ID Rules

- The `id` in `formInput` must be **unique across the entire page**.
- The same `id` must appear in the `save` expression.
- Use descriptive, camelCase IDs: `nightStart`, `colorOrder`, `ledBrightness`.
- Before choosing an ID, grep the file:
  ```bash
  grep -n "id='" firmware/lib/cw-commons/SettingsWebPage.h
  ```

### Property Key Rules

- `property` must **exactly** match the JSON key the API returns (case-sensitive).
- Verify against `CWWebServer.h` before writing the card.
- The frontend injects the current value via: `settings.<property>` in string concatenation.

---

## Input Types

| Setting type | Recommended input |
|-------------|-------------------|
| Number range | `<input type='range' id='x' min='N' max='M'>` |
| On/Off toggle | `<input type='checkbox' id='x'>` |
| Dropdown | `<select id='x'><option value='0'>...</option></select>` |
| Time (HH:MM) | `<input type='time' id='x'>` |
| Text | `<input type='text' id='x' maxlength='N'>` |

---

## updatePreference Pattern

All settings are saved via:
```js
updatePreference('nvs_key', value)
```

- First arg: NVS key string (must match `property` field and the backend endpoint).
- Second arg: the value — get from `id.value` (string) or `id.checked` (boolean).

For checkboxes:
```js
save: "updatePreference('nightMode', nightMode.checked ? 1 : 0)"
```

---

## Security Rules (non-negotiable)

- Never assign `innerHTML` from API response data — use `textContent` or explicit DOM nodes.
- Never use `eval()`, `new Function()`, or string-based event handlers.
- Never use `setAttribute('onclick', string)`.
- These rules exist because the settings page runs on the local network but could be
  accessed by any device on that network.

---

## Known Issues in the Current File

- **`autoChange` duplicate**: the `autoChange` property appears twice in the cards array.
  If you are touching the cards array for any reason, fix this duplicate and note it in
  your Handoff Contract. Do not introduce additional duplicates.

---

## Checking Your Work

Before handoff, always run:

```bash
# Extract JS and validate syntax
python3 - <<'EOF'
import re, sys
with open('firmware/lib/cw-commons/SettingsWebPage.h') as f: content = f.read()
match = re.search(r'R""""\((.*?)\)""""', content, re.DOTALL)
html = match.group(1)
scripts = re.findall(r'<script>(.*?)</script>', html, re.DOTALL)
open('/tmp/cw_check.js','w').write('\n'.join(scripts))
EOF
node --check /tmp/cw_check.js && echo OK || echo FAIL
```

Any syntax error will crash the entire settings page on the device.
