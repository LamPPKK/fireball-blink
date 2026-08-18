#ifndef FIREBALL_BROWSER_DOMAIN_MODEL_H_
#define FIREBALL_BROWSER_DOMAIN_MODEL_H_

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

using ProfileId = internal::StableId<ProfileIdTag>;
using SpaceId = internal::StableId<SpaceIdTag>;
using TabId = internal::StableId<TabIdTag>;

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

struct Profile {
  ProfileId id;
  StorageMode storage_mode;
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
              bool activate);
  bool ActivateTab(const SpaceId& space_id, const TabId& tab_id);
  bool CloseTab(const SpaceId& space_id, const TabId& tab_id);

  const Profile* FindProfile(const ProfileId& id) const;
  const Space* FindSpace(const SpaceId& id) const;
  const Tab* FindTab(const TabId& id) const;

  bool IsSpaceRestorable(const SpaceId& id) const;

  TabLayout tab_layout() const { return tab_layout_; }
  void SetTabLayout(TabLayout layout) { tab_layout_ = layout; }

 private:
  std::map<ProfileId, Profile> profiles_;
  std::map<SpaceId, Space> spaces_;
  std::map<TabId, Tab> tabs_;
  TabLayout tab_layout_ = TabLayout::kChromiumClassic;
};

}  // namespace fireball::browser

#endif  // FIREBALL_BROWSER_DOMAIN_MODEL_H_
