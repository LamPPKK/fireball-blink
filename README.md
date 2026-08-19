# fireball-blink

Buildable B0/B1 tooling foundation for the Chromium-based Fireball Browser.

<img src="Brand/FireballMeteorMark.png" width="104" alt="Fireball meteor brand mark">

The detached meteor mark is the shared Fireball identity: an obsidian core,
ember-orange flight surfaces and one electric-lime trail. Blink applies it to
an **orbital command deck**—dense desktop chrome, explicit Profile/Space
boundaries and visible provenance—without pretending this preview is a browser.

![Fireball Blink macOS tab-grid model preview](docs/assets/fireball-blink-macos-grid.png)

![Fireball Blink transfer queue model preview](docs/assets/fireball-blink-macos-transfers.png)

## macOS model preview

The repository includes a buildable AppKit preview that drives its four tab
presentations from the real C++ `BrowserModel`:

| Chromium Classic | Safari Floating |
| --- | --- |
| ![Classic tab presentation](docs/assets/fireball-blink-macos-classic.png) | ![Floating tab presentation](docs/assets/fireball-blink-macos-floating.png) |
| Vertical Sidebar | Tab Grid |
| ![Vertical tab presentation](docs/assets/fireball-blink-macos-vertical.png) | ![Tab-grid presentation](docs/assets/fireball-blink-macos-grid.png) |

```sh
make macos-preview
open "out/macos-preview/Fireball Blink Preview.app"
```

This is deliberately a **model/UI preview, not a Chromium browser build**. It
contains no Chromium checkout, WebContents, renderer, sandbox, extensions,
adblock engine, or URL cleaner. The executable and every screenshot repeat that
boundary. See [the reproducible preview and promotion rules](docs/MACOS_PREVIEW.md).

The product-specific UI tokens and component rules live in
[`design-system/fireball-blink/MASTER.md`](design-system/fireball-blink/MASTER.md),
with preview overrides in
[`pages/browser-shell.md`](design-system/fireball-blink/pages/browser-shell.md).
They adapt UI/UX Pro Max guidance to a desktop browser surface while keeping the
Brave overlay order and Helium provenance model visible in the interface.

## Architecture boundary

- Chromium Stable Linux `151.0.7922.169` and `depot_tools` are locked to exact official revisions in `pins/upstream.json`.
- Product code starts in the `fireball/` GN overlay.
- Chromium-relative overrides live in `chromium_src/` only when an overlay seam is insufficient.
- Direct patches are last-resort entries in `patches/manifest.json`.
- Every imported patch must record its source repository/path, HTTPS license URL, exact source commit, exact verified Chromium commit, milestone range, security impact, required tests and SHA-256.
- PartitionAlloc, Chromium's process model and sandbox remain intact.

`make check` validates upstream/reference pins, generated network policy and the patch manifest; exercises apply, reverse, conflict, checksum and path-traversal fixtures; compiles standalone C++ policy/domain tests; and links the C++ request pipeline to the real Rust blocker ABI. `make macos-preview-media` rebuilds the AppKit preview and deterministically regenerates all five repository screenshots. This repository does not fetch or build Chromium yet. The first full control build still needs a B0 builder with at least 8 cores, 32 GiB RAM and 300 GiB free disk; no Chromium artifact is claimed from this preview lane.

## Brave and Helium reference policy

`pins/reference-browsers.json` records the exact reviewed snapshots. Brave supplies the overlay → override → direct-patch ordering and rebase discipline. Helium supplies the vendor-sorted patch-series, pruning and download-checksum provenance patterns. Fireball does not automatically import either project's patches; every adopted change still needs its own source commit, license, Chromium range, checksum, security review and required tests.

Compiler optimization ideas from Thorium stay in a separate benchmark lane. Generic CPU builds remain the control.

## Startup network policy

`policies/startup_network.json` is default-deny and contains no allowed startup traffic. Its post-startup rules cover only user-initiated aria2, WARP and Tor activation, each with explicit consent. `tools/network_policy.py` generates the C++ table used by the overlay and CI rejects stale generated output, unknown schema fields, implicit consent or a default-allow policy. Future services must declare a stable owner, phase, user-visible purpose and explicit opt-in before a rule can be added.

## Security rebase gate

`security/rebases.json` is the evidence ledger for the 72-hour Chromium security-rebase SLA. Passing and failed attempts are both recorded so a failure breaks the consecutive-pass streak. A pass requires ordered release/triage/build/promotion timestamps, a promoted-artifact checksum, completion within 72 hours, and passing control build, overlay build, smoke tests and startup-network audit. `python3 tools/security_rebases.py status` reports the gate; it remains closed until two consecutive real passes are recorded. No placeholder success is checked in.

## Profiles, Spaces and Burner state

`fireball/browser/domain_model.*` establishes the B2 ownership boundary without
replacing Chromium objects: Profile owns persistent or off-the-record storage
identity, Space owns a tab collection and points to exactly one Profile, and
Tab has a stable UUID. Multiple regular Spaces can share a persistent Profile;
Burner Spaces require an off-the-record Profile and cannot be restored.

The Arc-inspired library now implements Profile-wide Favorites, Space-scoped
Pinned tabs, temporary Today tabs, per-Profile auto archive and same-Profile tab
moves. Its lightweight lifecycle ranks safe background discard candidates by
placement and LRU activity while protecting active, audible, capture-active and
unsaved-form tabs. The four layouts are presentation state, so switching layout
preserves every domain tab. See [the tab-management and Chromium adapter
contract](docs/TAB_MANAGEMENT.md).

This is not yet measured Helium-level memory usage. Chromium
Profile/WebContents adapters, a full isolation test and control-vs-overlay
benchmarks remain blocked on the B0 checkout/build.

