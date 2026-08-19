# macOS model preview

The macOS artifact is an AppKit documentation preview backed by
`fireball/browser/domain_model.cc`. It proves that the current Profile, Space,
Tab, Burner Space, and `TabLayout` state can drive four presentations without
mutating tab identity.

It is intentionally **not** a Fireball Blink browser build. It contains no
Chromium checkout, `content::WebContents`, Chromium Profile adapter, renderer,
sandbox, extension system, URL cleaner, or adblock engine. The UI repeats that
boundary so screenshots cannot be mistaken for B0/B3 release evidence.

## Build and run

Requirements: macOS 13 or newer, Xcode Command Line Tools or Xcode, and the
macOS SDK.

```sh
make macos-preview
open "out/macos-preview/Fireball Blink Preview.app"
```

Click `CLASSIC`, `FLOATING`, `VERTICAL`, or `GRID` to change presentation. The
same in-memory `BrowserModel` and tab IDs remain attached.

## Reproduce repository screenshots

```sh
make macos-preview-media
```

The capture mode renders the exact same AppKit view offscreen at 1440×900. The
four PNGs under `docs/assets/` are therefore reproducible build artifacts, not
hand-authored mockups.

## Promotion boundary

A screenshot from this preview can document layout direction only. The B0 gate
still requires an exact Chromium stable checkout, an upstream control build,
the Fireball overlay build, smoke tests, startup-network capture, artifact
checksum, and security-rebase evidence. Replace the preview with Chromium Views
screenshots only after those checks pass.
