# Chromium request adapters

Status: **compile-gated vertical slice, not activated in the browser**.

These slices replace the previous paper-only adapter contract with code that
uses Chromium's public `BrowserContext`, `NavigationHandle`,
`NavigationThrottleRegistry`, `NavigationThrottle`, `OpenURLParams` and
`WebContents` APIs, plus `ResourceRequest`, `RequestDestination` and
`URLLoaderThrottle`, at the exact revision in `pins/upstream.json`.

## Ownership and fail-closed decisions

`ProfilePolicyBinding` is stored on exactly one Chromium `BrowserContext`.
It owns the evaluator and binds a stable Fireball `ProfileId` plus the proxy
rules that the egress owner has confirmed as applied. Installation rejects a
persistent/off-the-record mismatch, duplicate binding, missing evaluator and
unbounded or control-bearing proxy rules. It also requires those rules to equal
the policy bundle's current transactional route. The binding is not removable
while the context lives, so an active throttle cannot retain a dangling
reference.

`ProfileRequestPolicyBundle` is the concrete non-movable evaluator. It owns the
per-Profile adblock policy, URL Cleaner, network evaluator and egress backend;
the internal `RequestPolicy` borrows only those same-lifetime members. A
request carrying another `ProfileId` fails closed. Direct is the initial route;
WARP/Tor changes still use the existing prepare → verify → activate → retire
transaction, and bundle destruction retires the active route before destroying
the backend.

`FireballNavigationThrottle` is deliberately limited to primary-main-frame
HTTP(S) requests. It snapshots trusted `GURL` and `NavigationHandle` fields,
then asks the standalone adapter contract to map the profile policy result to
one of three outcomes:

- proceed only when the policy route exactly matches the confirmed profile
  route;
- block on policy errors, route mismatch, main-frame adblock redirect or an
  unsupported rewrite;
- cancel and asynchronously restart a `GET` navigation when the URL cleaner
  returns a validated replacement URL.

The restart starts from `OpenURLParams::FromNavigationHandle`, preserving the
upstream navigation metadata instead of constructing an incomplete request.
It uses a weak `WebContents` pointer because throttle callbacks must not
destroy their owning contents synchronously.

`FireballURLLoaderThrottle` covers non-main-frame HTTP(S) resource requests.
The factory maps Chromium's typed request destination, canonical `GURL` and
initiator `Origin` into the standalone policy contract. Third-party status is
computed with Chromium's registry-controlled-domain service including private
registries. Initial decisions are computed synchronously on the Profile owner
sequence, allowing safe block, `data:` redirect and same-host/same-scheme
rewrite before the callback returns.

Chromium may move a URLLoader throttle to another sequence, while the pinned
Rust engine is explicitly single-sequence. Server redirects therefore defer,
post evaluation back to the Profile owner sequence and post only the bounded
decision to the loader sequence. Allow resumes; block/error cancels. A redirect
decision requiring URL mutation after the callback fails closed because
Chromium forbids asynchronously touching `RedirectInfo`.

## Evidence

Normal CI compiles and runs `chromium_navigation_adapter_test`, covering
missing policy, route mismatch, allow, block, cleaned-URL restart, unsafe POST
rewrite, same-document bypass and propagated policy failure. It also runs
`profile_request_policy_bundle_test` and `chromium_subresource_adapter_test`
for Profile isolation, egress lifecycle, typed resource context, block,
redirect, rewrite and fail-closed route handling.

The protected `chromium-control` workflow stages the checksum-pinned overlay.
Because `//fireball:fireball_overlay` depends on both
`//fireball/chromium:chromium_adapter` and
`//fireball/chromium:chromium_renderer_adapter`, `gn gen --check` and the
`//fireball:overlay_smoke` build compile and link the browser request adapters
plus both sides of the cosmetic Mojo transport and its document lifecycle owner
and async controller bridge against the exact Chromium checkout. No
successful full-builder run is claimed until GitHub records that workflow on
the protected self-hosted runner.

## Promotion work still required

This commit intentionally does not patch Chromium's navigation-throttle
registry. Activation requires all of the following in the same reviewed
change:

1. create the verified FFI network evaluator from the signed production rules
   artifact and install the completed bundle during Profile startup, before
   user navigation can begin;
2. apply the Direct/WARP/Tor proxy configuration to that Profile's network
   context, then update the binding only after verification succeeds;
3. register `FireballNavigationThrottle` after Chromium's metrics throttle;
4. append `FireballURLLoaderThrottle` from Chromium's URLLoader factory and
   cover the separate keepalive/prefetch paths before claiming complete
   subresource blocking;
5. register the existing renderer stylesheet agent, construct the async
   cosmetic bridge from Chrome, connect trusted DOM-mutation events to
   the bounded collector and run document/BFCache/crash integration tests;
6. build `chrome`, capture startup traffic and prove there is no unowned
   request or direct fallback.

Until those conditions pass, the adapter is a compile-gated vertical slice,
not a user-visible blocker, URL cleaner or egress guarantee.
