# Download, media discovery and torrent transfers

Fireball's transfer path is a privacy-bounded aria2 integration with a native
C++ queue. It targets the useful parts of a Cốc Cốc-style download experience:
multi-connection HTTP downloads, direct media discovery, pause/resume and
torrent handling. It does not import Cốc Cốc code.

## Product flow

```text
Chromium response observer (B0 adapter)
        │
        ▼
MediaDiscovery — RAM only, bounded per Tab
        │  direct audio/video: one-time TransferRequest
        │  HLS/DASH: visible but assembler-gated
        ▼
TransferQueue — stable UUID, no source URL/metainfo retained
        │
        ▼
Aria2RpcClient — authenticated IPv4-loopback JSON-RPC
        │
        ▼
aria2 sidecar — one persistence + egress boundary
```

The current AppKit Transfer Deck is driven by the real `TransferQueue` state
machine and a deterministic fake backend. The integration test drives the same
queue against a real aria2 1.37.0 process and byte-verifies an 8 MiB ranged
download. The AppKit artifact remains a model preview, not Chromium UI.

## Queue lifecycle

The public states are `Queued`, `Active`, `Paused`, `Complete`, `Failed` and
`Cancelled`.

- A request must use a canonical UUID and match the sidecar's persistent or
  ephemeral storage boundary.
- The queue submits URI or metainfo once, then retains only display metadata,
  source kind, progress counters and a backend GID.
- Pause, resume and cancel validate the current state and require the backend to
  echo the same GID.
- Terminal states never move backwards when a delayed status response arrives.
- A finished item is removed from both the product queue and aria2's result
  store only after `removeDownloadResult` succeeds.
- Backend transport text is bounded and control characters are removed. Text
  containing HTTP, magnet, token or secret material is replaced with a generic
  error before it reaches UI state. aria2 failure messages are represented by a
  short code rather than copied verbatim.

## Media discovery

`MediaDiscovery` accepts only safe HTTP(S) response URLs and classifies direct
audio, direct video, HLS and DASH from MIME type plus URL suffix. It is designed
for Chromium's response observer to call after B0.

- Source URLs remain in memory and never appear in `DiscoveredMedia` snapshots.
- Candidates are deduplicated per Tab and bounded to 32 per Tab / 256 total;
  oldest candidates are evicted deterministically.
- Closing a Tab removes all of its candidates; a monotonic-time sweep expires
  older records.
- Direct audio/video can be consumed once into a normal HTTP transfer.
- HLS and DASH are detected but deliberately not downloadable yet. Treating a
  manifest file as a finished video would be incorrect; a future assembler must
  validate VOD playlists, segment origins, encryption, size and output muxing.
- DRM/Widevine capture is out of scope.

## Torrent and egress policy

Canonical BitTorrent v1 magnets and bounded `.torrent` metainfo are accepted.
Uploaded metainfo is sent as base64 over authenticated loopback RPC and aria2 is
configured not to retain it. DHT, LPD, peer exchange, metadata saving and
post-download seeding are disabled in the first privacy lane.

Direct egress can allow peer-to-peer transfers. WARP and Tor expose HTTP CONNECT
for ordinary HTTP(S) downloads, but torrents fail closed because aria2 peer
sockets cannot yet be proven to stay inside those proxy routes. The UI must not
label a torrent private merely because the browser page itself uses WARP or Tor.

## Remaining Chromium work

- connect response/download observers to `MediaDiscovery` and `TransferQueue`;
- surface save-location confirmation, OS quarantine metadata and file-open
  policy;
- persist regular transfer metadata without persisting signed source URLs;
- delete partial ephemeral files when their Profile/Burner Space closes;
- implement bounded HLS/DASH VOD assembly and cancellation cleanup;
- wire real Views controls and accessibility instead of the AppKit model drawer;
- add hostile-server, disk-full, filename-conflict and reboot recovery tests.

