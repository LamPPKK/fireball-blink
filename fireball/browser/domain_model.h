#ifndef FIREBALL_BROWSER_DOMAIN_MODEL_H_
#define FIREBALL_BROWSER_DOMAIN_MODEL_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fireball::browser {

namespace internal {

bool IsCanonicalUuid(std::string_view value);

template <typename Tag>
class StableId final {
 public:
  static std::optional<StableId> Parse(std::string value) {
    if (!IsCanonicalUuid(value)) {
      return std::nullopt;
    }
    return StableId(std::move(value));
  }

  const std::string& value() const { return value_; }

  friend bool operator==(const StableId&, const StableId&) = default;

  friend bool operator<(const StableId& left, const StableId& right) {
    return left.value_ < right.value_;
  }

 private:
  explicit StableId(std::string value) : value_(std::move(value)) {}

  std::string value_;
};

}  // namespace internal

struct ProfileIdTag;
struct SpaceIdTag;
struct TabIdTag;
struct DocumentIdTag;

using ProfileId = internal::StableId<ProfileIdTag>;
using SpaceId = internal::StableId<SpaceIdTag>;
using TabId = internal::StableId<TabIdTag>;
using DocumentId = internal::StableId<DocumentIdTag>;

enum class StorageMode {
  kPersistent,
  kOffTheRecord,
};

enum class SpaceKind {
  kRegular,
  kBurner,
};

enum class TabLayout {
  kChromiumClassic,
  kSafariFloating,
  kVerticalSidebar,
  kTabGrid,
};

// Arc-inspired organization semantics. Favorites are Profile-scoped and are
// visible from every Space attached to that Profile. Pinned and Today tabs are
// owned by one Space. Only Today tabs participate in auto archive.
enum class TabPlacement {
  kFavorite,
  kPinned,
  kToday,
};

// Model-side lifecycle state. The Chromium adapter is responsible for
// releasing/recreating WebContents and only commits a transition after that
// operation succeeds.
enum class TabResidency {
  kLoaded,
  kDiscarded,
};

struct Profile {
  ProfileId id;
  StorageMode storage_mode;
  std::vector<TabId> favorite_order;
  std::uint64_t auto_archive_after_ms = 12ULL * 60ULL * 60ULL * 1000ULL;
};

struct Space {
  SpaceId id;
  ProfileId profile_id;
  SpaceKind kind;
  std::vector<TabId> tab_order;
  std::optional<TabId> active_tab;
};

struct Tab {
  TabId id;
  SpaceId space_id;
  std::string url;
  std::string title;
  TabPlacement placement = TabPlacement::kToday;
  TabResidency residency = TabResidency::kLoaded;
  std::uint64_t last_interaction_ms = 0;
  bool audible = false;
  bool capturing = false;
  bool has_unsaved_form = false;
};

struct ArchivedTab {
  TabId id;
  ProfileId profile_id;
  SpaceId original_space_id;
  std::string url;
  std::string title;
  std::uint64_t archived_at_ms;
};

// Owns only browser-domain state. Chromium Profiles and WebContents remain the
// runtime storage/process objects and are attached by the platform adapter.
class BrowserModel final {
 public:
  bool AddProfile(ProfileId id, StorageMode storage_mode);
  bool RemoveProfile(const ProfileId& id);

  bool AddSpace(SpaceId id, ProfileId profile_id, SpaceKind kind);
  bool RemoveSpace(const SpaceId& id);

  bool AddTab(TabId id,
              const SpaceId& space_id,
              std::string url,
              std::string title,
              bool activate,
              TabPlacement placement = TabPlacement::kToday,
              std::uint64_t now_ms = 0);
  bool ActivateTab(const SpaceId& space_id,
                   const TabId& tab_id,
                   std::uint64_t now_ms = 0);
  bool CloseTab(const SpaceId& space_id, const TabId& tab_id);
  bool MoveTab(const TabId& tab_id,
               const SpaceId& target_space_id,
               std::size_t target_index);
  bool SetTabPlacement(const TabId& tab_id, TabPlacement placement);

  std::vector<TabId> VisibleTabOrder(const SpaceId& space_id) const;

  bool SetAutoArchiveAfter(const ProfileId& profile_id,
                           std::uint64_t duration_ms);
  std::vector<TabId> ArchiveInactiveTabs(std::uint64_t now_ms);
  bool RestoreArchivedTab(const TabId& tab_id,
                          const SpaceId& target_space_id,
                          bool activate,
                          std::uint64_t now_ms = 0);

  bool RecordTabActivity(const TabId& tab_id, std::uint64_t now_ms);
  bool SetTabProtection(const TabId& tab_id,
                        bool audible,
                        bool capturing,
                        bool has_unsaved_form);
  std::vector<TabId> SelectDiscardCandidates(std::size_t limit) const;
  bool MarkTabDiscarded(const TabId& tab_id);
  bool MarkTabLoaded(const TabId& tab_id);

  const Profile* FindProfile(const ProfileId& id) const;
  const Space* FindSpace(const SpaceId& id) const;
  const Tab* FindTab(const TabId& id) const;
  const ArchivedTab* FindArchivedTab(const TabId& id) const;

  bool IsSpaceRestorable(const SpaceId& id) const;

  TabLayout tab_layout() const { return tab_layout_; }
  void SetTabLayout(TabLayout layout) { tab_layout_ = layout; }

 private:
  bool IsTabVisibleInSpace(const Tab& tab, const Space& space) const;
  bool IsTabActive(const TabId& tab_id) const;
  void RepairActiveTabsAfterRemoval(const TabId& tab_id);

  std::map<ProfileId, Profile> profiles_;
  std::map<SpaceId, Space> spaces_;
  std::map<TabId, Tab> tabs_;
  std::map<TabId, ArchivedTab> archived_tabs_;
  TabLayout tab_layout_ = TabLayout::kChromiumClassic;
};

}  // namespace fireball::browser

#endif  // FIREBALL_BROWSER_DOMAIN_MODEL_H_
