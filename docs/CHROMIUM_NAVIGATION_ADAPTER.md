# Chromium primary-navigation adapter

Status: **compile-gated vertical slice, not activated in the browser**.

This slice replaces the previous paper-only adapter contract with code that
uses Chromium's public `BrowserContext`, `NavigationHandle`,
`NavigationThrottleRegistry`, `NavigationThrottle`, `OpenURLParams` and
`WebContents` APIs at the exact revision in `pins/upstream.json`.

## Ownership and fail-closed decisions

`ProfilePolicyBinding` is stored on exactly one Chromium `BrowserContext`.
It owns the evaluator and binds a stable Fireball `ProfileId` plus the proxy
rules that the egress owner has confirmed as applied. Installation rejects a
persistent/off-the-record mismatch, duplicate binding, missing evaluator and
unbounded or control-bearing proxy rules. The binding is not removable while
the context lives, so an active throttle cannot retain a dangling reference.

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

## Evidence

Normal CI compiles and runs `chromium_navigation_adapter_test`, covering
missing policy, route mismatch, allow, block, cleaned-URL restart, unsafe POST
rewrite, same-document bypass and propagated policy failure.

The protected `chromium-control` workflow stages the checksum-pinned overlay.
Because `//fireball:fireball_overlay` depends on
`//fireball/chromium:chromium_adapter`, `gn gen --check` and the
`//fireball:overlay_smoke` build compile and link the API-facing implementation
against the exact Chromium checkout. No successful full-builder run is claimed
until GitHub records that workflow on the protected self-hosted runner.

## Promotion work still required

This commit intentionally does not patch Chromium's navigation-throttle
registry. Activation requires all of the following in the same reviewed
change:

1. create and install the complete signed-rule `RequestPolicy` bundle during
   Profile startup, before user navigation can begin;
2. apply the Direct/WARP/Tor proxy configuration to that Profile's network
   context, then update the binding only after verification succeeds;
3. register `FireballNavigationThrottle` after Chromium's metrics throttle;
4. add a URLLoader throttle for subresources, so main-frame coverage cannot be
   mistaken for complete adblocking;
5. add an isolated-world renderer stylesheet adapter for cosmetic filtering;
6. build `chrome`, capture startup traffic and prove there is no unowned
   request or direct fallback.

Until those conditions pass, the adapter is a compile-gated vertical slice,
not a user-visible blocker, URL cleaner or egress guarantee.
