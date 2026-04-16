# cw-commons

Clockwise common resources, organized by responsibility.

## Folder layout

- `widgets/clockface/` - clockface widget contracts and orchestration (`IClockface`, `CWClockfaceDriver`, `CWWidgetManager`)
- `connectivity/` - network and transport (`WiFiController`, `CWHttpClient`, `CWMqtt`, `CWOTA`)
- `core/` - shared runtime/configuration primitives (`CWDateTime`, `CWPreferences`)
- `display/` - display assets and status rendering (`Icons`, `picopixel`, `StatusController`)
- `web/` - web server/UI resources (`CWWebServer`, `CWWebUI`, `SettingsWebPage`)
- `pages/` - page render fragments used by `CWWebServer`

The `widgets/` category is the expansion point for additional widget modules (for example
weather or stocks) without changing the top-level library layout.

## Includes

Use direct categorized include paths (for example `web/CWWebServer.h`,
`widgets/clockface/IClockface.h`, `core/CWPreferences.h`).
