#include "fireball/win/domain_models.h"
#include <random>
#include <sstream>
#include <iomanip>

namespace fireball::win {

namespace {
std::string GenerateUUID() {
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;

    uint64_t ab = dis(gen);
    uint64_t cd = dis(gen);

    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (ab >> 32);
    ss << "-";
    ss << std::setw(4) << ((ab >> 16) & 0xFFFF);
    ss << "-4";
    ss << std::setw(3) << (ab & 0x0FFF);
    ss << "-";
    ss << std::setw(4) << (0x8000 | ((cd >> 48) & 0x3FFF));
    ss << "-";
    ss << std::setw(12) << (cd & 0xFFFFFFFFFFFFULL);
    return ss.str();
}

int64_t GetCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
} // namespace

Space Space::CreateDefaultMain() {
    Space s;
    s.id = "space-main";
    s.name = "Main";
    s.profile_id = "profile-main";
    s.is_burner = false;
    s.accent_color_hex = "#D8FF3E";
    s.icon_name = "globe";
    return s;
}

Space Space::CreateDefaultWork() {
    Space s;
    s.id = "space-work";
    s.name = "Work";
    s.profile_id = "profile-work";
    s.is_burner = false;
    s.accent_color_hex = "#3E9BFF";
    s.icon_name = "briefcase";
    return s;
}

Space Space::CreateDefaultBurner() {
    Space s;
    s.id = "space-burner";
    s.name = "Incognito";
    s.profile_id = "profile-burner";
    s.is_burner = true;
    s.accent_color_hex = "#FF5A1F";
    s.icon_name = "flame";
    return s;
}

Profile Profile::CreateDefault() {
    Profile p;
    p.id = "profile-main";
    p.name = "Default Profile";
    p.is_off_the_record = false;
    p.user_data_folder = "Profiles\\Main";
    return p;
}

Profile Profile::CreateBurner() {
    Profile p;
    p.id = "profile-burner";
    p.name = "Burner Profile";
    p.is_off_the_record = true;
    p.user_data_folder = "Profiles\\Burner_" + GenerateUUID();
    return p;
}

FireballTab FireballTab::Create(const std::string& space_id, const std::string& profile_id, const std::string& url, TabSection section) {
    FireballTab tab;
    tab.id = GenerateUUID();
    tab.space_id = space_id;
    tab.profile_id = profile_id;
    tab.url = url;
    tab.title = (url == "https://duckduckgo.com") ? "DuckDuckGo" : url;
    tab.section = section;
    tab.discard_state = DiscardState::LOADED;
    tab.last_accessed_timestamp_ms = GetCurrentTimeMs();
    return tab;
}

} // namespace fireball::win
