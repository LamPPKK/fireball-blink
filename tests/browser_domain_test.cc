#include "fireball/browser/domain_model.h"

#include <cassert>
#include <string>
#include <utility>

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
  using fireball::browser::ProfileId;
  using fireball::browser::SpaceId;
  using fireball::browser::SpaceKind;
  using fireball::browser::StorageMode;
  using fireball::browser::TabId;
  using fireball::browser::TabLayout;

  assert(!ProfileId::Parse("not-a-uuid").has_value());
  assert(!ProfileId::Parse("00000000-0000-0000-0000-000000000000")
              .has_value());
  assert(!ProfileId::Parse("AAAAAAAA-AAAA-AAAA-AAAA-AAAAAAAAAAAA")
              .has_value());

  const ProfileId regular_profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000001");
  const ProfileId burner_profile =
      Parse<ProfileId>("10000000-0000-4000-8000-000000000002");
  const SpaceId work_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000001");
  const SpaceId shared_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000002");
  const SpaceId burner_space =
      Parse<SpaceId>("20000000-0000-4000-8000-000000000003");
  const TabId first_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000001");
  const TabId second_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000002");
  const TabId burner_tab =
      Parse<TabId>("30000000-0000-4000-8000-000000000003");

  BrowserModel model;
  assert(model.AddProfile(regular_profile, StorageMode::kPersistent));
  assert(model.AddProfile(burner_profile, StorageMode::kOffTheRecord));
  assert(!model.AddProfile(regular_profile, StorageMode::kPersistent));

  assert(model.AddSpace(work_space, regular_profile, SpaceKind::kRegular));
  assert(model.AddSpace(shared_space, regular_profile, SpaceKind::kRegular));
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

  assert(model.AddTab(first_tab, work_space, "https://example.test/one",
                      "One", true));
  assert(model.AddTab(second_tab, work_space, "https://example.test/two",
                      "Two", true));
  assert(model.AddTab(burner_tab, burner_space,
                      "https://private.example.test/", "Private", true));
  assert(!model.ActivateTab(work_space, burner_tab));
  assert(model.FindSpace(work_space)->active_tab == second_tab);

  const std::string first_url = model.FindTab(first_tab)->url;
  model.SetTabLayout(TabLayout::kVerticalSidebar);
  assert(model.tab_layout() == TabLayout::kVerticalSidebar);
  assert(model.FindTab(first_tab)->url == first_url);

  assert(model.CloseTab(work_space, second_tab));
  assert(model.FindSpace(work_space)->active_tab == first_tab);
  assert(model.RemoveSpace(burner_space));
  assert(model.FindTab(burner_tab) == nullptr);
  assert(model.RemoveProfile(burner_profile));
  return 0;
}
