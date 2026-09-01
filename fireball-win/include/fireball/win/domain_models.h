#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <chrono>

namespace fireball::win {

enum class TabSection {
    FAVORITE,
    PINNED,
    TODAY
};

enum class TabTier {
    FAVORITES,
    PINNED,
    TODAY,
    BURNER
};

enum class DiscardState {
    LOADED,
    DISCARDED
};

struct Space {
    std::string id;
    std::string name;
    std::string profile_id;
    bool is_burner = false;
    std::string accent_color_hex = "#FF5A1F";
    std::string icon_name = "globe";

    static Space CreateDefaultMain();
    static Space CreateDefaultWork();
    static Space CreateDefaultBurner();
};

struct Profile {
    std::string id;
    std::string name;
    bool is_off_the_record = false;
    std::string user_data_folder;

    static Profile CreateDefault();
    static Profile CreateBurner();
};

struct FireballTab {
    std::string id;
    std::string space_id;
    std::string profile_id;
    std::string url = "https://duckduckgo.com";
    std::string title = "DuckDuckGo";
    TabSection section = TabSection::TODAY;
    DiscardState discard_state = DiscardState::LOADED;
    int64_t last_accessed_timestamp_ms = 0;
    std::string favicon_url;
    std::string preview_thumbnail_path;
    bool is_audible = false;
    bool is_capturing_media = false;
    bool has_unsaved_form_data = false;

    bool IsSafeToDiscard() const {
        return !is_audible && !is_capturing_media && !has_unsaved_form_data && discard_state == DiscardState::LOADED;
    }

    TabTier GetTier(bool is_space_burner) const {
        if (is_space_burner) return TabTier::BURNER;
        switch (section) {
            case TabSection::FAVORITE: return TabTier::FAVORITES;
            case TabSection::PINNED: return TabTier::PINNED;
            case TabSection::TODAY: return TabTier::TODAY;
        }
        return TabTier::TODAY;
    }

    static FireballTab Create(const std::string& space_id, const std::string& profile_id, const std::string& url = "https://duckduckgo.com", TabSection section = TabSection::TODAY);
};

} // namespace fireball::win
