# Browser overlay

Fireball browser UI and product services live here. Prefer a GN dependency or a `chromium_src` override before adding a direct Chromium patch. The overlay must not replace Chromium's sandbox, process model or PartitionAlloc.

`domain_model` defines stable UUID-backed Profile, Space and Tab identities
without owning Chromium runtime objects. A Profile is the storage boundary;
multiple regular Spaces may reference one persistent Profile. A Burner Space is
accepted only when it references an off-the-record Profile and is never
restorable.

The tab library has three placements: Profile-wide Favorite, Space-scoped
Pinned and auto-archivable Today. Same-Profile moves preserve the tab ID;
cross-Profile moves fail rather than silently crossing cookie/storage state.
Auto archive retains restorable metadata and refuses Burner or cross-Profile
restore.

`TabResidency` and the LRU candidate selector form the adapter boundary for
lightweight background-tab discard. Active, audible, capture-active and
unsaved-form tabs are protected. The model never claims it released renderer
memory: the Chromium adapter must release/recreate `WebContents` before
committing residency. Tab layout remains presentation state and changing it
does not recreate domain tabs. See [`docs/TAB_MANAGEMENT.md`](../../docs/TAB_MANAGEMENT.md).

Profile identity also binds URL-cleaner settings, blocker mode/site exemptions
and the committed Direct/WARP/Tor route. The standalone request policy refuses
to evaluate a request until all three services recognize the same Profile; see
[`docs/REQUEST_PIPELINE.md`](../../docs/REQUEST_PIPELINE.md).
