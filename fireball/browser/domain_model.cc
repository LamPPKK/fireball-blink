#include "fireball/browser/domain_model.h"

#include <algorithm>
#include <cstddef>
#include <tuple>
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
  Profile profile{id, storage_mode, {}};
  return profiles_.emplace(id, std::move(profile)).second;
}

bool BrowserModel::RemoveProfile(const ProfileId& id) {
  const bool in_use = std::any_of(
      spaces_.begin(), spaces_.end(),
      [&id](const auto& entry) { return entry.second.profile_id == id; });
  if (in_use || !profiles_.contains(id)) {
    return false;
  }
  std::erase_if(archived_tabs_, [&id](const auto& entry) {
    return entry.second.profile_id == id;
  });
  profiles_.erase(id);
  return true;
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
  std::vector<TabId> removed = space->second.tab_order;
  const ProfileId profile_id = space->second.profile_id;
  Profile* profile = &profiles_.at(profile_id);
  std::optional<SpaceId> favorite_home;
  for (const auto& [candidate_id, candidate] : spaces_) {
    if (candidate_id != id && candidate.profile_id == profile_id) {
      favorite_home = candidate_id;
      break;
    }
  }
  for (const TabId& favorite_id : profile->favorite_order) {
    auto favorite = tabs_.find(favorite_id);
    if (favorite == tabs_.end() || favorite->second.space_id != id) {
      continue;
    }
    if (favorite_home.has_value()) {
      favorite->second.space_id = *favorite_home;
    } else {
      removed.push_back(favorite_id);
    }
  }
  spaces_.erase(space);
  for (const TabId& tab_id : removed) {
    std::erase(profile->favorite_order, tab_id);
    tabs_.erase(tab_id);
    RepairActiveTabsAfterRemoval(tab_id);
  }
  return true;
}

bool BrowserModel::AddTab(TabId id,
                          const SpaceId& space_id,
                          std::string url,
                          std::string title,
                          bool activate,
                          TabPlacement placement,
                          std::uint64_t now_ms) {
  auto space = spaces_.find(space_id);
  if (space == spaces_.end() || tabs_.contains(id) ||
      archived_tabs_.contains(id)) {
    return false;
  }
  if (space->second.kind == SpaceKind::kBurner &&
      placement != TabPlacement::kToday) {
    return false;
  }
  Tab tab_value{
      id,
      space_id,
      std::move(url),
      std::move(title),
      placement,
      TabResidency::kLoaded,
      now_ms,
      false,
      false,
      false,
  };
  auto [tab, inserted] = tabs_.emplace(id, std::move(tab_value));
  if (!inserted) {
    return false;
  }
  if (placement == TabPlacement::kFavorite) {
    profiles_.at(space->second.profile_id).favorite_order.push_back(tab->first);
  } else {
    space->second.tab_order.push_back(tab->first);
  }
  if (activate || !space->second.active_tab.has_value()) {
    space->second.active_tab = tab->first;
  }
  return true;
}

bool BrowserModel::ActivateTab(const SpaceId& space_id,
                               const TabId& tab_id,
                               std::uint64_t now_ms) {
  auto space = spaces_.find(space_id);
  auto tab = tabs_.find(tab_id);
  if (space == spaces_.end() || tab == tabs_.end() ||
      !IsTabVisibleInSpace(tab->second, space->second)) {
    return false;
  }
  space->second.active_tab = tab_id;
  tab->second.residency = TabResidency::kLoaded;
  if (now_ms > 0) {
    tab->second.last_interaction_ms = now_ms;
  }
  return true;
}

bool BrowserModel::CloseTab(const SpaceId& space_id, const TabId& tab_id) {
  auto space = spaces_.find(space_id);
  auto tab = tabs_.find(tab_id);
  if (space == spaces_.end() || tab == tabs_.end() ||
      !IsTabVisibleInSpace(tab->second, space->second)) {
    return false;
  }
  if (tab->second.placement == TabPlacement::kFavorite) {
    Profile& profile = profiles_.at(space->second.profile_id);
    std::erase(profile.favorite_order, tab_id);
  } else {
    Space& owner = spaces_.at(tab->second.space_id);
    std::erase(owner.tab_order, tab_id);
  }
  tabs_.erase(tab);
  RepairActiveTabsAfterRemoval(tab_id);
  return true;
}

