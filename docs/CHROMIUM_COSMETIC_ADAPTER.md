# Chromium cosmetic stylesheet adapter

Status: **compile-gated renderer endpoint, not activated in Chrome**.

This slice moves Fireball's cosmetic filtering boundary from a standalone sink
contract to a target wired to compile against the exact Blink renderer API
pinned in `pins/upstream.json`. The protected builder has not produced compile
evidence for this revision yet. The slice deliberately stops before browser
lifecycle wiring, so it is not evidence that a Chromium page hides an ad.

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
it on every bind or mutation. A queued call from an earlier navigation then
fails even if it arrives after the new document exists. Binding also captures
the current Blink `DocumentToken`, and navigation resets the binding and all
layer keys. Calls fail closed for an old epoch, unbound or different document,
inactive frame, non-HTTP(S) URL, non-HTML/XHTML document or invalid stylesheet.

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
immediately after deserialization, and the future browser transport must apply
the same bounds before sending. This associated interface is exposed only to
the browser process, never through a page script or renderer interface broker.

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

The current `CosmeticStyleSink` is synchronous while Mojo acknowledgements are
asynchronous. The browser-side adapter must therefore own pending operations
and commit `DocumentCosmeticController` state only after the renderer confirms
the exact document/layer mutation. It must also:

1. register the renderer agent from Fireball's `ContentRendererClient` overlay;
2. bind a browser remote only after a committed primary-main-frame document;
3. hold a `WeakDocumentPtr` across the epoch handshake, discard its reply after
   invalidation, map that commit to one fresh `DocumentId` and reject late
   callbacks from the previous document;
4. install document rules before first paint, or record a measured limitation;
5. collect only bounded class/ID tokens for the generic phase and preserve the
   controller's monotonic revision checks;
6. revoke both layers for navigation, Tab close, Profile teardown and Shields
   policy change, including renderer crash/restart; and
7. pass a real Chromium build plus navigation, BFCache, crash, profile-isolation
   and visual-regression tests on the protected Linux builder.

Until all seven pass, screenshots and release notes must describe cosmetic
filtering as a native foundation rather than a user-visible blocker.

## Evidence in normal CI

- `renderer_cosmetic_style_state_test` executes document binding, strict
  stylesheet validation, fresh-key replacement, old-epoch bind/mutation
  rejection, independent layers, removal and navigation reset.
- `tests/test_chromium_cosmetic_adapter.py` locks the typed Mojo/GN wiring,
  exact Blink API use, document-token checks and absence of script injection.
- The checksum-pinned overlay includes the `.mojom` source and the protected
  `//fireball:overlay_smoke` graph compiles the renderer target against the
  pinned Chromium checkout when that builder lane runs.
