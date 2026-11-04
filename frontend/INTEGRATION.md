# Integrating the ed-utils logger settings UI

`edu-log-ui` is a single self-contained ESM bundle that defines the API client
factory and the `<edu-log-settings-page>` custom element on import:

```ts
import 'edu-log-ui';
```

It provides a small form for the device's network-logger target: host, URI,
port and the enabled flag. The element performs client-side sanity checks and
the device remains the authority on every value.

## Device endpoints

The defaults match the logger settings handler in `src/log/log_handlers.hpp`.
The path and the base URL can be overridden with attributes (see the reference
table below).

| Method | Path | Request | Response |
| --- | --- | --- | --- |
| `GET` | `/api/log/settings` | `Accept: application/json` | `200` `{"host":"…","uri":"…","port":8080,"enabled":true}` |
| `POST` | `/api/log/settings` | JSON body `{"host":"…","uri":"…","port":8080,"enabled":true}`, `Content-Type: application/json` | `200` echoing the **current** state (same shape); `422` with `{"message":"…"}` on field-validation failure; `500` with `{"message":"failed to store settings"}` when the firmware persistence callback fails (applied values stay active); bare `400` (empty/malformed JSON) and bare `413` (body over 1024 bytes), both without a `{"message":"…"}` body |

Field constraints: `host` and `uri` are non-empty strings shorter than
`LOG_HOST_LEN` (64), so at most 63 characters each; `port` is an integer in
`1..65535`; `enabled` is a boolean. Known device error messages, all sent as
`{"message":"…"}` with status `422`, include `invalid json`,
`host is required`, `host too long`, `uri is required`, `uri too long`,
`invalid port`, `invalid enabled` and `invalid settings`. A `500` with
`{"message":"failed to store settings"}` is returned when the firmware
persistence callback reports failure; the applied values remain active in that
case. Malformed or empty JSON is rejected by ESPAsyncWebServer with a bare
`400` and bodies over 1024 bytes with a bare `413` — neither carries a
`{"message":"…"}` body.

Persistence depends on the firmware: a consumer can register an `onApplied`
callback that stores the settings. When no callback is registered, the values
live in `NetworkLogger`'s runtime state only and are not persisted across
reboots. The device does **not** reboot after a save.

## Bundle budget

Measured with `npm run build` (Vite 8, esbuild minify, lit 3 bundled in):

| Build | Raw | Gzip |
| --- | --- | --- |
| `dist/edu-log-ui.js` (client + settings page) | 30,226 B (30.22 kB) | 9,431 B (9.43 kB) |

Bundling `edu-log-ui` into a host adds roughly **9.4 kB gzip**. If the host is
also served from LittleFS, budget that against the filesystem partition.

---

## (a) Primary path — bundle into a controller SPA (e.g. Prometey)

`ed-utils` sits next to the controller repo. Install the local package from the
controller's frontend directory:

```bash
cd <controller>/frontend
npm install file:../../ed-utils/frontend
```

This installs the built `dist/edu-log-ui.js` + `dist/types/**` and wires the
package `types` field. `dist/` is committed, so no Node build step is needed on
the consumer side. The package is `private` and is not published to npm.

### 1. Import once at the app entry

```tsx
// <controller>/frontend/src/main.tsx
import { render } from 'preact';
import './styles.css';
import { App } from './App';
import 'edu-log-ui'; // registers <edu-log-settings-page> globally

const root = document.getElementById('app');
if (root) {
  render(<App />, root);
}
```

### 2. Mount the page imperatively

Preact JSX does not know the custom-element tag name by default. Mount
imperatively to avoid JSX intrinsic typing (and any Preact/Lit ownership clash):

```tsx
// <controller>/frontend/src/pages/LoggerSettings.tsx
import { useEffect, useRef } from 'preact/hooks';
import 'edu-log-ui';

export function LoggerSettings() {
  const host = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const element = document.createElement('edu-log-settings-page');
    element.setAttribute('device-name', 'My Controller');
    // Same-origin by default. Point at another device only if that device
    // serves CORS: element.setAttribute('base-url', 'http://<device>');
    host.current?.append(element);
    return () => element.remove();
  }, []);

  return <div ref={host} />;
}
```

Listen for `edu-log-saved` if the host wants to react to a successful save.
The event detail is the device's echoed current settings:

```ts
element.addEventListener('edu-log-saved', (event) => {
  const settings = (event as CustomEvent<{
    host: string;
    uri: string;
    port: number;
    enabled: boolean;
  }>).detail;
  console.log('logger settings saved:', settings);
});
```

### If you prefer JSX tags

Declare the intrinsic once, e.g. `<controller>/frontend/src/edu-log.d.ts`:

```ts
import 'preact';

declare module 'preact' {
  namespace JSX {
    interface IntrinsicElements {
      'edu-log-settings-page': {
        'base-url'?: string;
        'settings-path'?: string;
        'device-name'?: string;
        'max-host-length'?: string;
      };
    }
  }
}
```

You can then write `<edu-log-settings-page device-name="My Controller" />`
directly.

---

## (b) Standalone path — LittleFS static page

Copy the built bundle next to a page:

```bash
cp ed-utils/frontend/dist/edu-log-ui.js <controller>/data/edu-log-ui.js
```

Create `<controller>/data/logger.html`:

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Logger settings</title>
  </head>
  <body>
    <edu-log-settings-page device-name="My Controller"></edu-log-settings-page>
    <script type="module" src="/edu-log-ui.js"></script>
  </body>
</html>
```

Serve it from the LittleFS root (the controller's static-file handler decides
the default document). Upload with the controller's filesystem upload target,
for example:

```bash
pio run -e <env> -t uploadfs
```

---

## Attribute reference

| Attribute | Property | Default | Meaning |
| --- | --- | --- | --- |
| `base-url` | `baseUrl` | `''` | Prefix for every request; empty means same origin |
| `settings-path` | `settingsPath` | `/api/log/settings` | Settings endpoint (GET + POST) |
| `device-name` | `deviceName` | `logger` | Name interpolated into the heading and notes |
| `max-host-length` | `maxHostLength` | `63` | Client-side sanity cap for host/uri (`LOG_HOST_LEN - 1`) |

The element registers `edu-log-settings-page` once at module scope and is safe
to import from more than one entry point. On connect it loads the current
settings with `GET`; the **Reload** button re-fetches them. **Save** validates
the form, sends trimmed values, and on success refreshes the form from the
device echo, shows a `Settings saved.` banner and dispatches `edu-log-saved`.

## Security notes

- The logger settings API has **no authentication and sends no CORS headers**.
  Serve the UI same-origin (the default `base-url=""`) on a trusted LAN. Do not
  expose the device to untrusted networks or the public internet, and do not
  ship a cross-origin deployment without adding auth/CORS on the device.
- The host/URI pair controls where logs are sent. Treat it as sensitive
  configuration and do not publish device-specific values.
- The 63-character cap and the port range are client-side sanity checks, not
  the security boundary. The firmware validates every field again and rejects
  the whole request with `{"message":"…"}` when anything is invalid.
- Saving never reboots the device, so no health polling is needed.
