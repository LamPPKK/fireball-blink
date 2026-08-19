#include "fireball/components/transfer/aria2_rpc_client.h"
#include "fireball/components/transfer/egress_transfer_policy.h"
#include "fireball/components/transfer/media_header_grant.h"
#include "fireball/components/transfer/media_discovery.h"
#include "fireball/components/transfer/transfer_queue.h"
#include "fireball/components/transfer/transfer_types.h"

#include <cassert>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

class FakeTransferBackend final : public fireball::transfer::TransferBackend {
 public:
  fireball::transfer::Aria2RpcResult<std::string> Enqueue(
      const fireball::transfer::TransferRequest& request) override {
    ++enqueue_count;
    last_source_kind = request.source_kind;
    return {enqueue_gid, {}};
  }

  fireball::transfer::Aria2RpcResult<
      fireball::transfer::Aria2TransferStatus>
  TellStatus(std::string_view gid) override {
    ++status_count;
    if (fail_status) {
      return {std::nullopt, status_error};
    }
    fireball::transfer::Aria2TransferStatus result = status;
    result.gid = std::string(gid);
    return {std::move(result), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Pause(
      std::string_view gid) override {
    ++pause_count;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Unpause(
      std::string_view gid) override {
    ++resume_count;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> Remove(
      std::string_view gid) override {
    ++cancel_count;
    return {std::string(gid), {}};
  }

  fireball::transfer::Aria2RpcResult<std::string> ForgetDownloadResult(
      std::string_view gid) override {
    ++forget_count;
    last_forgotten_gid = std::string(gid);
    return {std::string("OK"), {}};
  }

  std::string enqueue_gid = "0123456789abcdef";
  fireball::transfer::Aria2TransferStatus status;
  fireball::transfer::TransferSourceKind last_source_kind =
      fireball::transfer::TransferSourceKind::kHttp;
  bool fail_status = false;
  std::string status_error = "backend\ntransport failure";
  int enqueue_count = 0;
  int status_count = 0;
  int pause_count = 0;
  int resume_count = 0;
  int cancel_count = 0;
  int forget_count = 0;
  std::string last_forgotten_gid;
};

}  // namespace

int main() {
  using fireball::transfer::Aria2RpcClient;
  using fireball::transfer::ClassifyMediaCandidate;
  using fireball::transfer::IsPlausibleTorrentMetainfo;
  using fireball::transfer::IsSafeHttpDownloadUri;
  using fireball::transfer::IsSafeMagnetUri;
  using fireball::transfer::IsSafeOutputName;
  using fireball::transfer::IsValidAria2Gid;
  using fireball::transfer::IsValidAria2Secret;
  using fireball::transfer::MakeTorrentTransferRequest;
  using fireball::transfer::MakeUriTransferRequest;
  using fireball::transfer::MediaHeaderGrantStore;
  using fireball::transfer::MediaDiscovery;
  using fireball::transfer::MediaCandidateKind;
  using fireball::transfer::TransferPersistence;
  using fireball::transfer::TransferQueue;
  using fireball::transfer::TransferRequest;
  using fireball::transfer::TransferSourceKind;
  using fireball::transfer::TransferState;

  assert(IsSafeHttpDownloadUri("https://downloads.example.test/file.bin"));
  assert(IsSafeHttpDownloadUri("http://127.0.0.1:8080/file.bin?part=1"));
  assert(IsSafeHttpDownloadUri("http://[::1]:6800/file.bin"));
  assert(!IsSafeHttpDownloadUri("ftp://downloads.example.test/file.bin"));
  assert(!IsSafeHttpDownloadUri("https://user:secret@example.test/file"));
  assert(!IsSafeHttpDownloadUri("https:///missing-host"));
  assert(!IsSafeHttpDownloadUri("https://example.test:not-a-port/file"));
  assert(!IsSafeHttpDownloadUri("https://example.test:70000/file"));
  assert(!IsSafeHttpDownloadUri("https://example.:443/file"));
  assert(!IsSafeHttpDownloadUri("https://example.test\\evil/file"));
  assert(!IsSafeHttpDownloadUri("https://example.test/bad\nheader"));

  const std::string hex_magnet =
      "magnet:?xt=urn:btih:0123456789abcdef0123456789abcdef01234567&dn=Fireball";
  const std::string base32_magnet =
      "magnet:?xt=urn:btih:ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  assert(IsSafeMagnetUri(hex_magnet));
  assert(IsSafeMagnetUri(base32_magnet));
  assert(!IsSafeMagnetUri("magnet:?dn=missing-hash"));
  assert(!IsSafeMagnetUri(
      hex_magnet + "&xt=urn:btih:0123456789abcdef0123456789abcdef01234567"));
  assert(!IsSafeMagnetUri("magnet:?xt=urn:btih:short"));

  assert(IsSafeOutputName("Fireball Browser 0.1.dmg"));
  assert(!IsSafeOutputName("../escape"));
  assert(!IsSafeOutputName("folder/file.bin"));
  assert(!IsSafeOutputName("bad\rname.bin"));

  const std::vector<std::uint8_t> torrent = {
      'd', '4', ':', 'i', 'n', 'f', 'o', 'd', '1', ':', 'x', 'i', '1', 'e',
      'e', 'e'};
  assert(IsPlausibleTorrentMetainfo(torrent));
  assert(!IsPlausibleTorrentMetainfo(std::vector<std::uint8_t>{'d', 'e'}));

  auto http = MakeUriTransferRequest(
      "https://downloads.example.test/fireball.bin",
      TransferPersistence::kPersistent, "fireball.bin");
  assert(http.has_value());
  assert(http->source_kind == TransferSourceKind::kHttp);
  auto magnet = MakeUriTransferRequest(hex_magnet,
                                       TransferPersistence::kEphemeral);
  assert(magnet.has_value());
  assert(magnet->source_kind == TransferSourceKind::kMagnet);
  assert(!MakeUriTransferRequest("file:///etc/passwd",
                                 TransferPersistence::kPersistent)
              .has_value());
  assert(MakeTorrentTransferRequest(torrent, TransferPersistence::kPersistent)
             .has_value());

  assert(ClassifyMediaCandidate("https://cdn.example.test/movie.mp4?x=1",
                                "application/octet-stream") ==
         MediaCandidateKind::kDirectVideo);
  assert(ClassifyMediaCandidate("https://cdn.example.test/live",
                                "application/vnd.apple.mpegurl") ==
         MediaCandidateKind::kHlsManifest);
  assert(ClassifyMediaCandidate("https://cdn.example.test/manifest.mpd", "") ==
         MediaCandidateKind::kDashManifest);
  assert(ClassifyMediaCandidate("file:///tmp/movie.mp4", "video/mp4") ==
         MediaCandidateKind::kNone);

  MediaDiscovery discovery;
  const std::string tab_id = "30000000-0000-4000-8000-000000000010";
  const std::string media_id = "50000000-0000-4000-8000-000000000001";
  const std::string signed_media =
      "https://cdn.example.test/movie.mp4?token=private";
  assert(discovery.Observe(media_id, tab_id, signed_media, "video/mp4",
                           "Launch film.mp4", 4096, 100));
  auto candidates = discovery.SnapshotForTab(tab_id);
  assert(candidates.size() == 1);
  assert(candidates[0].id == media_id);
  assert(candidates[0].kind == MediaCandidateKind::kDirectVideo);
  assert(candidates[0].directly_downloadable);
  assert(candidates[0].content_length == 4096);
  assert(!discovery.Observe(media_id, tab_id,
                            "https://cdn.example.test/other.mp4", "video/mp4",
                            std::nullopt, 1024, 150));
  assert(discovery.Observe("50000000-0000-4000-8000-000000000002", tab_id,
                           signed_media, "video/mp4", "Launch film 4K.mp4",
                           8192, 200));
  candidates = discovery.SnapshotForTab(tab_id);
  assert(candidates.size() == 1);
  assert(candidates[0].id == media_id);
  assert(candidates[0].display_name == "Launch film 4K.mp4");
  assert(candidates[0].content_length == 8192);

  const std::string hls_id = "50000000-0000-4000-8000-000000000003";
  assert(discovery.Observe(hls_id, tab_id,
                           "https://cdn.example.test/master.m3u8",
                           "application/vnd.apple.mpegurl", std::nullopt, 0,
                           300));
  assert(!discovery.ConsumeDirect(hls_id, TransferPersistence::kPersistent)
              .has_value());
  candidates = discovery.SnapshotForTab(tab_id);
  assert(candidates.size() == 2);
  assert(candidates[0].id == hls_id);
  assert(candidates[0].hls_vod_downloadable);
  assert(!candidates[0].directly_downloadable);
  auto hls_request = discovery.ConsumeHls(hls_id);
  assert(hls_request.has_value());
  assert(hls_request->uri == "https://cdn.example.test/master.m3u8");
  assert(hls_request->request_headers.empty());
  assert(!discovery.ConsumeHls(hls_id).has_value());
  assert(!discovery.Observe(
      "50000000-0000-4000-8000-000000000004", tab_id,
      "https://user:password@cdn.example.test/movie.mp4", "video/mp4",
      std::nullopt, 0, 400));

  MediaHeaderGrantStore header_grants;
  std::vector<fireball::transfer::TransferRequestHeader> media_headers;
  media_headers.emplace_back(
      fireball::transfer::TransferRequestHeaderKind::kAuthorization,
      "Bearer candidate-bound-token");
  media_headers.emplace_back(
      fireball::transfer::TransferRequestHeaderKind::kCookie,
      "session=candidate-bound");
  constexpr std::string_view kGrantId =
      "53000000-0000-4000-8000-000000000001";
  constexpr std::string_view kProfileId =
      "33000000-0000-4000-8000-000000000001";
  assert(header_grants.Mint(std::string(kGrantId), std::string(kProfileId),
                            tab_id, media_id, std::move(media_headers), 1'000,
                            31'000));
  auto granted_headers =
      header_grants.Consume(kGrantId, kProfileId, tab_id, media_id, 2'000);
  assert(granted_headers.has_value());
  auto media_request = discovery.ConsumeDirect(
      media_id, TransferPersistence::kPersistent,
      std::move(*granted_headers));
  assert(media_request.has_value());
  assert(media_request->source == signed_media);
  assert(media_request->output_name ==
         std::optional<std::string>("Launch film 4K.mp4"));
  assert(media_request->request_headers.size() == 2);
  assert(media_request->request_headers[0].value.view() ==
         "Bearer candidate-bound-token");
  assert(discovery.SnapshotForTab(tab_id).empty());
  assert(discovery.ExpireBefore(301) == 0);
  assert(discovery.size() == 0);

  for (std::uint32_t index = 0; index < 34; ++index) {
    char candidate_id[37] = {};
    std::snprintf(candidate_id, sizeof(candidate_id),
                  "51000000-0000-4000-8000-%012x",
                  static_cast<unsigned int>(index + 1));
    assert(discovery.Observe(
        candidate_id, tab_id,
        "https://cdn.example.test/clip-" + std::to_string(index) + ".mp4",
        "video/mp4", std::nullopt, 1024, index + 1));
  }
  candidates = discovery.SnapshotForTab(tab_id);
  assert(candidates.size() ==
         fireball::transfer::kMaximumMediaCandidatesPerTab);
  assert(candidates.back().observed_at_ms == 3);
  assert(discovery.ForgetTab(tab_id) ==
         fireball::transfer::kMaximumMediaCandidatesPerTab);

  for (std::uint32_t index = 0; index < 257; ++index) {
    char candidate_id[37] = {};
    char owner_id[37] = {};
    std::snprintf(candidate_id, sizeof(candidate_id),
                  "52000000-0000-4000-8000-%012x",
                  static_cast<unsigned int>(index + 1));
    std::snprintf(owner_id, sizeof(owner_id),
                  "32000000-0000-4000-8000-%012x",
                  static_cast<unsigned int>(index / 32 + 1));
    assert(discovery.Observe(
        candidate_id, owner_id,
        "https://media.example.test/item-" + std::to_string(index) + ".mp3",
        "audio/mpeg", std::nullopt, 2048, index + 1));
  }
  assert(discovery.size() == fireball::transfer::kMaximumMediaCandidates);
  assert(discovery.ExpireBefore(std::numeric_limits<std::uint64_t>::max()) ==
         fireball::transfer::kMaximumMediaCandidates);
  assert(discovery.size() == 0);

  const std::string secret(64, 'a');
  assert(IsValidAria2Secret(secret));
  assert(!IsValidAria2Secret(std::string(63, 'a')));
  assert(!IsValidAria2Secret(std::string(64, 'A')));
  assert(IsValidAria2Gid("0123456789abcdef"));
  assert(!IsValidAria2Gid("0123456789ABCDEF"));

  Aria2RpcClient invalid_client(0, secret, TransferPersistence::kPersistent);
  assert(!invalid_client.IsConfigurationValid());
  assert(!invalid_client.GetVersion().ok());
  assert(!invalid_client.Pause("../../bad-gid").ok());
  Aria2RpcClient proxied_client(0, secret, TransferPersistence::kPersistent,
                                /*allow_peer_to_peer=*/false);
  assert(!proxied_client.Enqueue(*magnet).ok());
  auto torrent_request =
      MakeTorrentTransferRequest(torrent, TransferPersistence::kPersistent);
  assert(torrent_request.has_value());
  assert(!proxied_client.Enqueue(*torrent_request).ok());

  FakeTransferBackend backend;
  TransferQueue queue(&backend, TransferPersistence::kPersistent);
  const std::string transfer_id =
      "40000000-0000-4000-8000-000000000001";
  const TransferRequest unsafe_request{TransferSourceKind::kHttp,
                                       TransferPersistence::kPersistent,
                                       "file:///etc/passwd", std::nullopt, {},
                                       true, {}};
  assert(!queue.Enqueue("40000000-0000-4000-8000-000000000099",
                        unsafe_request, "Unsafe"));
  assert(!queue.Enqueue("not-a-uuid", *http, "Fireball download"));
  assert(queue.Enqueue(transfer_id, *http, "Fireball download",
                       MediaCandidateKind::kDirectVideo));
  assert(!queue.Enqueue(transfer_id, *http, "Duplicate"));
  assert(backend.enqueue_count == 1);
  assert(queue.Snapshot().size() == 1);
  assert(queue.Find(transfer_id)->display_name == "Fireball download");
  assert(queue.Find(transfer_id)->media_kind ==
         MediaCandidateKind::kDirectVideo);

  backend.status.state = fireball::transfer::Aria2TransferState::kActive;
  backend.status.total_bytes = 1024;
  backend.status.completed_bytes = 256;
  backend.status.bytes_per_second = 128;
  assert(queue.Refresh(transfer_id));
  assert(queue.Find(transfer_id)->state == TransferState::kActive);
  assert(queue.Find(transfer_id)->completed_bytes == 256);
  assert(queue.Pause(transfer_id));
  assert(queue.Find(transfer_id)->state == TransferState::kPaused);
  assert(queue.Resume(transfer_id));
  assert(queue.Find(transfer_id)->state == TransferState::kQueued);

  backend.status.state = fireball::transfer::Aria2TransferState::kError;
  backend.status.error_code = "3";
  backend.status.error_message =
      "failed https://signed.example.test/video?token=private";
  assert(queue.Refresh(transfer_id));
  assert(queue.Find(transfer_id)->state == TransferState::kFailed);
  assert(queue.Find(transfer_id)->failure_code == "ARIA2_3");
  assert(queue.Find(transfer_id)->failure_code.find("signed.example") ==
         std::string::npos);
  assert(queue.ForgetFinished(transfer_id));
  assert(queue.Find(transfer_id) == nullptr);
  assert(backend.last_forgotten_gid == backend.enqueue_gid);

  const std::string torrent_id =
      "40000000-0000-4000-8000-000000000002";
  assert(queue.Enqueue(torrent_id, *torrent_request, "Private torrent"));
  assert(backend.last_source_kind == TransferSourceKind::kTorrentMetainfo);
  assert(queue.Cancel(torrent_id));
  assert(queue.Find(torrent_id)->state == TransferState::kCancelled);
  backend.status.state = fireball::transfer::Aria2TransferState::kActive;
  assert(queue.Refresh(torrent_id));
  assert(queue.Find(torrent_id)->state == TransferState::kCancelled);
  assert(queue.ForgetFinished(torrent_id));

  const std::string malformed_error_id =
      "40000000-0000-4000-8000-000000000005";
  assert(queue.Enqueue(malformed_error_id, *http, "Malformed backend error"));
  backend.status.state = fireball::transfer::Aria2TransferState::kError;
  backend.status.error_code = "http";
  assert(queue.Refresh(malformed_error_id));
  assert(queue.Find(malformed_error_id)->failure_code == "TRANSFER_FAILED");
  assert(queue.ForgetFinished(malformed_error_id));

  auto ephemeral_http = MakeUriTransferRequest(
      "https://downloads.example.test/ephemeral.bin",
      TransferPersistence::kEphemeral, "ephemeral.bin");
  assert(ephemeral_http.has_value());
  assert(!queue.Enqueue("40000000-0000-4000-8000-000000000003",
                        *ephemeral_http, "Wrong boundary"));

  assert(queue.Enqueue("40000000-0000-4000-8000-000000000004", *http,
                       "Backend outage"));
  backend.fail_status = true;
  assert(!queue.RefreshAll());
  assert(queue.last_control_error() == "backendtransport failure");
  backend.status_error =
      "failed https://signed.example.test/file?token=private";
  assert(!queue.RefreshAll());
  assert(queue.last_control_error() == "transfer backend request failed");

  fireball::transfer::Aria2SidecarConfig sidecar_config;
  std::string policy_error;
  assert(fireball::transfer::ApplyEgressRoute(
      fireball::egress::MakeDirectRoute(), &sidecar_config, &policy_error));
  assert(!sidecar_config.outbound_http_proxy.has_value());
  assert(sidecar_config.allow_peer_to_peer);
  auto warp_route = fireball::egress::MakeWarpRoute(40000);
  assert(warp_route.has_value());
  assert(fireball::transfer::ApplyEgressRoute(
      *warp_route, &sidecar_config, &policy_error));
  assert(sidecar_config.outbound_http_proxy ==
         std::optional<std::string>("http://127.0.0.1:40000"));
  assert(!sidecar_config.allow_peer_to_peer);
  return 0;
}
