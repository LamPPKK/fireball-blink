#include "fireball/win/shields_engine.h"
#include <sstream>
#include <algorithm>

namespace fireball::win {

ShieldsEngine::ShieldsEngine() {
    tracking_params_ = {
        "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
        "fbclid", "gclid", "msclkid", "yclid", "mc_eid", "igshid", "dclid",
        "_hsenc", "_hsmi", "mc_cid", "mkt_tok", "wickedid", "twclid"
    };

    blocked_patterns_ = {
        "doubleclick.net",
        "google-analytics.com",
        "googlesyndication.com",
        "adservice.google.com",
        "facebook.net/tr",
        "scorecardresearch.com",
        "criteo.com",
        "adnxs.com",
        "hotjar.com",
        "clarity.ms/tag"
    };
}

std::string ShieldsEngine::CleanTrackingParameters(const std::string& raw_url) const {
    size_t q_pos = raw_url.find('?');
    if (q_pos == std::string::npos) {
        return raw_url;
    }

    std::string base = raw_url.substr(0, q_pos);
    std::string query_and_frag = raw_url.substr(q_pos + 1);

    size_t hash_pos = query_and_frag.find('#');
    std::string query = (hash_pos == std::string::npos) ? query_and_frag : query_and_frag.substr(0, hash_pos);
    std::string fragment = (hash_pos == std::string::npos) ? "" : query_and_frag.substr(hash_pos);

    std::vector<std::string> clean_pairs;
    std::stringstream ss(query);
    std::string pair;

    while (std::getline(ss, pair, '&')) {
        if (pair.empty()) continue;
        size_t eq_pos = pair.find('=');
        std::string key = (eq_pos == std::string::npos) ? pair : pair.substr(0, eq_pos);
        if (tracking_params_.find(key) == tracking_params_.end()) {
            clean_pairs.push_back(pair);
        }
    }

    if (clean_pairs.empty()) {
        return base + fragment;
    }

    std::string clean_query;
    for (size_t i = 0; i < clean_pairs.size(); ++i) {
        if (i > 0) clean_query += "&";
        clean_query += clean_pairs[i];
    }

    return base + "?" + clean_query + fragment;
}

bool ShieldsEngine::ShouldBlockResource(const std::string& request_url) const {
    for (const auto& pattern : blocked_patterns_) {
        if (request_url.find(pattern) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string ShieldsEngine::GenerateCosmeticCssScript() const {
    return "(function() {"
           "  var css = '.ad-banner, .adsbox, [class*=\"ad-container\"], [id*=\"google_ads\"], .taboola-container, .outbrain { display: none !important; }';"
           "  var head = document.head || document.getElementsByTagName('head')[0];"
           "  if (head) {"
           "    var style = document.createElement('style');"
           "    style.type = 'text/css';"
           "    style.appendChild(document.createTextNode(css));"
           "    head.appendChild(style);"
           "  }"
           "})();";
}

void ShieldsEngine::AddBlockRule(const std::string& domain_pattern) {
    blocked_patterns_.push_back(domain_pattern);
}

} // namespace fireball::win
