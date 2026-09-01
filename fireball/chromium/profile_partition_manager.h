#ifndef FIREBALL_CHROMIUM_PROFILE_PARTITION_MANAGER_H_
#define FIREBALL_CHROMIUM_PROFILE_PARTITION_MANAGER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fireball/browser/domain_model.h"

namespace fireball::chromium {

// Configuration describing an isolated StoragePartition for a Profile or Space.
struct PartitionConfig {
  browser::ProfileId profile_id;
  std::optional<browser::SpaceId> space_id;
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;
  std::string storage_path;
};

// Manages Chromium StoragePartition isolation across multiple Profiles and Spaces.
// Guarantees zero cookie, local storage or service worker leakage between profiles,
// allowing concurrent multi-account sessions (e.g. Personal vs Work workspaces).
class ProfilePartitionManager final {
 public:
  ProfilePartitionManager() = default;
  ~ProfilePartitionManager() = default;

  ProfilePartitionManager(const ProfilePartitionManager&) = delete;
  ProfilePartitionManager& operator=(const ProfilePartitionManager&) = delete;

  // Registers a persistent or in-memory StoragePartition for a Profile.
  bool RegisterProfile(const browser::ProfileId& profile_id,
                       browser::StorageMode storage_mode,
                       std::string_view base_storage_path = "");

  // Unregisters a profile and purges any ephemeral/in-memory session state.
  bool UnregisterProfile(const browser::ProfileId& profile_id);

  // Registers an isolated partition for a Burner Space.
  bool RegisterBurnerSpace(const browser::SpaceId& space_id,
                           const browser::ProfileId& profile_id);

  // Cleans up a Burner Space partition, guaranteeing complete data destruction.
  bool UnregisterBurnerSpace(const browser::SpaceId& space_id);

  // Retrieves the partition configuration for a Profile.
  std::optional<PartitionConfig> GetPartitionConfig(
      const browser::ProfileId& profile_id) const;

  // Retrieves the partition configuration for a Space (falling back to Profile if regular).
  std::optional<PartitionConfig> GetPartitionConfigForSpace(
      const browser::SpaceId& space_id,
      const browser::ProfileId& profile_id) const;

  // Verifies whether two spaces/profiles share storage or are strictly isolated.
  bool AreIsolated(const browser::ProfileId& a,
                   const browser::ProfileId& b) const;

  std::size_t ActivePartitionCount() const { return partitions_.size(); }
  std::size_t ActiveBurnerPartitionCount() const { return burner_partitions_.size(); }

 private:
  std::map<browser::ProfileId, PartitionConfig> partitions_;
  std::map<browser::SpaceId, PartitionConfig> burner_partitions_;
};

}  // namespace fireball::chromium

#endif  // FIREBALL_CHROMIUM_PROFILE_PARTITION_MANAGER_H_
