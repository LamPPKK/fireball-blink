# Profile cosmetic filtering and renderer adapter contract

Fireball now carries the non-network half of its native blocker through the
real pinned `adblock-rust` engine. The implementation is intentionally split at
the renderer boundary: this repository proves rule evaluation, strict decoding,
Profile policy, safe stylesheet construction and a compile-gated native Blink
stylesheet endpoint, asynchronous browser transport, lifecycle owner and
acknowledgement-driven controller bridge. Chrome activation and DOM-token
collection remain open, so no real-page hiding is claimed.

## Two-phase document plan

`DocumentCosmeticPolicy::BeginDocument` accepts a stable `ProfileId`, a strict
HTTP(S) URL and its canonical hostname. It first checks the Profile's blocker
mode and site exemption, then asks the Rust engine for document-specific
resources. A successful plan contains:

- a bounded stylesheet compiled from validated hide selectors;
- the generic-rule exceptions needed for the second phase;
- whether generic DOM matching is allowed;
- counts showing how many selectors were accepted and how many unsupported
  procedural actions were skipped; and
- an explicit flag when engine scriptlets were skipped.

The plan stores only the Profile key and hostname required to prevent reuse in
another privacy boundary. It retains no page URL. When `generichide` is set by
the engine, the second phase is disabled. Otherwise the renderer may submit a
bounded snapshot of class and ID tokens to `MatchGenericSelectors`, which asks
the same engine for generic matches and returns a second validated stylesheet.

Site exemptions and blocker disablement stop both phases before the engine is
called. Removing an exemption does not revive an old plan: every generic query
rechecks the current Profile policy and the Profile/hostname binding.

## Document lifecycle controller

`DocumentCosmeticController` owns the state between that policy and the
browser-side style sink. Every committed main-frame document receives a non-zero,
UUID-backed `DocumentId`. The controller verifies that its Tab still belongs to
the supplied Profile, permits only one live document per Tab and keeps at most
1,024 active or pending-cleanup document states.

A navigation commit revokes both style layers from the previous document before
the new state becomes active. DOM snapshots carry a monotonically increasing
revision; duplicate, out-of-order or post-navigation snapshots are rejected
without touching the sink. Closing a Tab, changing a Profile policy or tearing
down a Profile has an explicit revocation path. When a live generic evaluation
observes that blocking has been disabled or the site has become exempt, the
controller removes the initial and generic layers and forgets the document.

The `CosmeticStyleSink` contract has exactly two independently replaceable
layers: document-specific and generic. An empty stylesheet removes that layer,
and full revocation removes both. The sink must copy input synchronously and
must never concatenate stylesheet text into HTML. Failed installation does not
publish controller state. Failed revocation is reported and moves into a
pending-cleanup tombstone: no late snapshot can revive it, its `DocumentId`
cannot be reused, and the adapter can retry through
`RetryPendingRevocations()`.

## Boundary limits

Both Rust and C++ enforce limits independently. The current contract is:

| Value | Limit |
| --- | ---: |
| Cosmetic or generic JSON result | 1 MiB |
| Document hide/procedural/exception entries | 4,096 each |
| Generic selector entries | 8,192 |
| Selector | 4,096 bytes |
| Procedural action | 8,192 bytes |
| Exception or DOM class/ID token | 256 bytes |
| Engine-provided injected script | 256 KiB |
| Each class/ID/exception JSON request | 256 KiB |
| Compiled stylesheet | 512 KiB |
| Active + pending-cleanup document states | 1,024 |

The C++ parser accepts exactly the five versioned Rust fields, rejects unknown,
missing or duplicate keys, validates UTF-8 and JSON escapes (including Unicode
surrogate pairs), and requires sorted unique arrays. Every Rust C string is
destroyed exactly once even when decoding fails.

Selectors containing control bytes, rule delimiters, comment delimiters,
markup delimiters, at-rule markers or semicolons are rejected before a
stylesheet is produced. Fireball never concatenates selectors into HTML.
Procedural actions and the engine's injected script are deliberately not
returned for execution; enabling either needs a separate renderer threat model,
an allowlisted instruction format and dedicated security tests.

## Chromium renderer endpoint

`FireballCosmeticStyleAgent` now supplies a typed frame-associated Mojo endpoint
inside the renderer. It binds a `DocumentId` to the active main-frame Blink
`DocumentToken`, a renderer-owned document epoch and a renderer-owned binding
generation, revalidates the exact compiled CSS format and installs two
independent user-origin layers through `WebDocument::InsertStyleSheet`. Every
replacement gets a fresh key; every bind rotates its generation; and a new
document advances the epoch and clears binding and layer state. Stale IPC
therefore cannot rebind an old `DocumentId` to a new page. Receiver disconnect
or replacement removes both style layers and suspends the binding while
retaining the first UUID claimed for that Blink document. Only that UUID can
rebind; a late mutation after rebind is rejected by its old generation. The
lifecycle delegate must reapply the desired two-layer plan after restore. The
endpoint rejects
non-HTTP(S), inactive and non-HTML/XHTML documents and contains no JavaScript
or page-markup injection path.

