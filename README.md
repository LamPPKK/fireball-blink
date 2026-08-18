# fireball-blink

Buildable B0/B1 tooling foundation for the Chromium-based Fireball Browser.

## Architecture boundary

- Chromium Stable Linux `151.0.7922.169` and `depot_tools` are locked to exact official revisions in `pins/upstream.json`.
- Product code starts in the `fireball/` GN overlay.
- Chromium-relative overrides live in `chromium_src/` only when an overlay seam is insufficient.
- Direct patches are last-resort entries in `patches/manifest.json`.
- Every imported patch must record source project, HTTPS URL, exact commit, license, Chromium milestone range, security impact, required tests and SHA-256.
- PartitionAlloc, Chromium's process model and sandbox remain intact.

`make check` validates upstream/reference pins, generated network policy and the patch manifest; exercises apply, reverse, conflict, checksum and path-traversal fixtures; and compiles a standalone C++ policy test. This repository does not fetch or build Chromium yet. The first full Linux control build still needs the dedicated B0 builder (at least 8 cores, 32 GiB RAM and 300 GiB free disk).

## Brave and Helium reference policy

`pins/reference-browsers.json` records the exact reviewed snapshots. Brave supplies the overlay → override → direct-patch ordering and rebase discipline. Helium supplies the vendor-sorted patch-series, pruning and download-checksum provenance patterns. Fireball does not automatically import either project's patches; every adopted change still needs its own source commit, license, Chromium range, checksum, security review and required tests.

Compiler optimization ideas from Thorium stay in a separate benchmark lane. Generic CPU builds remain the control.

## Startup network policy

`policies/startup_network.json` is default-deny and currently contains no allowed background traffic. `tools/network_policy.py` generates the C++ table used by the overlay and CI rejects stale generated output, unknown schema fields, implicit consent or a default-allow policy. Future services must declare a stable owner, phase, user-visible purpose and explicit opt-in before a rule can be added.
