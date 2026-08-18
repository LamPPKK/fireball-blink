#include "fireball/browser/domain_model.h"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace fireball::browser {
namespace internal {

bool IsCanonicalUuid(std::string_view value) {
  if (value.size() != 36 ||
      value == "00000000-0000-0000-0000-000000000000") {
    return false;
  }
  for (std::size_t index = 0; index < value.size(); ++index) {
    const bool separator =
        index == 8 || index == 13 || index == 18 || index == 23;
    if (separator) {
      if (value[index] != '-') {
        return false;
      }
      continue;
    }
    const char character = value[index];
    if (!((character >= '0' && character <= '9') ||
          (character >= 'a' && character <= 'f'))) {
      return false;
    }
  }
  return true;
}

}  // namespace internal

bool BrowserModel::AddProfile(ProfileId id, StorageMode storage_mode) {
  Profile profile{id, storage_mode};
  return profiles_.emplace(id, std::move(profile)).second;
}

bool BrowserModel::RemoveProfile(const ProfileId& id) {
  const bool in_use = std::any_of(
      spaces_.begin(), spaces_.end(),
      [&id](const auto& entry) { return entry.second.profile_id == id; });
  return !in_use && profiles_.erase(id) == 1;
}

bool BrowserModel::AddSpace(SpaceId id,
                            ProfileId profile_id,
                            SpaceKind kind) {
  const Profile* profile = FindProfile(profile_id);
  if (profile == nullptr) {
    return false;
  }
  const bool requires_off_the_record = kind == SpaceKind::kBurner;
  const bool is_off_the_record =
      profile->storage_mode == StorageMode::kOffTheRecord;
  if (requires_off_the_record != is_off_the_record) {
    return false;
  }
  Space space{id, profile_id, kind, {}, {}};
  return spaces_.emplace(id, std::move(space)).second;
}

bool BrowserModel::RemoveSpace(const SpaceId& id) {
  auto space = spaces_.find(id);
  if (space == spaces_.end()) {
    return false;
  }
  for (const TabId& tab_id : space->second.tab_order) {
    tabs_.erase(tab_id);
  }
  spaces_.erase(space);
  return true;
}

bool BrowserModel::AddTab(TabId id,
                          const SpaceId& space_id,
                          std::string url,
                          std::string title,
                          bool activate) {
  auto space = spaces_.find(space_id);
  if (space == spaces_.end() || tabs_.contains(id)) {
    return false;
  }
  Tab tab_value{id, space_id, std::move(url), std::move(title)};
  auto [tab, inserted] = tabs_.emplace(id, std::move(tab_value));
  if (!inserted) {
    return false;
  }
  space->second.tab_order.push_back(tab->first);
  if (activate || !space->second.active_tab.has_value()) {
    space->second.active_tab = tab->first;
  }
  return true;
}

bool BrowserModel::ActivateTab(const SpaceId& space_id,
                               const TabId& tab_id) {
  auto space = spaces_.find(space_id);
  auto tab = tabs_.find(tab_id);
  if (space == spaces_.end() || tab == tabs_.end() ||
      tab->second.space_id != space_id) {
    return false;
  }
  space->second.active_tab = tab_id;
  return true;
}

bool BrowserModel::CloseTab(const SpaceId& space_id, const TabId& tab_id) {
  auto space = spaces_.find(space_id);
  auto tab = tabs_.find(tab_id);
  if (space == spaces_.end() || tab == tabs_.end() ||
      tab->second.space_id != space_id) {
    return false;
  }
  auto position = std::find(space->second.tab_order.begin(),
                            space->second.tab_order.end(), tab_id);
  if (position == space->second.tab_order.end()) {
    return false;
  }
  const std::size_t closed_index =
      static_cast<std::size_t>(position - space->second.tab_order.begin());
  const bool was_active = space->second.active_tab == tab_id;
  space->second.tab_order.erase(position);
  tabs_.erase(tab);

  if (was_active) {
    if (space->second.tab_order.empty()) {
      space->second.active_tab.reset();
    } else {
      const std::size_t next_index =
          std::min(closed_index, space->second.tab_order.size() - 1);
      space->second.active_tab = space->second.tab_order[next_index];
    }
  }
  return true;
}

const Profile* BrowserModel::FindProfile(const ProfileId& id) const {
  auto profile = profiles_.find(id);
  return profile == profiles_.end() ? nullptr : &profile->second;
}

const Space* BrowserModel::FindSpace(const SpaceId& id) const {
  auto space = spaces_.find(id);
  return space == spaces_.end() ? nullptr : &space->second;
}

const Tab* BrowserModel::FindTab(const TabId& id) const {
  auto tab = tabs_.find(id);
  return tab == tabs_.end() ? nullptr : &tab->second;
}

bool BrowserModel::IsSpaceRestorable(const SpaceId& id) const {
  const Space* space = FindSpace(id);
  if (space == nullptr || space->kind == SpaceKind::kBurner) {
    return false;
  }
  const Profile* profile = FindProfile(space->profile_id);
  return profile != nullptr &&
         profile->storage_mode == StorageMode::kPersistent;
}

}  // namespace fireball::browser