`FireballCosmeticStyleTransport` now owns the other side of that Mojo channel
for one document-scoped Chromium `WeakDocumentPtr`. It accepts only an active
primary-main-frame document, echoes the renderer epoch, records the bind
generation and sends both values on every mutation. Separate browser
generation tickets reject late async callbacks. Disconnect, invalidation,
BFCache/inactive state and renderer rejection fail closed. The production
Chromium path uses the async bridge rather than adapting the synchronous test
sink.

A compile-gated `DocumentUserData` host now owns that transport for the exact
Blink document. The host survives BFCache with one UUID, suspends and drops its
remote while inactive, then performs a fresh epoch handshake on restore. The
renderer accepts a repeated bind only for the same UUID and epoch, then rotates
the binding generation. Disconnect cleanup removes any mutation that was
already ordered on the older pipe; later old-generation mutations fail. The
host retains only the last acknowledged CSS for its two bounded layers and
replays document then generic CSS before it reports READY. A
`WebContentsObserver` owner uses `PrimaryPageChanged`,
`RenderFrameHostStateChanged` and `RenderFrameDeleted`. Only a document in
`kInBackForwardCache` retains its plan; normal navigation and cache eviction
dispose it. The owner rotates the UUID after renderer crash.
Neither the owner nor its delegate is constructed by Chrome yet.

`FireballCosmeticControllerBridge` now supplies the lifecycle delegate and owns
the observer lifetime. It requires an exclusive Profile/Tab/WebContents binding,
revalidates that claim before every asynchronous commit, evaluates only a
committed active HTTP(S) Chromium URL and commits plan/revision state only after
the exact document host acknowledges a mutation. Suspend/crash invalidates late
callbacks. Policy refresh revokes both layers, rebinds the same document and
reevaluates the current committed URL before applying a replacement plan.
Teardown clears active and cached desired styles before releasing the Tab claim;
failed mutations can be synchronously reset and rebound on the same active
document. Generic snapshots remain an explicit typed C++ input. The bridge rejects more
than 4,096 total entries, tokens over 256 bytes, NUL bytes and snapshots over
256 KiB before policy evaluation; the renderer-side collector that will call
this API is not implemented yet.

The production browser adapter must keep this policy as the only source of
cosmetic decisions and satisfy all of the following:

1. Build URL/hostname inputs from committed Chromium navigation state, not page
   JavaScript or an untrusted string parser.
2. Convert Chromium's committed document token to a fresh `DocumentId`, install
   one authoritative Tab/WebContents binding inside the owning Chromium
   Profile, bind one policy/engine sequence to it, and route every
   navigation/tab/Profile teardown through the async bridge.
3. Commit document, generic and revoke state only after the exact host and
   renderer acknowledge the mutation. Never use `innerHTML`, `document.write`
   or script strings.
4. Collect only bounded class/ID tokens for the generic phase. Do not serialize
   text content, attributes, forms, page URLs or DOM subtrees.
5. Give every class/ID snapshot a strictly increasing revision. Apply the
   generic stylesheet only if its `DocumentId`, Tab, Profile key, hostname and
   current site policy still match.
6. Remove both stylesheets when blocking is disabled or the document is
   replaced, and expose only counts—not selectors or URLs—to Shields UI and
   diagnostics.

`tools/run_adblock_tests.py` proves the full Rust C ABI → C++ parser → Profile
policy and lifecycle-controller path with site-specific hiding, generic class
matching, `$generichide` and a site exemption.
`tests/cosmetic_evaluator_test.cc` covers malformed JSON, duplicate/unsorted
output, unsafe CSS, cross-Profile plan reuse, disabled generic matching and
missing-engine failure. `tests/cosmetic_controller_test.cc` covers navigation
replacement, stale revisions, cross-Profile Tab misuse, policy revocation,
generic suppression, sink failure, Tab deletion and Profile teardown.
`tests/renderer_cosmetic_style_state_test.cc` covers renderer revalidation,
fresh-key replacement, stale commits, independent layers and navigation reset.
`tests/browser_cosmetic_transport_state_test.cc` covers browser epoch/generation
state, stale callbacks, invalidation and revocation.
`tests/browser_cosmetic_document_state_test.cc` covers activation, suspension,
two-layer restore, stale acknowledgement rejection and revocation.
`tests/browser_cosmetic_controller_state_test.cc` covers activation/generic/
revoke tickets, monotonic DOM revisions and stale completion rejection.
`tests/test_chromium_cosmetic_controller_bridge.py` locks the committed-URL,
Profile/Tab, renderer-ack and restore-order source contracts.
The [Chromium cosmetic adapter contract](CHROMIUM_COSMETIC_ADAPTER.md) records
the exact upstream seam and remaining activation work.

This is a production-oriented native foundation, not yet a claim that ads are
visually hidden in a Chromium build. That claim requires Chrome construction,
the bounded DOM collector, renderer registration, a production
EasyList/EasyPrivacy release from the existing
[signed artifact pipeline](ADBLOCK_RULE_ARTIFACTS.md), a real-page regression
corpus and Linux control-versus-overlay build evidence.
