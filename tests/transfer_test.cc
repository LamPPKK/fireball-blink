#include "fireball/components/transfer/aria2_rpc_client.h"
#include "fireball/components/transfer/egress_transfer_policy.h"
#include "fireball/components/transfer/transfer_types.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

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
  using fireball::transfer::MediaCandidateKind;
  using fireball::transfer::TransferPersistence;
  using fireball::transfer::TransferSourceKind;

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
