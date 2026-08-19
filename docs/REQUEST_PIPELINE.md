# Profile request policy and Chromium adapter contract

Fireball's navigation component is the single policy seam between browser
requests and the existing privacy/egress foundations. It does not own sockets,
persist URLs or replace Chromium's URL parser. The standalone implementation
uses a deliberately strict HTTP(S) parser so its behavior can be tested before
the B0 Chromium checkout; the production adapter must supply canonical values
from Chromium's `GURL` and registry-controlled-domain services.

## Ordered decision

Every request carries a stable `ProfileId`, full request URL, canonical
destination hostname, canonical source hostname, method, resource type and
third-party/main-frame state. `RequestPolicy::Evaluate` then applies this order:

1. Validate the context and reject credentials, non-HTTP(S) schemes, malformed
   ports, controls, backslashes, unsupported methods and oversized URLs.
2. For a main-frame `GET`, remove exact-name tracking parameters using the
   versioned, Profile-scoped URL-cleaner policy. Raw values and the order of
   retained parameters are preserved; the cleaner retains neither.
3. Check the Profile's blocker mode and source-site exemption. If enabled,
   evaluate the request through the real `adblock-rust` engine.
4. Return an allow, block, bounded `data:` resource redirect, safe same-host
   rewrite or stable error code, together with the already committed
   Profile-scoped Chromium proxy rules.

An exception wins over a block. Redirect/rewrite flags must exactly match their
payloads; unknown flags fail closed. Main-frame `data:` redirects, cross-host
rewrites and HTTPS-to-HTTP rewrites are rejected. An enabled blocker without a
live engine also fails closed instead of silently disabling protection.

The decision object can contain the final request URL because it is handed
directly to the Chromium adapter. It is explicitly not a logging or metrics
object. Error codes never contain URLs, query values or filter content.

## Profile boundary

The URL cleaner, blocker policy and egress controller each require the same
registered `ProfileId`. A Space therefore cannot carry an exemption, blocker
mode or route into a different cookie/storage boundary. Direct is installed
when a Profile is registered; WARP/Tor routes only become visible after their
existing transaction has prepared, verified and activated the candidate.

URL-cleaner rules are exact, ASCII and versioned. The built-in `2026.8.1` set
removes common campaign/click identifiers, including percent-encoded parameter
names, without decoding or rewriting parameter values. Per-site exemptions use
canonical hostnames and belong to one Profile.

## Real engine proof

`network_evaluator.*` is a small C++ owner-neutral adapter over the pinned Rust
C ABI. It converts the engine's flags and consumes every returned allocation
exactly once with `fireball_adblock_string_destroy`. Redirect and rewrite
payloads are bounded again at the C++ boundary.

`tools/run_adblock_tests.py` now builds the Rust crate with its test-only
unsigned constructor, runs the Rust and Python C-ABI suites, then links a C++
integration executable against the produced library. That final test proves
the whole path for URL cleaning, blocking, an allow exception, third-party
matching, a site exemption and the Direct route. Normal product builds do not
expose the unsigned constructor.

## Chromium wiring after B0

The pinned upstream checkout still needs thin adapters rather than another
policy implementation:

- a main-frame navigation throttle must derive trusted `GURL`/host fields and
  apply the cleaned URL before the network request starts;
- a URL-loader throttle must map Chromium resource types and third-party state
  for subresources, then enforce block/redirect/rewrite results;
- one sequenced, verified rules engine must back each applicable Profile policy
  lifetime; rule updates replace the immutable engine at a safe boundary;
- the Profile network context must consume only the committed
  `ProxyConfigWithAnnotation` corresponding to `proxy_rules`; route changes
  remain idle-session transactions;
- response observation can pass bounded media candidates to `MediaDiscovery`,
  but must not persist complete browsing URLs or re-evaluate policy elsewhere.

The current repository proves the platform-neutral policy and native FFI seam.
It does not claim a Chromium interceptor, user-visible Shields UI, cosmetic
injection or working browser proxy until those adapters pass the control and
overlay builds on the Linux builder.