bool BrowserModel::MoveTab(const TabId& tab_id,
                           const SpaceId& target_space_id,
                           std::size_t target_index) {
  auto tab = tabs_.find(tab_id);
  auto target = spaces_.find(target_space_id);
  if (tab == tabs_.end() || target == spaces_.end()) {
    return false;
  }
  auto source = spaces_.find(tab->second.space_id);
  if (source == spaces_.end() ||
      source->second.profile_id != target->second.profile_id ||
      (target->second.kind == SpaceKind::kBurner &&
       tab->second.placement != TabPlacement::kToday)) {
    return false;
  }

  if (tab->second.placement == TabPlacement::kFavorite) {
    tab->second.space_id = target_space_id;
    return true;
  }

  std::erase(source->second.tab_order, tab_id);
  const std::size_t insertion =
      std::min(target_index, target->second.tab_order.size());
  target->second.tab_order.insert(target->second.tab_order.begin() + insertion,
                                  tab_id);
  tab->second.space_id = target_space_id;
  if (source->first != target->first && source->second.active_tab == tab_id) {
    source->second.active_tab.reset();
    const std::vector<TabId> fallback = VisibleTabOrder(source->first);
    if (!fallback.empty()) {
      source->second.active_tab = fallback.front();
    }
  }
  return true;
}

bool BrowserModel::SetTabPlacement(const TabId& tab_id,
                                   TabPlacement placement) {
  auto tab = tabs_.find(tab_id);
  if (tab == tabs_.end() || tab->second.placement == placement) {
    return tab != tabs_.end();
  }
  Space& owner = spaces_.at(tab->second.space_id);
  if (owner.kind == SpaceKind::kBurner && placement != TabPlacement::kToday) {
    return false;
  }
  Profile& profile = profiles_.at(owner.profile_id);
  if (tab->second.placement == TabPlacement::kFavorite) {
    std::erase(profile.favorite_order, tab_id);
    owner.tab_order.push_back(tab_id);
  } else if (placement == TabPlacement::kFavorite) {
    std::erase(owner.tab_order, tab_id);
    profile.favorite_order.push_back(tab_id);
  }
  tab->second.placement = placement;
  if (placement != TabPlacement::kFavorite) {
    for (auto& [space_id, space] : spaces_) {
      if (space_id != owner.id && space.active_tab == tab_id) {
        space.active_tab.reset();
        const std::vector<TabId> fallback = VisibleTabOrder(space_id);
        if (!fallback.empty()) {
          space.active_tab = fallback.front();
        }
      }
    }
  }
  return true;
}

std::vector<TabId> BrowserModel::VisibleTabOrder(
    const SpaceId& space_id) const {
  const Space* space = FindSpace(space_id);
  if (space == nullptr) {
    return {};
  }
  const Profile* profile = FindProfile(space->profile_id);
  std::vector<TabId> result =
      profile == nullptr ? std::vector<TabId>{} : profile->favorite_order;
  for (TabPlacement placement : {TabPlacement::kPinned,
                                 TabPlacement::kToday}) {
    for (const TabId& id : space->tab_order) {
      const Tab* tab = FindTab(id);
      if (tab != nullptr && tab->placement == placement) {
        result.push_back(id);
      }
    }
  }
  return result;
}

bool BrowserModel::SetAutoArchiveAfter(const ProfileId& profile_id,
                                       std::uint64_t duration_ms) {
  auto profile = profiles_.find(profile_id);
  if (profile == profiles_.end() || duration_ms == 0) {
    return false;
  }
  profile->second.auto_archive_after_ms = duration_ms;
  return true;
}

std::vector<TabId> BrowserModel::ArchiveInactiveTabs(std::uint64_t now_ms) {
  std::vector<TabId> candidates;
  for (const auto& [tab_id, tab] : tabs_) {
    const Space& space = spaces_.at(tab.space_id);
    const Profile& profile = profiles_.at(space.profile_id);
    if (tab.placement != TabPlacement::kToday || IsTabActive(tab_id) ||
        space.kind == SpaceKind::kBurner ||
        profile.storage_mode != StorageMode::kPersistent ||
        tab.last_interaction_ms == 0 || now_ms < tab.last_interaction_ms ||
        now_ms - tab.last_interaction_ms < profile.auto_archive_after_ms) {
      continue;
    }
    candidates.push_back(tab_id);
  }

  for (const TabId& tab_id : candidates) {
    const Tab tab = tabs_.at(tab_id);
    const ProfileId profile_id = spaces_.at(tab.space_id).profile_id;
    std::erase(spaces_.at(tab.space_id).tab_order, tab_id);
    archived_tabs_.emplace(
        tab_id, ArchivedTab{tab.id, profile_id, tab.space_id, tab.url,
                            tab.title, now_ms});
    tabs_.erase(tab_id);
  }
  return candidates;
}

