#include "fireball/browser/domain_model.h"

#include <cassert>
#include <string>
#include <utility>
#include <vector>

namespace {

template <typename Id>
Id Parse(const char* value) {
  auto parsed = Id::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

}  // namespace

int main() {
  using fireball::browser::BrowserModel;
  using fireball::browser::DocumentId;
  using fireball::browser::ProfileId;
  using fireball::browser::SpaceId;
  using fireball::browser::SpaceKind;
  using fireball::browser::StorageMode;
  using fireball::browser::TabId;
  using fireball::browser::TabLayout;
  using fireball::browser::TabPlacement;
  using fireball::browser::TabResidency;

  assert(!ProfileId::Parse("not-a-uuid").has_value());
  assert(!ProfileId::Parse("00000000-0000-0000-0000-000000000000")
              .has_value());
  assert(!ProfileId::Parse("AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA")
              .has_value());
  assert(DocumentId::Parse("40000000-0000-4000-8000-000000000001")
             .has_value());
  assert(!DocumentId::Parse("00000000-0000-0000-0000-000000000000")
              .has_value());

  const ProfileId regular_profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000001");
  const ProfileId burner_profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000002");
  const SpaceId work_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000001");
  const SpaceId research_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000002");
  const SpaceId burner_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000003");
  const TabId favorite_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000001");
  const TabId pinned_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000002");
  const TabId today_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000003");
  const TabId research_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000004");
  const TabId burner_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000005");
  const TabId stale_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000006");
  const TabId oldest_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000007");
  const TabId rehomed_favorite =
      Parse<TabId>("30000000-0000-4000-8000-000000000009");

  BrowserModel model;
  assert(model.AddProfile(regular_profile, StorageMode::kPersistent));
  assert(model.AddProfile(burner_profile, StorageMode::kOffTheRecord));
  assert(!model.AddProfile(regular_profile, StorageMode::kPersistent));

  assert(model.AddSpace(work_space, regular_profile, SpaceKind::kRegular));
  assert(model.AddSpace(research_space, regular_profile, SpaceKind::kRegular));
  assert(model.AddSpace(burner_space, burner_profile, SpaceKind::kBurner));
  assert(!model.AddSpace(
      Parse<SpaceId>("20000000-0000-4000-8000-000000000004"),
      regular_profile, SpaceKind::kBurner));
  assert(!model.AddSpace(
      Parse<SpaceId>("20000000-0000-4000-8000-000000000005"),
      burner_profile, SpaceKind::kRegular));

  assert(model.IsSpaceRestorable(work_space));
  assert(!model.IsSpaceRestorable(burner_space));
  assert(!model.RemoveProfile(regular_profile));

  assert(model.AddTab(favorite_tab, work_space,
                      "https://mail.example.test/", "Mail", false,
                      TabPlacement::kFavorite, 100));
  assert(model.AddTab(pinned_tab, work_space,
                      "https://example.test/project", "Project", false,
                      TabPlacement::kPinned, 200));
  assert(model.AddTab(today_tab, work_space, "https://example.test/today",
                      "Today", true, TabPlacement::kToday, 300));
  assert(model.AddTab(research_tab, research_space,
                      "https://example.test/research", "Research", true,
                      TabPlacement::kToday, 400));
  assert(model.AddTab(burner_tab, burner_space,
                      "https://private.example.test/", "Private", true,
                      TabPlacement::kToday, 500));
  assert(!model.AddTab(
      Parse<TabId>("30000000-0000-4000-8000-000000000008"), burner_space,
      "https://private.example.test/pinned", "Private pinned", false,
      TabPlacement::kPinned, 500));

  assert(model.VisibleTabOrder(work_space) ==
         std::vector<TabId>({favorite_tab, pinned_tab, today_tab}));
  assert(model.VisibleTabOrder(research_space) ==
         std::vector<TabId>({favorite_tab, research_tab}));
  assert(model.ActivateTab(research_space, favorite_tab, 600));
  assert(model.FindSpace(research_space)->active_tab == favorite_tab);
  assert(!model.ActivateTab(work_space, burner_tab));

  // A tab can move between Spaces that share a Profile, but never across the
  // Chromium Profile storage boundary.
  assert(model.MoveTab(today_tab, research_space, 0));
  assert(model.FindTab(today_tab)->space_id == research_space);
  assert(model.FindSpace(work_space)->active_tab == favorite_tab);
  assert(!model.MoveTab(today_tab, burner_space, 0));
  assert(model.MoveTab(today_tab, work_space, 1));

  assert(model.SetTabPlacement(today_tab, TabPlacement::kPinned));
  assert(model.FindTab(today_tab)->placement == TabPlacement::kPinned);
  assert(model.SetTabPlacement(today_tab, TabPlacement::kToday));
  assert(!model.SetTabPlacement(burner_tab, TabPlacement::kFavorite));

  // Layout changes are presentation-only and preserve tab identity and URL.
  const std::string favorite_url = model.FindTab(favorite_tab)->url;
  model.SetTabLayout(TabLayout::kVerticalSidebar);
  assert(model.tab_layout() == TabLayout::kVerticalSidebar);
  assert(model.FindTab(favorite_tab)->url == favorite_url);

  // Auto archive is per Profile and touches only inactive Today tabs. Pinned,
  // Favorite, Burner and active tabs remain live.
  assert(model.SetTabPlacement(research_tab, TabPlacement::kPinned));
  assert(model.SetAutoArchiveAfter(regular_profile, 1000));
  assert(model.AddTab(stale_tab, work_space, "https://example.test/stale",
                      "Stale", false, TabPlacement::kToday, 1000));
  assert(model.ActivateTab(work_space, pinned_tab, 2200));
  const std::vector<TabId> archived = model.ArchiveInactiveTabs(2500);
  assert(archived == std::vector<TabId>({today_tab, stale_tab}));
  assert(model.FindTab(stale_tab) == nullptr);
  assert(model.FindArchivedTab(stale_tab) != nullptr);
  assert(model.FindTab(favorite_tab) != nullptr);
  assert(model.FindTab(research_tab) != nullptr);
  assert(model.FindTab(burner_tab) != nullptr);
  assert(!model.RestoreArchivedTab(stale_tab, burner_space, false, 2600));
  assert(model.RestoreArchivedTab(stale_tab, research_space, false, 2600));
  assert(model.RestoreArchivedTab(today_tab, work_space, false, 2700));
  assert(model.FindArchivedTab(stale_tab) == nullptr);

  // The lightweight lifecycle lane ranks inactive Today tabs before Pinned
  // and Favorite tabs, then uses LRU order. Safety signals and active tabs are
  // never returned as discard candidates.
  assert(model.AddTab(oldest_tab, work_space, "https://example.test/oldest",
                      "Oldest", false, TabPlacement::kToday, 50));
  assert(model.SetTabProtection(research_tab, true, false, false));
  const std::vector<TabId> discard = model.SelectDiscardCandidates(3);
  assert(discard.size() == 3);
  assert(discard[0] == oldest_tab);
  assert(discard[1] == stale_tab);
  assert(discard[2] == today_tab);
  assert(model.MarkTabDiscarded(oldest_tab));
  assert(model.FindTab(oldest_tab)->residency == TabResidency::kDiscarded);
  assert(model.ActivateTab(work_space, oldest_tab, 3000));
  assert(model.FindTab(oldest_tab)->residency == TabResidency::kLoaded);
  assert(!model.MarkTabDiscarded(oldest_tab));
  assert(!model.RecordTabActivity(oldest_tab, 2999));
  assert(model.RecordTabActivity(oldest_tab, 3100));

  // Closing a Profile-scoped Favorite removes it everywhere and repairs an
  // active Space to another visible tab.
  assert(model.CloseTab(research_space, favorite_tab));
  assert(model.FindTab(favorite_tab) == nullptr);
  assert(model.FindSpace(research_space)->active_tab == research_tab);

  // Deleting a Space must not delete a Profile-scoped Favorite while another
  // Space with that Profile remains; its implementation home is reattached.
  assert(model.AddTab(rehomed_favorite, work_space,
                      "https://example.test/favorite", "Favorite", false,
                      TabPlacement::kFavorite, 3200));
  assert(model.RemoveSpace(work_space));
  assert(model.FindTab(rehomed_favorite) != nullptr);
  assert(model.FindTab(rehomed_favorite)->space_id == research_space);
  assert(model.VisibleTabOrder(research_space).front() == rehomed_favorite);

  assert(model.RemoveSpace(burner_space));
  assert(model.FindTab(burner_tab) == nullptr);
  assert(model.RemoveProfile(burner_profile));
  return 0;
}
