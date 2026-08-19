# Fireball Blink egress contract

Fireball supports three profile-scoped modes: `Direct`, `WARP` and `Tor`.
Changing mode is a transaction and is rejected while that profile has an
active browsing session. The browser must close its sockets first, prepare and
verify the candidate route, apply one proxy configuration without a `DIRECT`
fallback, and only then retire the old route.

## Route mapping

| Mode | Chromium | HTTP(S) downloads | BitTorrent | Product wording |
| --- | --- | --- | --- | --- |
| Direct | `direct://` | Direct | Enabled | Direct Internet |
| WARP | `socks5://127.0.0.1:<port>` | HTTP CONNECT on the same local port | Disabled until process-level routing exists | Encrypted egress, not anonymity |
| Tor | Per-profile SOCKS5 listener | Per-profile Tor `HTTPTunnelPort` | Disabled | Tor anonymity network, not a guarantee of anonymity |

Chromium always performs destination name resolution at a configured SOCKS5
proxy, which is required to avoid local DNS queries. Chromium does not support
SOCKS5 authentication, so Fireball launches a distinct Tor process and listener
pair for every Profile instead of pretending username/password circuit
isolation can work. See Chromium's official
[proxy behavior](https://chromium.googlesource.com/chromium/src/+/HEAD/net/docs/proxy.md).

aria2 1.37 supports HTTP proxies rather than SOCKS5. Fireball therefore gives
aria2 the local HTTP CONNECT endpoint, forces `proxy-method=tunnel`, clears
proxy bypasses and strips inherited proxy environment variables. The native RPC
client refuses magnet and `.torrent` submissions whenever the active route does
not explicitly allow peer-to-peer. This preserves Direct-mode torrent support
without presenting a proxied transfer that may leak peer sockets. See the
[aria2 proxy contract](https://aria2.github.io/manual/en/html/aria2c.html#cmdoption-all-proxy).

## Tor lifecycle

`TorSidecar` starts an absolute, non-group/world-writable executable with a
mode-0600 temporary configuration and a mode-0700 per-profile data directory.
The generated configuration is client-only, loopback-only, enables `SafeSocks`
and exposes no control or DNS port. The config is unlinked after a real SOCKS5
handshake succeeds. Stop escalates from `SIGTERM` to `SIGKILL` and removes the
ephemeral data directory.

The listener design follows the [Tor manual](https://manpages.debian.org/trixie/tor/tor.1.en.html)
for `SocksPort`, `HTTPTunnelPort`, `SafeSocks` and isolation flags. The Linux
alpha uses the distribution Tor package; Ubuntu 24.04 currently provides Tor
0.4.8.10. Fireball still needs a release-time security-version check before
packaging.

## WARP lifecycle

WARP remains a separately installed system service. Fireball does not silently
change or disconnect the user's machine-wide WARP configuration. The user must
configure Cloudflare Local proxy mode, after which Fireball requires an explicit
activation action and performs a real SOCKS5 negotiation against the loopback
endpoint before accepting the route.

Cloudflare documents that Local proxy listens on `127.0.0.1`, uses port 40000 by
default, supports SOCKS5 and HTTP CONNECT, and requires MASQUE. It also has a
10-second request timeout. See [Cloudflare client modes](https://developers.cloudflare.com/cloudflare-one/team-and-resources/devices/cloudflare-one-client/configure/modes/)
and the [Linux client commands](https://developers.cloudflare.com/warp-client/get-started/linux/).

For a consumer client where mode switching is permitted, provisioning is done
outside Fireball:

```sh
warp-cli tunnel protocol set MASQUE
warp-cli mode proxy
warp-cli proxy port 40000
warp-cli connect
```

Managed Cloudflare Zero Trust devices may forbid these commands; their device
profile must select Local proxy mode instead.

## Pre-commit verification evidence

`RuntimeEgressDelegate` no longer returns an unstructured success boolean. It
must collect a bounded `EgressVerificationEvidence` record through the exact
candidate route without first mutating the active Profile proxy configuration.
The delegate keeps raw HTTPS responses and randomized probe hostnames private;
the native validator receives only route-use facts, the observed proxy port, a
public address, provider attestation and DNS/fallback observations.

For WARP and Tor, commit requires at least two successful probes, both public-IP
and randomized-hostname probes using the candidate proxy, the exact prepared
SOCKS5 port, remote DNS confirmation, no local resolver event, no direct
fallback, a globally routable result address and matching provider evidence.
The WARP collector must derive `kWarp` only from Cloudflare's `warp=on`; the Tor
collector must derive `kTor` only from a successful `IsTor=true` response.
Direct mode requires one successful route-bound public-IP probe and forbids a
proxy port or provider attestation.

Validation failures expose stable `egress.verification.*` codes and never put
the public address, hostname or provider response in the error. A failed check
rolls back the prepared candidate and, for Tor, destroys the new sidecar before
the previous route is touched.

## Implemented and still open

Implemented now:

- typed, profile-scoped routes with correct privacy labels;
- no implicit direct fallback in generated Chromium proxy rules;
- transactional controller with rollback and explicit-consent policy;
- real loopback SOCKS5 readiness negotiation;
- ephemeral per-profile Tor sidecar lifecycle;
- WARP local-proxy verification;
- runtime backend that owns prepared/active Tor processes and retires the old
  process only after route activation succeeds;
- typed public-IP/DNS evidence with native mode, proxy-port, provider,
  remote-DNS, local-leak and direct-fallback validation;
- stable redacted verification failure codes and Tor rollback coverage;
- HTTP CONNECT configuration for aria2 and fail-closed P2P policy.

Still required before the Linux alpha can claim working browser egress:

- implement the Chromium runtime delegate with per-Profile
  `ProxyConfigWithAnnotation` application and collect the required evidence
  from real HTTPS/NetLog probes;
- add real disconnect and kill-switch tests around that collector;
- route Chromium-owned helper traffic and the full aria2 process under the same
  policy, then evaluate whether WARP torrent support can safely be enabled;
- add hardware/network integration tests on the Linux builder.
