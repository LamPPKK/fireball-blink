#include "fireball/win/search_engines.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace fireball::win {

namespace {
const std::vector<SearchBang> kAllBangs = {
    {"!g", "Google", "https://www.google.com/search?q=%s", "google"},
    {"!b", "Brave", "https://search.brave.com/search?q=%s", "shield"},
    {"!yt", "YouTube", "https://www.youtube.com/results?search_query=%s", "play"},
    {"!gh", "GitHub", "https://github.com/search?q=%s", "code"},
    {"!w", "Wikipedia", "https://en.wikipedia.org/wiki/Special:Search?search=%s", "book"},
    {"!k", "Kagi", "https://kagi.com/search?q=%s", "kagi"},
    {"!sp", "Startpage", "https://www.startpage.com/sp/search?query=%s", "startpage"},
    {"!e", "Ecosia", "https://www.ecosia.org/search?q=%s", "leaf"},
    {"!r", "Reddit", "https://www.reddit.com/search/?q=%s", "reddit"}
};

std::string Trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
} // namespace

const std::vector<SearchBang>& SearchEngineParser::GetAllBangs() {
    return kAllBangs;
}

std::string SearchEngineParser::UrlEncode(const std::string& value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (char c : value) {
        if (isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << '+';
        } else {
            escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

bool SearchEngineParser::IsValidUrl(const std::string& input) {
    std::string trimmed = Trim(input);
    if (trimmed.rfind("http://", 0) == 0 || trimmed.rfind("https://", 0) == 0 || trimmed.rfind("file:///", 0) == 0 || trimmed.rfind("about:", 0) == 0) {
        return true;
    }
    if (trimmed.find(' ') == std::string::npos && trimmed.find('.') != std::string::npos) {
        size_t dot_pos = trimmed.find_last_of('.');
        if (dot_pos != std::string::npos && dot_pos < trimmed.length() - 1) {
            std::string tld = trimmed.substr(dot_pos + 1);
            size_t slash_pos = tld.find('/');
            if (slash_pos != std::string::npos) {
                tld = tld.substr(0, slash_pos);
            }
            if (tld.length() >= 2 && std::all_of(tld.begin(), tld.end(), [](char ch) { return isalpha(static_cast<unsigned char>(ch)); })) {
                return true;
            }
        }
    }
    return false;
}

std::string SearchEngineParser::ParseQuery(const std::string& input, const std::string& default_engine_template) {
    std::string trimmed = Trim(input);
    if (trimmed.empty()) {
        return "https://duckduckgo.com";
    }

    if (IsValidUrl(trimmed)) {
        if (trimmed.rfind("http://", 0) != 0 && trimmed.rfind("https://", 0) != 0 && trimmed.rfind("file:///", 0) != 0 && trimmed.rfind("about:", 0) != 0) {
            return "https://" + trimmed;
        }
        return trimmed;
    }

    for (const auto& bang : kAllBangs) {
        if (trimmed.rfind(bang.prefix + " ", 0) == 0) {
            std::string raw_query = Trim(trimmed.substr(bang.prefix.length() + 1));
            std::string encoded = UrlEncode(raw_query);
            std::string target = bang.query_template;
            size_t placeholder = target.find("%s");
            if (placeholder != std::string::npos) {
                target.replace(placeholder, 2, encoded);
            }
            return target;
        }
    }

    std::string encoded = UrlEncode(trimmed);
    std::string target = default_engine_template;
    size_t placeholder = target.find("%s");
    if (placeholder != std::string::npos) {
        target.replace(placeholder, 2, encoded);
    }
    return target;
}

} // namespace fireball::win
