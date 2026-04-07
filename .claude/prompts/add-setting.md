---
description: Template for adding a new NVS device setting end-to-end: CWPreferences struct, load/save, HTTP endpoint, and Settings UI card. Use this prompt when implementing any new configurable parameter.
---

# Task: Add New Device Setting

## Setting Specification

Fill in before dispatching to the coder + frontend agents:

```
Setting name (human-readable): _______________
NVS key (≤ 15 chars, snake_case): _______________
Data type: [ ] int  [ ] bool  [ ] String  [ ] float
Default value: _______________
Valid range / allowed values: _______________
HTTP endpoint path: /api/_______________
UI card title: _______________
UI card description: _______________
UI input type: [ ] range  [ ] checkbox  [ ] select  [ ] text  [ ] time
Card icon (FontAwesome): fa-_______________
Card group (which section of the settings page): _______________
```

---

## Coder Agent Scope (type: firmware)

Implement all four required locations in order:

### 1 — CWPreferences.h: Struct field
Add to the `Preferences` struct:
```cpp
<type> <fieldName> = <defaultValue>;
```

### 2 — CWPreferences.h: Load
Add to `loadPreferences()`:
```cpp
prefs.<fieldName> = nvs.get<Type>("<nvsKey>", <defaultValue>);
```

### 3 — CWPreferences.h: Save
Add to `savePreferences()`:
```cpp
nvs.put<Type>("<nvsKey>", prefs.<fieldName>);
```

### 4 — CWWebServer.h: HTTP endpoints
```cpp
// GET /api/<endpoint>
if (method == "GET" && path == "/api/<endpoint>") {
    sendJson(client, "{\"value\":" + String(prefs.<fieldName>) + "}");
    return;
}
// POST /api/<endpoint>
if (method == "POST" && path == "/api/<endpoint>") {
    String body = readBody(client);
    prefs.<fieldName> = body.<toType>();
    savePreferences();
    sendJson(client, "{\"status\":\"ok\"}");
    return;
}
```

**Test cases required:**
- Verify default value is returned on first boot (NVS not set)
- Verify value persists after save + reload
- Verify out-of-range input is rejected (if applicable)

---

## Frontend Agent Scope (type: frontend)

Add one card to `createCards()` in `SettingsWebPage.h`:

```js
{
  title:       "<UI card title>",
  description: "<UI card description>",
  formInput:   "<input type='<type>' id='<uniqueId>' <attributes>>",
  icon:        "fa-<icon>",
  save:        "updatePreference('<nvsKey>', <uniqueId>.value)",
  property:    "<nvsKey>"
}
```

**Checklist:**
- [ ] `id` is unique across all cards (grep before choosing)
- [ ] `property` matches the exact JSON key from the GET endpoint
- [ ] `save` expression references the correct `id`
- [ ] Card is placed in the correct group
- [ ] `autoChange` duplicate fixed if touching the cards array

---

## Coordinator Notes

- Dispatch **coder first** — the frontend agent cannot verify Check 3 until the endpoint exists.
- Frontend Check 3 will be BLOCKED until the coder agent's PR is reviewer-approved.
- After both contracts pass review, the GitHub Specialist commits both sets of changes
  in a single commit or two sequential commits on the same branch.