## Native adblock foundation

`fireball/components/adblock` contains a real, pinned `adblock-rust` 0.13.2
engine behind a panic-contained C ABI plus a C++ per-profile policy boundary.
Network rules, exceptions, third-party matching, site-specific cosmetic rules
and generic class/ID selectors are exercised through the actual engine. The
single-thread feature is selected for this first memory-conscious lane; a
generic build remains the performance control.

Release engine creation fails closed until Chromium registers its
registry-controlled-domain resolver and a rules artifact passes bounded input,
SHA-256, Ed25519, source provenance, engine-version and minimum-app-version
checks. The unsigned constructor is compiled only for the FFI test feature.
Rules updates will build a replacement immutable engine and swap it at a safe
sequence boundary; they will not mutate an engine serving requests.

## Profile request pipeline

`fireball/components/navigation` now combines strict request validation,
versioned URL cleaning, the per-Profile blocker policy and the committed
Direct/WARP/Tor route into one deterministic decision. Main-frame `GET`
navigations remove exact tracking-parameter names while retaining raw values,
parameter order and fragments. Site exemptions remain inside their Profile.
The adblock path fails closed on a missing engine, malformed result or unknown
flags; only bounded subresource `data:` redirects and same-host, same-scheme
rewrites are accepted.

The integration gate links this C++ policy to the actual pinned Rust library
and proves block, exception, third-party and exemption behavior. See the
[request pipeline and Chromium adapter contract](docs/REQUEST_PIPELINE.md).

This is not yet full Brave Shields or a Chromium network interceptor. The B0
Chromium checkout still needs to supply trusted `GURL` fields, wire the Rust
target into GN, apply decisions in navigation/URL-loader throttles, inject
cosmetic resources through isolated worlds, and publish a real signed
EasyList/EasyPrivacy-derived artifact and embedded production key. Until those
steps land, the feature is a tested native foundation rather than a
user-visible blocker.

## Download and torrent foundation

`fireball/components/transfer` now contains the first production transfer
vertical slice. It accepts bounded HTTP(S), canonical BitTorrent v1 magnet
links and bounded `.torrent` metainfo; classifies direct audio/video and
HLS/DASH candidates; and controls a foreground aria2 sidecar through typed
JSON-RPC. HTTP transfers use four range connections by default, support
pause/resume and never overwrite an existing file.

The C++ `TransferQueue` gives each job a stable UUID and enforces
Queued/Active/Paused/Complete/Failed/Cancelled transitions. It retains no source
URI or uploaded metainfo after enqueue, keeps terminal states monotonic, redacts
URL/token-bearing backend errors and forgets finished jobs from aria2 only after
the RPC confirms cleanup. The real aria2 integration test drives this queue,
pauses/resumes an 8 MiB ranged download and verifies every output byte.

`MediaDiscovery` adds a bounded, RAM-only per-Tab candidate store for direct
audio/video, HLS and DASH. Public snapshots omit source URLs, direct media and
HLS candidates are consumed once, and candidates disappear on Tab cleanup or
expiry. The HLS lane parses bounded master playlists, selects a bandwidth
variant, and accepts only finite, unencrypted MPEG-TS VOD media playlists.
Every segment is downloaded through the same aria2 storage/egress boundary,
then assembled into a mode-0600 `.ts` file with atomic no-overwrite publish.
Temporary segments and aria2 results are removed before success is reported.
Live/event HLS, encryption, byte ranges, discontinuities, fMP4 and low-latency
extensions still fail closed; DASH remains detected but gated.

The sidecar cannot launch until the network policy observes an explicit user
transfer action, and its RPC binds only to IPv4 loopback. A fresh 256-bit secret is placed
in a mode-0600 file inside a private runtime directory—not in process
arguments—and is unlinked after authenticated RPC becomes ready. Uploaded
torrent metadata is not retained, DHT/LPD/peer exchange and post-download
seeding are disabled in this initial privacy lane, and an ephemeral request is
rejected unless its download directory is a strict descendant of that private
runtime directory. HTTP(S) downloads can consume a verified route's loopback
HTTP CONNECT endpoint. Magnet and `.torrent` requests fail closed on proxied
routes until every peer socket can be proven to stay inside that egress.

`make check` requires `aria2c` (1.37.0 is the development control), launches
the real sidecar, downloads and byte-verifies an 8 MiB local fixture through
multiple HTTP Range requests, exercises pause/resume, downloads three HLS
segments and verifies the assembled output byte-for-byte, submits valid torrent
metainfo, verifies no uploaded `.torrent` is retained, and proves clean child
process shutdown. Install the dependency with `brew install aria2` on macOS or
`apt install aria2` on Ubuntu. DASH assembly, Chromium download interception,
master-playlist child fetching and the Chromium user-facing transfer shelf
remain follow-up work. The AppKit preview includes a deterministic drawer backed
by the real queue state machine and HLS parser, not a production download
surface. See [the transfer architecture and remaining promotion
work](docs/TRANSFERS.md).

## WARP and Tor egress foundation

`fireball/components/egress` now defines profile-scoped Direct, WARP and Tor
routes, a transaction controller, a real loopback SOCKS5 readiness probe and an
ephemeral Tor sidecar. WARP is accepted only as a preconfigured Local proxy
after explicit user action; it is labeled encrypted egress rather than
anonymity. Tor gets distinct SOCKS5 and HTTP CONNECT listeners per Profile.

Chromium proxy rules contain no implicit Direct fallback. HTTP(S) downloads are
mapped to the route's HTTP CONNECT listener, while peer-to-peer requests remain
disabled on proxied routes. The detailed security boundary, external setup and
remaining Chromium/public-IP/DNS-leak wiring are documented in
[`docs/EGRESS.md`](docs/EGRESS.md).
