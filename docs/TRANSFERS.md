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
        │  DASH VOD: one-time DashManifestRequest
        │
        ├── MediaHeaderGrantStore — one-time Profile/Tab/candidate capability
        ▼
TransferQueue / HlsDownload / DashDownload — no source URL in snapshots
        │
        ▼
Aria2RpcClient — authenticated IPv4-loopback JSON-RPC
        │
        ▼
aria2 sidecar — one persistence + egress boundary
        │
        └── DASH private fMP4 tracks → local-only FFmpeg stream-copy → MP4
```

The current AppKit Transfer Deck is driven by the real `TransferQueue` state
machine and HLS parser with a deterministic fake backend. The integration test
drives the same queue plus both media coordinators against aria2 1.37.0,
byte-verifies an 8 MiB ranged download and a three-segment HLS output, then
generates a real fMP4 DASH fixture, downloads it and verifies the muxed video and
audio streams with ffprobe. It repeats HLS and DASH through the loopback HTTP
CONNECT route used by WARP/Tor downloads. The same suite proves that aria2
rejects credential-bearing requests before network I/O. The AppKit artifact
remains a model preview, not Chromium UI.

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
- DASH candidates can be consumed once into `DashDownload`. The closed parser
  accepts static, single-Period, unencrypted fMP4 MPDs using `SegmentTemplate`
  with a fixed duration or bounded `SegmentTimeline`. It selects one video
  representation at or below the configured bandwidth cap and optional audio;
  it rejects DTD/custom entities, XLink, `ContentProtection`, SegmentBase/List,
  live/low-latency behavior, unsafe URLs, unsupported codecs, more than 64
  representations, more than 4,096 segments per track or more than 12 hours.
- DASH initialization and media fragments use the same exact-name, no-rename
  aria2 boundary as the manifest. The coordinator assembles private owner-only
  track files, forgets backend results and removes fragments, then runs an
  absolute, non-group/world-writable FFmpeg executable without a shell. FFmpeg
  receives a clean environment, pre-opened no-symlink inputs through `pipe`
  descriptors only, explicit video/audio maps and stream-copy output. The final
  MP4 is fsynced and published without overwriting; failure/cancel removes
  private manifest, fragment, track and mux artifacts.
- DRM/Widevine capture is out of scope.

## Authenticated media grants

The native boundary prepares authenticated downloads without handing a backend
the whole Profile cookie jar. The future Chromium adapter creates a
cryptographically random UUIDv4 capability ID and stores the associated header
values only in `MediaHeaderGrantStore`.

- A grant is bound to one canonical Profile UUID, Tab UUID and discovered-media
  candidate UUID. It can be consumed once, expires after at most 60 seconds and
  is revoked when its Tab or Profile closes. Failed binding checks do not
  consume another caller's grant.
- The store is RAM-only and capped at 128 records. Grant IDs must never be put
  in URLs, logs, persistence or public UI state.
- The allowlist is exactly `Authorization`, `Cookie`, `Origin`, `Referer` and
  `User-Agent`, in canonical unique order. Values must be printable ASCII,
  cannot have surrounding spaces, are capped at 8 KiB each and 16 KiB total.
  This rejects CR/LF injection and excludes proxy credentials.
- Request headers are valid only for HTTP(S); magnet and `.torrent` requests
  reject them. HLS and DASH carry the same consumed header set across their
  manifest and artifact plans so an origin-aware backend can enforce one policy.
- aria2 is not that backend: version 1.37.0 forwards custom authentication and
  cookie headers across cross-origin redirects, and its RPC API exposes no
  per-request redirect prohibition. `Aria2RpcClient` therefore rejects every
  request with headers before network I/O. Private-media promotion remains
  blocked until Chromium supplies an origin-pinned backend with redirect tests.
- Header containers overwrite owned bytes on destruction. This is
  best-effort process-memory hygiene, not a claim that every allocator or kernel
  copy can be physically overwritten.
- Queue snapshots, errors and result records contain no header or grant value;
  aria2 still runs without `save-session` persistence.

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
- pass `DashManifestRequest` to `DashDownload`, provide the verified FFmpeg
  executable and expose the stable DASH failure codes in the transfer shelf;
- mint the implemented one-time header grant from Chromium's current request
  context without exposing the full cookie jar, revoke it with the owning
  Tab/Profile lifecycle, and use a new origin-aware browser backend rather than
  aria2 whenever a grant is present;
- persist regular transfer metadata without persisting signed source URLs;
- delete partial ephemeral files when their Profile/Burner Space closes;
- decide separately whether encrypted non-DRM HLS is supportable without
  weakening the key lifecycle;
- wire real Views controls and accessibility instead of the AppKit model drawer;
- add hostile-server, disk-full, filename-conflict and reboot recovery tests.
