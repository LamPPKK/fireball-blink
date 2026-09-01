#pragma once

#include <string>
#include <vector>
#include <unordered_set>

namespace fireball::win {

class ShieldsEngine {
public:
    ShieldsEngine();

    // Strips tracking parameters (e.g. utm_source, fbclid, gclid)
    std::string CleanTrackingParameters(const std::string& raw_url) const;

    // Checks if a subresource URL should be blocked (telemetry, trackers, ads)
    bool ShouldBlockResource(const std::string& request_url) const;

    // Generates cosmetic CSS injection script
    std::string GenerateCosmeticCssScript() const;

    // Registers a custom blocked rule pattern
    void AddBlockRule(const std::string& domain_pattern);

private:
    std::unordered_set<std::string> tracking_params_;
    std::vector<std::string> blocked_patterns_;
};

} // namespace fireball::win
