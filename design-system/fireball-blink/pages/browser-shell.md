# Browser shell overrides

These rules override `../MASTER.md` for the four-layout model preview.

- A 78px top identity rail contains the meteor mark, product name, selected
  layout and explicit `MODEL PREVIEW` boundary.
- The left rail owns Profiles, Spaces, Burner state and startup-network policy.
- The address row owns navigation, location, Shields state and build boundary.
- Classic, Floating, Vertical and Grid are views over the same tab collection;
  switching them must not recreate or reload a tab.
- Vertical exposes the Arc-inspired hierarchy explicitly: Profile-wide
  Favorites, Space-scoped Pinned tabs, then auto-archivable Today tabs. It must
  never imply that a Favorite crosses a Profile boundary.
- Floating uses a restrained Safari-like unified toolbar and elevated tabs,
  while keeping Fireball's solid surfaces and angular orbital identity.
- `LIVE`, `SLEEP`, `ACTIVE` and `PROTECTED` are text-plus-shape states. Orange
  marks a discarded model tab; lime alone is never the only signal.
- Transfer Deck is a right-side utility drawer, never a center modal. Each job
  shows source class, explicit state, byte progress, rate and a single valid
  action. It never renders a signed URL, magnet URI or raw backend error.
- Direct media, supported finite MPEG-TS HLS VOD and supported static fMP4 DASH
  VOD use lime readiness. DRM, encrypted/live media, unsupported HLS/DASH forms
  and torrents on proxy routes use orange plus explicit gated text. WARP/Tor
  must never imply torrent privacy when peer sockets are policy-disabled.
- Grid cards use index, active/background text and URL; the active card needs a
  border plus an `ACTIVE` label, not color alone.
- The UI must continue to say `NO CHROMIUM ENGINE` and `PREVIEW · NOT A BROWSER
  BUILD` until a real pinned Chromium artifact exists.
