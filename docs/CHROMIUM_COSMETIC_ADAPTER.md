# Chromium cosmetic stylesheet adapter

Status: **compile-gated renderer endpoint, browser transport, document
lifecycle owner, async controller bridge and bounded native light-DOM
collector; not activated in Chrome**.

This slice moves Fireball's cosmetic filtering boundary from a standalone sink
contract to a target wired to compile against the exact Blink renderer API
pinned in `pins/upstream.json`. The protected builder has not produced compile
evidence for this revision yet. The slice deliberately stops before Chrome
constructs the bridge or registers trusted post-load/mutation refresh triggers,
so it is not evidence that a Chromium page hides an ad.

## Pinned upstream seam

The implementation was checked against Chromium `4b1c7520055f77780fe76d89bb89b76e4d19f64c`:

- [`WebDocument::InsertStyleSheet` and `RemoveInsertedStyleSheet`](https://chromium.googlesource.com/chromium/src/+/4b1c7520055f77780fe76d89bb89b76e4d19f64c/third_party/blink/public/web/web_document.h)
  provide the browser-owned stylesheet seam and document token;
- Chromium's own [extension stylesheet injection](https://chromium.googlesource.com/chromium/src/+/4b1c7520055f77780fe76d89bb89b76e4d19f64c/extensions/renderer/script_injection.cc)
  demonstrates user-origin insertion, BFCache handling and keyed removal; and
- [`RenderFrameObserver`](https://chromium.googlesource.com/chromium/src/+/4b1c7520055f77780fe76d89bb89b76e4d19f64c/content/public/renderer/render_frame_observer.h)
  supplies the new-document and frame-destruction lifecycle.

`FireballCosmeticStyleAgent` is created only for a main frame. A frame-associated
Mojo interface has one authoritative receiver and accepts an opaque UUID
`DocumentId`, one of two typed layers and the already compiled stylesheet. The
renderer increments a document epoch in `DidCreateNewDocument`; the browser
must retrieve that epoch while holding a still-valid `WeakDocumentPtr` and echo
it on every bind or mutation. Every successful bind also rotates a
renderer-owned binding generation, which the browser must echo on mutations.
A queued call from an earlier navigation or an older BFCache remote then fails
even if it arrives after a new bind. Binding also captures the current Blink
`DocumentToken`, and navigation resets the binding and all layer keys. Calls
fail closed for an old epoch or binding generation, unbound or different
document, inactive frame, non-HTTP(S) URL, non-HTML/XHTML document or invalid
stylesheet.

The renderer revalidates the exact selector-only output format even though the
browser policy already validated it. Empty text removes a layer. Replacement
uses a fresh key, inserts the new user-origin stylesheet, commits renderer
state, then removes the previous key. This order avoids a visible gap and
avoids Blink's append semantics for repeated keys. The endpoint uses
`BackForwardCacheAware::kPossiblyDisallow` until a real BFCache restore test can
prove a narrower policy. This flag is not treated as a guarantee that a
document cannot enter BFCache; activation still requires explicit restore and
revocation coverage.

No Fireball renderer source calls `ExecuteJavaScript`, `WebScriptSource`,
`innerHTML`, `document.write` or a page-world script. The state object retains
only the document identity, revisions and opaque stylesheet keys; it does not
retain CSS, URL, hostname, DOM tokens or selector text.

Mojom does not provide a schema annotation for a maximum string length. The
renderer therefore rejects a non-UUID ID and any stylesheet over 512 KiB
immediately after deserialization. The compile-gated browser transport applies
the same bounds before sending. This associated interface is exposed only to
the browser process, never through a page script or renderer interface broker.

## Browser document transport

`FireballCosmeticStyleTransport` is the asynchronous browser-side owner for one
committed primary-main-frame document. Its constructor captures Chromium's
document-scoped `WeakDocumentPtr`, not a raw `RenderFrameHost`. Binding is
allowed only while that exact document remains valid, active and in the primary
main frame.

The transport opens one `AssociatedRemote`, requests the renderer epoch, echoes
it when binding the Fireball `DocumentId`, records the returned non-zero binding
generation, then includes both values on every stylesheet or revoke mutation.
A standalone state machine gives each browser async boundary a separate
generation ticket. Late callbacks, duplicate operations, zero/stale renderer
epochs or bind generations, inactive/BFCache documents, disconnects and
renderer rejections all fail closed. The transport never blocks a browser
sequence with a sync Mojo wait and reports only a status plus stable error code.

The transport validates the 512 KiB selector-only stylesheet contract before
sending, while the renderer independently validates it again after receipt.
It supports one in-flight operation; successful full revocation drops the
remote. `Invalidate()` cancels pending callbacks through an explicit error and
is the hook the compile-gated lifecycle owner calls before document/Tab
teardown.

This transport remains a single-document primitive; the compile-gated
`FireballCosmeticControllerBridge` owns policy commit timing above it. The
legacy synchronous `CosmeticStyleSink` remains only for standalone component
tests and is not the production Chromium path.

## Document and page lifecycle

`FireballCosmeticDocumentHost` uses Chromium
[`DocumentUserData`](https://chromium.googlesource.com/chromium/src/+/4b1c7520055f77780fe76d89bb89b76e4d19f64c/content/public/browser/document_user_data.h)
instead of retaining a raw `RenderFrameHost`. One CSPRNG UUID identifies the
same Blink document while it is active or stored in BFCache. Suspension drops
the browser remote and invalidates every pending generation; restore opens a
new remote, repeats the epoch handshake using the same UUID, then replays the
last acknowledged document layer. The controller schedules a fresh bounded DOM
scan before it reapplies the generic layer, so a cached generic match set is not
trusted across BFCache suspension. The host reports READY after the document
layer replay; generic readiness is committed only after the new scan and
renderer acknowledgement.

The renderer accepts that rebind only when both the UUID and renderer epoch
still match. Closing or replacing the associated receiver removes both style
layers and suspends the live binding while retaining the first UUID claimed by
that Blink document. Only that UUID can rebind, and a successful rebind rotates
its binding generation. A mutation queued before disconnect may run first
because of associated-pipe ordering, but the later disconnect cleanup removes
its result. A mutation delivered after rebind carries an old generation and is
rejected. The lifecycle delegate must resend the document layer and derive a
fresh generic layer after every successful restore handshake.

`FireballCosmeticLifecycleOwner` follows Chromium's preferred
[`PrimaryPageChanged` and `RenderFrameHostStateChanged`](https://chromium.googlesource.com/chromium/src/+/4b1c7520055f77780fe76d89bb89b76e4d19f64c/content/public/browser/web_contents_observer.h)
callbacks rather than resetting state in `DidFinishNavigation`. It retains a
plan only for `kInBackForwardCache`, while normal navigation, BFCache eviction
and `RenderFrameDeleted` terminally dispose the tracked entry. It explicitly
deletes stale `DocumentUserData` after a primary renderer crash, so crash
reinitialization receives a fresh UUID. The delegate exposes no URL, selector
or epoch.

`FireballCosmeticControllerBridge` implements that delegate. Creation requires
a matching Profile-owned `ProfilePolicyBinding`, an authoritative
`TabWebContentsBinding`, its exclusive controller claim and BrowserModel
Tab/Space ownership. The binding reserves one Tab ID per Profile
`BrowserContext` and permits one bridge for that bound `WebContents`. The bridge
revalidates the complete boundary before BFCache replay and every async commit;
loss of that claim first rotates controller/callback generations, detaches the
tracked map, then clears every active or cached document in the bridge. It
obtains the URL
and hostname only from the still-active committed
`RenderFrameHost`, starts a generation ticket, sends the policy stylesheet and
commits its tracked plan only after the exact `DocumentUserData` host reports a
renderer acknowledgement. Generic snapshots use a separate monotonically
increasing revision ticket. The typed `CollectDomSnapshot` call echoes the
document epoch and binding generation, walks at most 50,000 light-DOM elements
through `WebDocument::All()`, and returns only sorted unique `class`/`id`
tokens. The wire representation is a fixed 270,336-byte Mojo array containing
at most 256 KiB of token data plus a two-byte length per token, so a compromised
renderer cannot force an unbounded response allocation. The browser decodes a
canonical sorted payload, revalidates the 4,096-entry and 256-byte-per-token
limits, and reports malformed tuples as bad Mojo messages. Individual invalid
values are skipped; a global limit failure removes any acknowledged generic
layer before retaining the document-specific layer. Revoke and policy refresh
cancel an in-flight collection before mutating styles and also wait for renderer
acknowledgement; refresh clears both desired layers, rebinds, reevaluates the
committed URL and applies the new document plan plus a fresh initial snapshot.

The bridge never accepts caller-supplied token vectors publicly. It posts the
initial collection after the document mutation callback has committed, avoiding
reentrant result ordering. A WebDocument that already reports loaded takes the
fast path; otherwise the renderer waits for `DidFinishLoad()` and uses a
five-second one-shot fallback that caps the normal document-load wait. BFCache
restore requests a fresh snapshot. A future trusted lifecycle
adapter may call `RefreshDomSnapshot()` for bounded DOM-mutation events. The
current collector does not traverse shadow roots or child frames and does not
claim continuously updated SPA coverage.

Suspension invalidates any in-flight bridge ticket but retains the last
acknowledged plan only for a BFCache document. Normal navigation and BFCache
eviction remove it. BFCache restore replays only the document-specific layer;
generic CSS requires a newly acknowledged snapshot, so stale generic matches
are never restored while the scan waits. Renderer crash drops that plan and
the lifecycle owner rotates the UUID. Result callbacks expose counts and stable
codes only; URL, hostname, CSS and DOM tokens remain inside the policy/host
boundary. The bridge drops document CSS from its tracked plan after
acknowledgement; the document-scoped host retains the bounded desired layers
needed for restore. Teardown invalidates bridge callbacks, clears desired state
and drops renderer remotes for the active and cached documents before releasing
the Tab claim. A replacement owner adopts the already-current primary document.
Failed apply/revoke state has an explicit reset-and-rebind path, so recovery does
not require navigation. The bridge owns the lifecycle owner so its delegate
always outlives the observer, but no Chrome tab installs the authoritative
binding or constructs the bridge yet.

## Brave and Helium decisions

The pinned [Brave cosmetic observer](https://github.com/brave/brave-core/blob/724b099256e79f8fc3c5e0d395574590331705da/components/cosmetic_filters/renderer/cosmetic_filters_js_render_frame_observer.cc)
confirmed the renderer-observer lifecycle and keeping product code in an
overlay component. Fireball does **not** copy Brave's JavaScript injection
mechanism; the data contract and native Blink stylesheet call are Fireball's
own narrower seam.

Helium remains a provenance reference only: exact upstream revisions, ordered
patch series and checksum-bound input. This slice imports no Brave or Helium
patch and leaves `patches/manifest.json` empty.

## What remains before activation

The async prepare → apply → acknowledge bridge now exists and cancels stale
operations across suspend, navigation and crash. Activation must still:

1. register the renderer agent from Fireball's `ContentRendererClient` overlay;
2. install one authoritative `TabWebContentsBinding` and construct one
   controller bridge from each Chrome tab lifecycle;
3. install document rules before first paint, or record a measured limitation;
4. connect bounded DOM-mutation events to `RefreshDomSnapshot()` without
   exposing page-controlled token vectors to the browser API;
5. connect Tab close, Profile teardown and Shields changes to bridge revoke or
   refresh entry points; and
6. pass a real Chromium build plus navigation, BFCache, crash, profile-isolation
   and visual-regression tests on the protected Linux builder.

Until all six pass, screenshots and release notes must describe cosmetic
filtering as a native foundation rather than a user-visible blocker.

## Evidence in normal CI

- `renderer_cosmetic_style_state_test` executes document binding, strict
  stylesheet validation, fresh-key replacement, old-epoch bind/mutation
  rejection, disconnect cleanup after a queued mutation, stale
  binding-generation rejection, monotonic snapshot revisions, independent
  layers, removal and navigation reset.
- `cosmetic_dom_snapshot_test` executes ASCII-whitespace class splitting,
  deterministic deduplication, invalid-token skipping, element/entry limits and
  zero-revision rejection.
- `browser_cosmetic_transport_state_test` executes epoch handshake, single
  in-flight mutation, binding-generation capture, stale callback rejection,
  invalidation, revocation and renderer rejection without a Chromium checkout.
- `browser_cosmetic_document_state_test` executes activate, suspend, stale
  callback rejection, two-step bind/desired-style restore, revoke and failed
  retry.
- `browser_cosmetic_controller_state_test` executes acknowledgement-gated
  activation, monotonic generic revisions, stale callback rejection, suspend,
  restore, failed-state reset, terminal disposal and revoke without Chromium.
- `tests/test_chromium_cosmetic_adapter.py` locks the typed Mojo/GN wiring,
  exact `WebDocument::All()`/`WebElement` API use, document-token checks and
  absence of script injection.
- `tests/test_chromium_cosmetic_transport.py` locks the `WeakDocumentPtr`,
  active-primary-frame, associated-remote, async generation and fail-closed
  boundaries.
- `tests/test_chromium_cosmetic_lifecycle.py` locks `DocumentUserData`, UUID
  rotation on crash, BFCache-only retention, terminal disposal,
  primary-page/lifecycle callbacks, suspend/rebind behavior and the absence of
  a `DidFinishNavigation` reset path.
- `tests/test_chromium_cosmetic_controller_bridge.py` locks committed-URL and
  Profile/Tab/WebContents validation, exclusive claims, acknowledgement
  ordering, cleanup/recovery, two-layer restore, policy refresh and the fact
  that Chrome still does not construct the bridge.
- The checksum-pinned overlay includes the `.mojom` source and the protected
  `//fireball:overlay_smoke` graph compiles the renderer target against the
  pinned Chromium checkout when that builder lane runs.
