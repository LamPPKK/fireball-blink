# Tab management and lifecycle

Fireball's tab model combines an Arc-inspired organization layer with a
Chromium-compatible lifecycle boundary. It does not import Arc code and it does
not replace Chromium `Profile` or `content::WebContents` objects.

## Organization model

The product vocabulary follows three sections:

- **Favorite** — Profile-scoped and visible from every Space attached to that
  Profile. Fireball never exposes a Favorite across a Profile storage boundary.
- **Pinned** — durable placement inside one Space and excluded from auto
  archive.
- **Today** — temporary placement inside one Space and eligible for per-Profile
  auto archive when inactive.

This adapts the documented Arc distinction between [Favorites shared across
Spaces](https://resources.arc.net/hc/en-us/articles/19230755904151-Favorites-Top-Tabs-Across-Every-Space),
[Pinned tabs scoped to a Space](https://resources.arc.net/hc/en-us/articles/19231060187159-Pinned-Tabs-Tabs-you-want-to-stick-around),
and [inactive unpinned tabs entering an archive](https://resources.arc.net/hc/en-us/articles/19228855311127-Auto-Archive-Clean-as-you-go).
The implementation and visual language remain Fireball's own.

`BrowserModel` enforces the important boundaries:

- moving a tab between Spaces succeeds only when both Spaces use the same
  Profile;
- Burner Spaces accept Today tabs only and are never restorable;
- changing Classic, Floating, Vertical or Grid presentation never changes tab
  identity, URL or ownership;
- deleting a Space rehomes its Profile-scoped Favorites to another Space with
  the same Profile instead of deleting them;
- removing a Favorite removes its Profile-wide projection and repairs every
  Space that had it active;
- archive restore must target a regular Space owned by the same Profile.

## Lightweight lifecycle contract

The model records `Loaded` and `Discarded` residency without pretending to own
renderer memory. Under pressure, the future Chromium adapter asks
`SelectDiscardCandidates(limit)` for a deterministic order:

1. inactive Today tabs;
2. inactive Pinned tabs;
3. inactive Favorites;
4. least-recent interaction first inside each group.

Active, audible, capture-active and unsaved-form tabs are never candidates.
The adapter must release a background `WebContents` successfully before it
calls `MarkTabDiscarded`. Activating a discarded model tab moves it back to
`Loaded`; the adapter then recreates its `WebContents` using the same Profile,
stable Tab ID and last committed URL.

This is a memory-conscious policy foundation, not evidence that Fireball is as
light as Helium. That claim requires a pinned Chromium control build, an overlay
build, a repeatable site corpus and measured peak/steady-state memory results.

## Auto archive contract

Auto archive uses monotonic timestamps supplied by the platform adapter. The
default threshold is 12 hours and can be configured per Profile. A sweep:

- archives only inactive Today tabs in persistent regular Spaces;
- never archives Favorites, Pinned tabs, active tabs or Burner tabs;
- retains stable ID, URL, title, original Space and owning Profile;
- restores only into a regular Space with the original Profile.

Wall-clock changes therefore cannot archive tabs early. Persistence and sync of
the archive are future adapter responsibilities.

## Chromium promotion work

The standalone C++ test proves ordering and ownership rules. A real browser
still needs:

- `Profile` and `WebContents` adapters;
- renderer discard/recreate acknowledgement and crash-safe persistence;
- memory-pressure observation and telemetry-free local diagnostics;
- tab audio, capture and form-dirty signals from Chromium;
- drag/reorder, command palette, archive library and undo in Chromium Views;
- benchmark comparison against the upstream control artifact.
