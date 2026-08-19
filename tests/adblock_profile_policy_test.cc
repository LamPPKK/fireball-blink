#include "fireball/components/adblock/profile_policy.h"

#include <cassert>
#include <string>
#include <utility>

namespace {

fireball::browser::ProfileId Profile(const char* value) {
  auto parsed = fireball::browser::ProfileId::Parse(std::string(value));
  assert(parsed.has_value());
  return std::move(*parsed);
}

}  // namespace

int main() {
  using fireball::adblock::BlockingMode;
  using fireball::adblock::IsCanonicalHostname;
  using fireball::adblock::ProfilePolicy;

  const auto work = Profile("10000000-0000-4000-8000-000000000001");
  const auto private_profile =
      Profile("10000000-0000-4000-8000-000000000002");

  assert(IsCanonicalHostname("news.example"));
  assert(!IsCanonicalHostname("News.Example"));
  assert(!IsCanonicalHostname("*.example"));
  assert(!IsCanonicalHostname("example..test"));

  ProfilePolicy policy;
  assert(policy.AddProfile(work));
  assert(policy.AddProfile(private_profile, BlockingMode::kAggressive));
  assert(!policy.AddProfile(work));
  assert(policy.ShouldEvaluate(work, "news.example"));
  assert(policy.ShouldEvaluate(private_profile, "news.example"));

  assert(policy.SetSiteExemption(work, "News.Example", true));
  assert(!policy.ShouldEvaluate(work, "news.example"));
  assert(policy.ShouldEvaluate(private_profile, "news.example"));
  assert(policy.SetSiteExemption(work, "news.example", false));
  assert(policy.ShouldEvaluate(work, "news.example"));

  assert(policy.SetMode(work, BlockingMode::kDisabled));
  assert(!policy.ShouldEvaluate(work, "news.example"));
  assert(policy.GetMode(private_profile) == BlockingMode::kAggressive);
  assert(policy.RemoveProfile(private_profile));
  assert(policy.GetMode(private_profile) == BlockingMode::kDisabled);
  return 0;
}
