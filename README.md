# fireball-blink

Buildable F0 tooling foundation for the Chromium-based Fireball Browser.

## Architecture boundary

- Chromium and `depot_tools` are locked to exact official revisions in `pins/upstream.json`.
- Product code starts in the `fireball/` GN overlay.
- Chromium-relative overrides live in `chromium_src/` only when an overlay seam is insufficient.
- Direct patches are last-resort entries in `patches/manifest.json`.
- Every imported patch must record source project, HTTPS URL, exact commit, license, Chromium milestone range, security impact, required tests and SHA-256.
- PartitionAlloc, Chromium's process model and sandbox remain intact.

`make check` validates the pins and patch manifest, then exercises apply, reverse, conflict, checksum and path-traversal fixtures. F0 does not fetch or build Chromium; the first full Linux build remains Gate E3 because it needs a dedicated builder and large storage/RAM budget.

Compiler optimization ideas from Thorium stay in a separate benchmark lane. Brave and Helium are architectural/provenance references only; their patches are never imported automatically.
