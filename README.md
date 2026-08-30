# Lifenvader

Windows desktop app for the LIFENVADER panels from the Figma file
*FUTURE SERVICE – ECHO DEV*. A native C++ host renders the UI in an embedded
WebView2, so the design stays pixel-accurate while all logic lives in C++.

```
┌─────────────────────────────────────────┐
│ lifenvader.exe   (Win32 + WebView2)     │
│                                         │
│  src/App.cpp        window + webview    │
│  src/Bridge.cpp     JSON message router │
│  src/backend/       your business logic │
│         ▲                               │
│         │  postMessage / PostWebMessage │
│         ▼                               │
│  ui/    HTML + CSS + JS  (the design)   │
└─────────────────────────────────────────┘
```

## Build

Requires **Windows** and **Visual Studio 2022** with the *Desktop development
with C++* workload. CMake fetches the WebView2 SDK and nlohmann/json on the
first configure, so the machine needs internet access once.

```bat
cmake -B build -S . -A x64
cmake --build build --config Release
build\Release\lifenvader.exe
```

The build copies `ui/` next to the executable. Ship the `.exe` together with
that folder.

End users need the **WebView2 Runtime**. It is preinstalled on Windows 11 and
current Windows 10; otherwise grab the Evergreen Bootstrapper from
<https://developer.microsoft.com/microsoft-edge/webview2/>.

### Developing the UI without building

`ui/` is a plain static site. Serve it and iterate in any browser:

```bash
cd ui && python3 -m http.server 8791
```

`ui/js/bridge.js` detects the missing WebView2 host and answers every backend
call from its built-in mock, so the screens stay usable.

## Log in

The placeholder account is **`demo` / `demo`**.

Replace `AuthService::VerifyCredentials()` in
`src/backend/AuthService.cpp` with real authentication — it is the single
place that decides whether a login succeeds. The current version compares
plain text, which is fine for a stub and unacceptable in production: hash
passwords (argon2/bcrypt) or delegate to a server that does.

## The C++ ↔ JavaScript bridge

Messages are JSON.

| Direction | Shape |
|---|---|
| Request  | `{"id":"42","channel":"auth:login","payload":{…}}` |
| Response | `{"id":"42","ok":true,"data":{…}}` |
| Error    | `{"id":"42","ok":false,"error":"…"}` |
| Push     | `{"channel":"feed:updated","data":{…}}` |

From JavaScript:

```js
const user = await bridge.call('auth:login', { username, password, remember });
bridge.on('feed:updated', data => { /* … */ });
bridge.window('close');   // 'close' | 'minimize' | 'drag'
```

Adding a backend call takes two edits: handle the channel in
`Bridge::Dispatch()` (`src/Bridge.cpp`) and call it via `bridge.call()`.

### Channels

| Channel | Purpose |
|---|---|
| `auth:login` | Verify credentials, returns the user |
| `auth:rememberedUser` | Username stored by a previous “remember me” |
| `auth:logout` | Clear the session |
| `feed:adverts` | Dashboard advert list |
| `feed:quest` | Daily quest |
| `window:command` | close / minimize / drag (fire-and-forget) |

## Layout of the UI

`ui/css/tokens.css` mirrors the Figma variables — colours, fonts, radii,
shadows. Figma token names are kept in comments, so `--violet-55` is
traceable to `color/violet/55` (#8A2DEB).

Each screen is authored at its exact Figma size (login: 768 × 530) and
`ui/js/app.js` scales the whole stage to fit the window. Proportions
therefore match the design at any window size.

## Fonts

`Rajdhani` is bundled (SIL Open Font License).

`Estricta` and `TT Octosquares Trl Cnd` are **commercial** and are not
included. Drop the licensed files into `ui/fonts/` as
`estricta-regular.woff2` and `tt-octosquares-cnd-bold.woff2`; the
`@font-face` rules in `tokens.css` pick them up automatically. Until then the
stack falls back to Rajdhani, which changes the look of the wordmark and the
interface copy slightly.

## Status

| Screen | Figma node | State |
|---|---|---|
| Login | `6:438` | Done |
| Dashboard | `4025:2742` | Frame + branding only — needs the full design |
| Products / News | `6:694` | Not started |

The dashboard and products screens could not be pulled from Figma
(`get_design_context` was blocked by a permission check), so only the login
screen is built from real design data. The router, the panel chrome and the
`feed:*` backend calls those screens need are already in place.

Two deliberate deviations from the Figma node, both leftovers from the design
this frame was copied out of:

- The `0$` labels on the right edge of the username and password rows
  (`span#symbolTotal`, `span#priceTotal`) are omitted — a price readout in a
  login form is a copy-paste artefact, not intent.
- The clipped `Bank` / `Bargeld` buttons below the visible container area are
  omitted for the same reason.

Add them back in `ui/index.html` if they turn out to be wanted.
