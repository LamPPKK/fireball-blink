# Fireball B1 overlay component-link gate

This gate is the first real handoff from the standalone Fireball components to
the pinned Chromium toolchain. It is intentionally narrower than a browser
build: it proves that the checksum-pinned `fireball/` GN tree is staged into the
exact Chromium checkout, compiled as one dependency graph, linked into an
executable and run. It does not claim that Fireball is activated from Chromium
`Browser`, `Profile`, `WebContents`, URLLoader or renderer lifecycle hooks.

## Overlay order and provenance

Fireball follows the maintainable ordering learned from Brave:

1. product code in `fireball/`;
2. narrow `chromium_src/` overrides only when an overlay seam is insufficient;
3. direct Chromium patches only as a last resort.

It adopts Helium's checksum discipline without importing Helium patches.
`overlay/manifest.json` binds the exact Chromium commit, source repository,
file count, byte count and SHA-256 of the complete staged tree. The digest binds
each normalized path, size and content digest. Symlinks, executables, unknown
file types, files larger than 2 MiB and a total tree larger than 32 MiB fail
closed. Ignored compiler output is never staged.

Automatic imports remain disabled. `pins/reference-browsers.json` records the
reviewed Brave and Helium revisions and licenses. A reference update never
changes Fireball source or the overlay checksum by itself.

## Staging transaction

`tools/fireball_overlay.py stage` requires:

- a clean, committed Fireball overlay tree;
- the trusted Fireball origin;
- the exact Chromium origin and pinned `HEAD`;
- a current overlay checksum;
- a valid ordered direct-patch manifest;
- no unmanaged `chromium_src` files.

It copies only allowlisted source files into a temporary directory inside the
Chromium checkout, verifies every copied byte, then atomically renames the tree
to `//fireball`. It refuses to overwrite an existing destination. The current
override and direct-patch counts are both zero.

## Link evidence

After the upstream control `.deb` and smoke test have completed, the protected
workflow stages the overlay and builds `//fireball:overlay_smoke` in a separate
GN output directory. The executable depends on the complete current
`fireball_overlay` graph and exercises the domain, blocker policy, URL cleaner,
egress, transfer, primary-navigation adapter contract and startup-network
policy symbols. The graph also compiles the API-facing `ProfilePolicyBinding`
`FireballNavigationThrottle` and `FireballURLLoaderThrottle` against Chromium's
actual public headers. It also compiles the typed Mojo
`FireballCosmeticStyleAgent` against Blink's real `WebDocument` stylesheet and
document-token APIs, plus `FireballCosmeticStyleTransport` against Chromium's
document-scoped `WeakDocumentPtr` and associated-interface APIs. The graph also
compiles the `DocumentUserData` host and `WebContentsObserver` lifecycle owner
against the pinned BFCache, primary-page and renderer-crash callbacks.

The uploaded evidence contains:

- the linked smoke executable and its SHA-256;
- the complete staged-file report;
- exact Chromium and `depot_tools` checkout evidence;
- the passing builder preflight;
- the exact smoke JSON;
- a manifest that repeats the explicit remaining integration limitations.

The gate remains **not run** until the protected self-hosted builder produces a
green artifact. Even after it passes, B1 still needs the Profile lifecycle
hooks, keepalive/prefetch coverage, Chrome construction of the cosmetic owner,
the async controller bridge, renderer registration, a full overlay `chrome` build and
startup-network capture before it can be called a Fireball browser build.
