# macOS model preview

The macOS artifact is an AppKit documentation preview backed by
`fireball/browser/domain_model.cc`. It proves that the current Profile, Space,
Tab, Burner Space, Favorite/Pinned/Today placement, archive policy,
Loaded/Discarded residency, and `TabLayout` state can drive four presentations
without mutating tab identity.

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

Click `CLASSIC`, `FLOATING`, `VERTICAL`, or `GRID` to change presentation, or
use keys `1`–`4` and the left/right arrow keys. The same in-memory
`BrowserModel` and tab IDs remain attached.

Click `TRANSFER 02` or press `D` to toggle the Transfer Deck. The drawer uses
the production `TransferQueue` state machine with a deterministic preview
backend: one direct-video job is active and one torrent is paused. It proves UI
mapping and privacy labels only; the real aria2 process is exercised separately
by `make check`.

The Vertical layout renders the Arc-inspired Favorite/Pinned/Today hierarchy.
The Safari Floating layout uses a unified location surface and floating tab
chrome. Grid exposes the residency policy, including a deterministic discarded
tab that restores when activated. These are model states rather than measured
renderer-memory savings; the Chromium adapter and benchmark remain B0 work.

## Reproduce repository screenshots

```sh
make macos-preview-media
```

The capture mode renders the exact same AppKit view offscreen at 1440×900. The
four layout PNGs and Transfer Deck PNG under `docs/assets/` are therefore
reproducible build artifacts, not hand-authored mockups.

## Promotion boundary

A screenshot from this preview can document layout direction only. The B0 gate
still requires an exact Chromium stable checkout, an upstream control build,
the Fireball overlay build, smoke tests, startup-network capture, artifact
checksum, and security-rebase evidence. Replace the preview with Chromium Views
screenshots only after those checks pass.