bool BrowserModel::RestoreArchivedTab(const TabId& tab_id,
                                      const SpaceId& target_space_id,
                                      bool activate,
                                      std::uint64_t now_ms) {
  auto archived = archived_tabs_.find(tab_id);
  auto target = spaces_.find(target_space_id);
  if (archived == archived_tabs_.end() || target == spaces_.end() ||
      archived->second.profile_id != target->second.profile_id ||
      target->second.kind == SpaceKind::kBurner) {
    return false;
  }
  ArchivedTab value = archived->second;
  archived_tabs_.erase(archived);
  if (!AddTab(value.id, target_space_id, value.url, value.title, activate,
              TabPlacement::kToday, now_ms)) {
    archived_tabs_.emplace(value.id, std::move(value));
    return false;
  }
  return true;
}

bool BrowserModel::RecordTabActivity(const TabId& tab_id,
                                     std::uint64_t now_ms) {
  auto tab = tabs_.find(tab_id);
  if (tab == tabs_.end() || now_ms < tab->second.last_interaction_ms) {
    return false;
  }
  tab->second.last_interaction_ms = now_ms;
  return true;
}

bool BrowserModel::SetTabProtection(const TabId& tab_id,
                                    bool audible,
                                    bool capturing,
                                    bool has_unsaved_form) {
  auto tab = tabs_.find(tab_id);
  if (tab == tabs_.end()) {
    return false;
  }
  tab->second.audible = audible;
  tab->second.capturing = capturing;
  tab->second.has_unsaved_form = has_unsaved_form;
  return true;
}

std::vector<TabId> BrowserModel::SelectDiscardCandidates(
    std::size_t limit) const {
  std::vector<const Tab*> candidates;
  for (const auto& [tab_id, tab] : tabs_) {
    if (tab.residency == TabResidency::kLoaded && !IsTabActive(tab_id) &&
        !tab.audible && !tab.capturing && !tab.has_unsaved_form) {
      candidates.push_back(&tab);
    }
  }
  const auto priority = [](TabPlacement placement) {
    switch (placement) {
      case TabPlacement::kToday:
        return 0;
      case TabPlacement::kPinned:
        return 1;
      case TabPlacement::kFavorite:
        return 2;
    }
  };
  std::sort(candidates.begin(), candidates.end(),
            [&priority](const Tab* left, const Tab* right) {
              return std::tuple(priority(left->placement),
                                left->last_interaction_ms, left->id.value()) <
                     std::tuple(priority(right->placement),
                                right->last_interaction_ms, right->id.value());
            });
  std::vector<TabId> result;
  for (std::size_t index = 0;
       index < std::min(limit, candidates.size()); ++index) {
    result.push_back(candidates[index]->id);
  }
  return result;
}

bool BrowserModel::MarkTabDiscarded(const TabId& tab_id) {
  auto tab = tabs_.find(tab_id);
  if (tab == tabs_.end() || IsTabActive(tab_id) || tab->second.audible ||
      tab->second.capturing || tab->second.has_unsaved_form) {
    return false;
  }
  tab->second.residency = TabResidency::kDiscarded;
  return true;
}

bool BrowserModel::MarkTabLoaded(const TabId& tab_id) {
  auto tab = tabs_.find(tab_id);
  if (tab == tabs_.end()) {
    return false;
  }
  tab->second.residency = TabResidency::kLoaded;
  return true;
}

bool BrowserModel::IsTabVisibleInSpace(const Tab& tab,
                                       const Space& space) const {
  if (tab.space_id == space.id) {
    return true;
  }
  if (tab.placement != TabPlacement::kFavorite) {
    return false;
  }
  const Space* owner = FindSpace(tab.space_id);
  return owner != nullptr && owner->profile_id == space.profile_id;
}

bool BrowserModel::IsTabActive(const TabId& tab_id) const {
  return std::any_of(spaces_.begin(), spaces_.end(),
                     [&tab_id](const auto& entry) {
                       return entry.second.active_tab == tab_id;
                     });
}

void BrowserModel::RepairActiveTabsAfterRemoval(const TabId& tab_id) {
  for (auto& [space_id, space] : spaces_) {
    if (space.active_tab != tab_id) {
      continue;
    }
    space.active_tab.reset();
    const std::vector<TabId> fallback = VisibleTabOrder(space_id);
    if (!fallback.empty()) {
      space.active_tab = fallback.front();
    }
  }
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

const ArchivedTab* BrowserModel::FindArchivedTab(const TabId& id) const {
  auto tab = archived_tabs_.find(id);
  return tab == archived_tabs_.end() ? nullptr : &tab->second;
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
