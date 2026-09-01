#pragma once

#include <string>
#include <vector>

namespace fireball::win {

struct SearchBang {
    std::string prefix;
    std::string name;
    std::string query_template;
    std::string icon;
};

class SearchEngineParser {
public:
    static std::string ParseQuery(const std::string& input, const std::string& default_engine_template = "https://duckduckgo.com/?q=%s");
    static const std::vector<SearchBang>& GetAllBangs();
    static bool IsValidUrl(const std::string& input);
    static std::string UrlEncode(const std::string& value);
};

} // namespace fireball::win
