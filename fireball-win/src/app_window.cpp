#include "fireball/win/app_window.h"
#include <algorithm>

namespace fireball::win {

AppWindow::AppWindow() {
    spaces_.push_back(Space::CreateDefaultMain());
    spaces_.push_back(Space::CreateDefaultWork());
    spaces_.push_back(Space::CreateDefaultBurner());

    profiles_.push_back(Profile::CreateDefault());
    profiles_.push_back(Profile::CreateBurner());

    active_space_id_ = "space-main";

    // Initial Tab in Main Space
    CreateNewTab("https://duckduckgo.com");
}

AppWindow::~AppWindow() = default;

bool AppWindow::Initialize() {
    // Setup WebView2 event hooks
    webview_host_.SetUrlChangedCallback([this](const std::string& url) {
        if (!active_tab_id_.empty()) {
            for (auto& tab : tabs_) {
                if (tab.id == active_tab_id_) {
                    tab.url = url;
                    break;
                }
            }
        }
    });

    webview_host_.SetTitleChangedCallback([this](const std::string& title) {
        if (!active_tab_id_.empty()) {
            for (auto& tab : tabs_) {
                if (tab.id == active_tab_id_) {
                    tab.title = title;
                    break;
                }
            }
        }
    });

    return true;
}

void AppWindow::Run() {
    // In native Win32 runtime: executes standard message loop (GetMessage / TranslateMessage / DispatchMessage)
}

const Space& AppWindow::GetActiveSpace() const {
    for (const auto& s : spaces_) {
        if (s.id == active_space_id_) return s;
    }
    return spaces_.front();
}

std::vector<FireballTab> AppWindow::GetTabsForActiveSpace() const {
    std::vector<FireballTab> result;
    for (const auto& tab : tabs_) {
        if (tab.space_id == active_space_id_) {
            result.push_back(tab);
        }
    }
    return result;
}

const FireballTab* AppWindow::GetActiveTab() const {
    for (const auto& tab : tabs_) {
        if (tab.id == active_tab_id_) return &tab;
    }
    return nullptr;
}

const FireballTab& AppWindow::CreateNewTab(const std::string& url, TabSection section) {
    const Space& active_space = GetActiveSpace();
    FireballTab new_tab = FireballTab::Create(active_space.id, active_space.profile_id, url, section);
    tabs_.push_back(new_tab);
    active_tab_id_ = new_tab.id;
    webview_host_.Navigate(new_tab.url);
    return tabs_.back();
}

bool AppWindow::CloseTab(const std::string& tab_id) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(), [&](const FireballTab& t) {
        return t.id == tab_id;
    });
    if (it == tabs_.end()) return false;

    std::string space_id = it->space_id;
    tabs_.erase(it);

    if (active_tab_id_ == tab_id) {
        auto current_space_tabs = GetTabsForActiveSpace();
        if (!current_space_tabs.empty()) {
            SelectTab(current_space_tabs.front().id);
        } else {
            // Keep at least one tab
            CreateNewTab("https://duckduckgo.com");
        }
    }
    return true;
}

bool AppWindow::SelectTab(const std::string& tab_id) {
    for (auto& tab : tabs_) {
        if (tab.id == tab_id) {
            active_tab_id_ = tab.id;
            active_space_id_ = tab.space_id;
            webview_host_.Navigate(tab.url);
            return true;
        }
    }
    return false;
}

void AppWindow::SelectNextTab() {
    auto current_tabs = GetTabsForActiveSpace();
    if (current_tabs.size() <= 1) return;
    for (size_t i = 0; i < current_tabs.size(); ++i) {
        if (current_tabs[i].id == active_tab_id_) {
            size_t next_idx = (i + 1) % current_tabs.size();
            SelectTab(current_tabs[next_idx].id);
            return;
        }
    }
}

void AppWindow::SelectPrevTab() {
    auto current_tabs = GetTabsForActiveSpace();
    if (current_tabs.size() <= 1) return;
    for (size_t i = 0; i < current_tabs.size(); ++i) {
        if (current_tabs[i].id == active_tab_id_) {
            size_t prev_idx = (i == 0) ? current_tabs.size() - 1 : i - 1;
            SelectTab(current_tabs[prev_idx].id);
            return;
        }
    }
}

void AppWindow::CreateSpace(const std::string& name, const std::string& accent_color) {
    Space s;
    s.id = "space-" + std::to_string(spaces_.size() + 1);
    s.name = name;
    s.profile_id = "profile-" + s.id;
    s.is_burner = false;
    s.accent_color_hex = accent_color;
    s.icon_name = "folder";
    spaces_.push_back(s);
}

bool AppWindow::SwitchSpace(const std::string& space_id) {
    for (const auto& s : spaces_) {
        if (s.id == space_id) {
            active_space_id_ = s.id;
            Profile prof = s.is_burner ? Profile::CreateBurner() : Profile::CreateDefault();
            webview_host_.SwitchToSpace(s, prof);

            auto space_tabs = GetTabsForActiveSpace();
            if (!space_tabs.empty()) {
                active_tab_id_ = space_tabs.front().id;
                webview_host_.Navigate(space_tabs.front().url);
            } else {
                CreateNewTab("https://duckduckgo.com");
            }
            return true;
        }
    }
    return false;
}

void AppWindow::NavigateFromOmnibox(const std::string& input) {
    std::string resolved_url = SearchEngineParser::ParseQuery(input);
    if (!active_tab_id_.empty()) {
        for (auto& tab : tabs_) {
            if (tab.id == active_tab_id_) {
                tab.url = resolved_url;
                break;
            }
        }
    }
    webview_host_.Navigate(resolved_url);
}

bool AppWindow::HandleShortcut(ShortcutCommand cmd) {
    switch (cmd) {
        case ShortcutCommand::NEW_TAB:
            CreateNewTab();
            return true;
        case ShortcutCommand::CLOSE_TAB:
            if (!active_tab_id_.empty()) {
                CloseTab(active_tab_id_);
            }
            return true;
        case ShortcutCommand::NEXT_TAB:
            SelectNextTab();
            return true;
        case ShortcutCommand::PREV_TAB:
            SelectPrevTab();
            return true;
        case ShortcutCommand::FOCUS_OMNIBOX:
            // Handled by UI focus state
            return true;
        case ShortcutCommand::NEW_INCOGNITO_SPACE:
            SwitchSpace("space-burner");
            return true;
        case ShortcutCommand::RELOAD:
            webview_host_.Reload();
            return true;
        case ShortcutCommand::UNKNOWN:
            return false;
    }
    return false;
}

} // namespace fireball::win
