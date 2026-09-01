#include "fireball/chromium/profile_partition_manager.h"

#include <cassert>
#include <iostream>

using fireball::browser::ProfileId;
using fireball::browser::SpaceId;
using fireball::browser::StorageMode;
using fireball::chromium::ProfilePartitionManager;

int main() {
  auto profile_work =
      ProfileId::Parse("10000000-0000-4000-8000-000000000001");
  auto profile_personal =
      ProfileId::Parse("10000000-0000-4000-8000-000000000002");
  auto profile_burner =
      ProfileId::Parse("10000000-0000-4000-8000-000000000003");
  auto space_burner =
      SpaceId::Parse("20000000-0000-4000-8000-000000000001");

  assert(profile_work.has_value());
  assert(profile_personal.has_value());
  assert(profile_burner.has_value());
  assert(space_burner.has_value());

  ProfilePartitionManager manager;

  // 1. Register persistent profiles (Work & Personal)
  assert(manager.RegisterProfile(*profile_work, StorageMode::kPersistent, "/data"));
  assert(manager.RegisterProfile(*profile_personal, StorageMode::kPersistent, "/data"));
  assert(!manager.RegisterProfile(*profile_work, StorageMode::kPersistent));  // Duplicate rejected

  auto config_work = manager.GetPartitionConfig(*profile_work);
  auto config_personal = manager.GetPartitionConfig(*profile_personal);

  assert(config_work.has_value());
  assert(config_personal.has_value());
  assert(!config_work->in_memory);
  assert(!config_personal->in_memory);
  assert(config_work->partition_name != config_personal->partition_name);
  assert(config_work->storage_path != config_personal->storage_path);

  // 2. Strict isolation check between Work and Personal
  assert(manager.AreIsolated(*profile_work, *profile_personal));
  assert(!manager.AreIsolated(*profile_work, *profile_work));

  // 3. Register Burner Space partition (In-Memory ephemeral)
  assert(manager.RegisterBurnerSpace(*space_burner, *profile_burner));
  auto config_burner =
      manager.GetPartitionConfigForSpace(*space_burner, *profile_burner);
  assert(config_burner.has_value());
  assert(config_burner->in_memory);
  assert(config_burner->storage_path.empty());
  assert(manager.ActiveBurnerPartitionCount() == 1);

  // 4. Burner Space cleanup and data destruction
  assert(manager.UnregisterBurnerSpace(*space_burner));
  assert(!manager.UnregisterBurnerSpace(*space_burner));  // Already deleted
  assert(manager.ActiveBurnerPartitionCount() == 0);

  std::cout << "profile_partition_manager_test: passed\n";
  return 0;
}
