#ifndef FIREBALL_COMPONENTS_TRANSFER_TRANSFER_TYPES_H_
#define FIREBALL_COMPONENTS_TRANSFER_TRANSFER_TYPES_H_

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fireball::transfer {

inline constexpr std::size_t kMaximumUriBytes = 8192;
inline constexpr std::size_t kMaximumTorrentBytes = 4 * 1024 * 1024;

enum class TransferSourceKind {
  kHttp,
  kMagnet,
  kTorrentMetainfo,
};

enum class TransferPersistence {
  kPersistent,
  kEphemeral,
};

enum class MediaCandidateKind {
  kNone,
  kDirectAudio,
  kDirectVideo,
  kHlsManifest,
  kDashManifest,
};

struct TransferRequest {
  TransferSourceKind source_kind;
  TransferPersistence persistence;
  std::string source;
  std::optional<std::string> output_name;
  std::vector<std::uint8_t> torrent_metainfo;
  // Ordinary downloads may choose a collision-free name. Multi-file assembly
  // jobs require exact private filenames and therefore disable this per RPC.
  bool allow_automatic_renaming = true;
};

// Accepts only ordinary HTTP(S) downloads. Credentials and control characters
// are rejected before a request reaches the aria2 sidecar.
bool IsSafeHttpDownloadUri(std::string_view uri);

// Accepts one canonical BitTorrent v1 info hash (40 hex or 32 base32) in xt.
// Tracker/display-name parameters may follow, but duplicate xt values fail.
bool IsSafeMagnetUri(std::string_view uri);

// This is a cheap boundary check, not a complete bencode parser. aria2 remains
// the authoritative metainfo parser after the size/type boundary is enforced.
bool IsPlausibleTorrentMetainfo(std::span<const std::uint8_t> metainfo);

bool IsSafeOutputName(std::string_view name);
bool IsCanonicalTransferId(std::string_view id);
bool IsValidTransferRequest(const TransferRequest& request);

std::optional<TransferRequest> MakeUriTransferRequest(
    std::string uri,
    TransferPersistence persistence,
    std::optional<std::string> output_name = std::nullopt);

std::optional<TransferRequest> MakeTorrentTransferRequest(
    std::vector<std::uint8_t> metainfo,
    TransferPersistence persistence,
    std::optional<std::string> output_name = std::nullopt);

MediaCandidateKind ClassifyMediaCandidate(std::string_view uri,
                                          std::string_view mime_type);

}  // namespace fireball::transfer

#endif  // FIREBALL_COMPONENTS_TRANSFER_TRANSFER_TYPES_H_
