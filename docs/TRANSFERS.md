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
        │  HLS VOD: one-time HlsManifestRequest
        │  DASH / unsupported HLS: visible but gated
        ▼
TransferQueue / HlsDownload — no source URL/metainfo in snapshots
        │
        ▼
Aria2RpcClient — authenticated IPv4-loopback JSON-RPC
        │
        ▼
aria2 sidecar — one persistence + egress boundary
```

The current AppKit Transfer Deck is driven by the real `TransferQueue` state
machine and HLS parser with a deterministic fake backend. The integration test
drives the same queue and `HlsDownload` against a real aria2 1.37.0 process,
byte-verifies an 8 MiB ranged download, fetches a master and selected child
playlist, then verifies a three-segment HLS output. It repeats that HLS flow
through the loopback HTTP CONNECT route used by WARP/Tor downloads. The AppKit
artifact remains a model preview, not Chromium UI.

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
- HLS candidates can be consumed once into `HlsDownload`. The coordinator
  fetches the entry manifest through the same aria2 storage/egress boundary,
  accepts either a direct media playlist or one master level, selects a variant
  by bandwidth cap, then fetches that child manifest through the same route.
  Master playlists expose at most 32 validated variants.
- Manifest files are private exact-name downloads capped at 1 MiB. Their URL,
  selected child URL and aria2 GIDs never enter product snapshots; result-store
  entries, manifest files and `.aria2` controls are removed before parsing
  proceeds. Pause/resume/cancel work during both manifest and segment phases.
- The supported media-playlist lane requires `ENDLIST`, at most 2,048 MPEG-TS
  segments, at most 12 hours and at most 32 GiB assembled output. It rejects
  live/event streams, encryption, byte ranges, discontinuities, fMP4 maps,
  unknown behavior-changing tags and low-latency HLS.
- Each HLS segment is queued with an exact private filename and automatic
  renaming disabled. Source URLs are discarded after enqueue; snapshots expose
  only progress and stable error codes. Completion concatenates regular,
  owner-controlled files in order, fsyncs a mode-0600 temporary file, removes
  aria2 results and segment files, then publishes without overwriting an
  existing destination.
- Destroying an active coordinator or segment session issues best-effort
  cancellation, removes the stopped aria2 results and deletes private
  artifacts. A failed atomic publication removes both temporary and newly
  linked output names before reporting error.
- DASH remains detected but deliberately gated. Downloading a manifest alone
  is never presented as a completed video.
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
- pass the one-time `HlsManifestRequest` from the Chromium response observer to
  `HlsDownload`, together with the current Profile's verified aria2 route;
- design a bounded, short-lived cookie/header grant for authenticated media.
  The current coordinator intentionally supports public or URL-tokenized HLS
  only and does not read Chromium cookie storage;
- persist regular transfer metadata without persisting signed source URLs;
- delete partial ephemeral files when their Profile/Burner Space closes;
- add DASH/fMP4 muxing and decide separately whether encrypted non-DRM HLS is
  supportable without weakening the key lifecycle;
- wire real Views controls and accessibility instead of the AppKit model drawer;
- add hostile-server, disk-full, filename-conflict and reboot recovery tests.
