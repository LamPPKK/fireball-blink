#include "fireball/chromium/profile_partition_manager.h"

#include <utility>

namespace fireball::chromium {

namespace {

constexpr std::string_view kDefaultPartitionDomain = "fireball";

}  // namespace

bool ProfilePartitionManager::RegisterProfile(
    const browser::ProfileId& profile_id,
    browser::StorageMode storage_mode,
    std::string_view base_storage_path) {
  if (partitions_.find(profile_id) != partitions_.end()) {
    return false;
  }

  const bool in_memory = (storage_mode == browser::StorageMode::kOffTheRecord);
  std::string storage_path;
  if (!in_memory) {
    if (base_storage_path.empty()) {
      storage_path = "Profile_" + profile_id.value();
    } else {
      storage_path = std::string(base_storage_path) + "/Profile_" + profile_id.value();
    }
  }

  PartitionConfig config{
      .profile_id = profile_id,
      .space_id = std::nullopt,
      .partition_domain = std::string(kDefaultPartitionDomain),
      .partition_name = profile_id.value(),
      .in_memory = in_memory,
      .storage_path = std::move(storage_path),
  };

  partitions_.emplace(profile_id, std::move(config));
  return true;
}

bool ProfilePartitionManager::UnregisterProfile(
    const browser::ProfileId& profile_id) {
  return partitions_.erase(profile_id) > 0;
}

bool ProfilePartitionManager::RegisterBurnerSpace(
    const browser::SpaceId& space_id,
    const browser::ProfileId& profile_id) {
  if (burner_partitions_.find(space_id) != burner_partitions_.end()) {
    return false;
  }

  PartitionConfig config{
      .profile_id = profile_id,
      .space_id = space_id,
      .partition_domain = std::string(kDefaultPartitionDomain),
      .partition_name = "burner_" + space_id.value(),
      .in_memory = true,
      .storage_path = "",
  };

  burner_partitions_.emplace(space_id, std::move(config));
  return true;
}

bool ProfilePartitionManager::UnregisterBurnerSpace(
    const browser::SpaceId& space_id) {
  return burner_partitions_.erase(space_id) > 0;
}

std::optional<PartitionConfig> ProfilePartitionManager::GetPartitionConfig(
    const browser::ProfileId& profile_id) const {
  auto it = partitions_.find(profile_id);
  if (it == partitions_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<PartitionConfig>
ProfilePartitionManager::GetPartitionConfigForSpace(
    const browser::SpaceId& space_id,
    const browser::ProfileId& profile_id) const {
  auto burner_it = burner_partitions_.find(space_id);
  if (burner_it != burner_partitions_.end()) {
    return burner_it->second;
  }
  return GetPartitionConfig(profile_id);
}

bool ProfilePartitionManager::AreIsolated(
    const browser::ProfileId& a,
    const browser::ProfileId& b) const {
  if (a == b) {
    return false;
  }
  auto config_a = GetPartitionConfig(a);
  auto config_b = GetPartitionConfig(b);
  if (!config_a || !config_b) {
    return true;
  }
  // Independent partition names ensure separate cookie jars and storage
  return config_a->partition_name != config_b->partition_name;
}

}  // namespace fireball::chromium
