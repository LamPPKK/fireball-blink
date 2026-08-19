# Profile cosmetic filtering and renderer adapter contract

Fireball now carries the non-network half of its native blocker through the
real pinned `adblock-rust` engine. The implementation is intentionally split at
the renderer boundary: this repository proves rule evaluation, strict decoding,
Profile policy and safe stylesheet construction; the B0 Chromium checkout must
still inject those styles into real documents.

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

`DocumentCosmeticController` owns the state between that policy and a future
Chromium style sink. Every committed main-frame document receives a non-zero,
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

## Chromium renderer contract after B0

The production adapter must keep this policy as the only source of cosmetic
decisions and satisfy all of the following:

1. Build URL/hostname inputs from committed Chromium navigation state, not page
   JavaScript or an untrusted string parser.
2. Convert Chromium's committed document token to a fresh `DocumentId`, bind
   one policy/engine sequence to the owning Chromium Profile, and route every
   navigation/tab/Profile teardown through `DocumentCosmeticController`.
3. Install the initial stylesheet before first paint through a browser-owned
   stylesheet API in an isolated world. Never use `innerHTML`, `document.write`
   or a page-world script string.
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

This is a production-oriented native foundation, not yet a claim that ads are
visually hidden in a Chromium build. That claim requires the renderer adapter,
a production EasyList/EasyPrivacy release from the existing [signed artifact
pipeline](ADBLOCK_RULE_ARTIFACTS.md), real-page regression corpus and Linux
control-versus-overlay build evidence.
