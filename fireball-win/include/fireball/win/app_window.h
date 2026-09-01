#pragma once

#include "fireball/win/domain_models.h"
#include "fireball/win/webview2_host.h"
#include "fireball/win/search_engines.h"
#include "fireball/win/password_vault.h"
#include "fireball/win/sync_engine.h"
#include <string>
#include <vector>
#include <memory>

namespace fireball::win {

class AppWindow {
public:
    AppWindow();
    ~AppWindow();

    // Lifecycle
    bool Initialize();
    void Run();

    // Tabs Management
    const FireballTab& CreateNewTab(const std::string& url = "https://duckduckgo.com", TabSection section = TabSection::TODAY);
    bool CloseTab(const std::string& tab_id);
    bool SelectTab(const std::string& tab_id);
    void SelectNextTab();
    void SelectPrevTab();
    std::vector<FireballTab> GetTabsForActiveSpace() const;

    // Spaces Management
    void CreateSpace(const std::string& name, const std::string& accent_color = "#3E9BFF");
    bool SwitchSpace(const std::string& space_id);
    const std::vector<Space>& GetSpaces() const { return spaces_; }
    const Space& GetActiveSpace() const;

    // Omnibox Navigation
    void NavigateFromOmnibox(const std::string& input);

    // Active state getters
    const FireballTab* GetActiveTab() const;
    WebView2Host& GetWebViewHost() { return webview_host_; }
    PasswordVault& GetPasswordVault() { return vault_; }
    SyncEngine& GetSyncEngine() { return sync_engine_; }

    // Keyboard Shortcuts Dispatcher
    enum class ShortcutCommand {
        NEW_TAB,
        CLOSE_TAB,
        NEXT_TAB,
        PREV_TAB,
        FOCUS_OMNIBOX,
        NEW_INCOGNITO_SPACE,
        RELOAD,
        UNKNOWN
    };
    bool HandleShortcut(ShortcutCommand cmd);

private:
    std::vector<Space> spaces_;
    std::vector<Profile> profiles_;
    std::string active_space_id_ = "space-main";

    std::vector<FireballTab> tabs_;
    std::string active_tab_id_;

    WebView2Host webview_host_;
    PasswordVault vault_;
    SyncEngine sync_engine_;
};

} // namespace fireball::win
