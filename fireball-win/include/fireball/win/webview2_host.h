#pragma once

#include "fireball/win/domain_models.h"
#include "fireball/win/shields_engine.h"
#include <string>
#include <memory>
#include <functional>
#include <vector>

namespace fireball::win {

struct WebView2HostConfig {
    std::string base_data_directory = "%LOCALAPPDATA%\\Fireball";
    bool enable_hardware_acceleration = true;
    bool enable_tracking_protection = true;
    std::string default_user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36 Fireball/1.0";
};

class WebView2Host {
public:
    explicit WebView2Host(const WebView2HostConfig& config = {});
    ~WebView2Host();

    // Resolves isolated User Data Folder for a space profile
    std::string GetUserDataFolderForProfile(const Profile& profile) const;

    // Navigation and state
    void Navigate(const std::string& url);
    void Reload();
    void GoBack();
    void GoForward();
    void Stop();

    std::string GetCurrentUrl() const { return current_url_; }
    std::string GetCurrentTitle() const { return current_title_; }
    bool CanGoBack() const { return can_go_back_; }
    bool CanGoForward() const { return can_go_forward_; }
    bool IsLoading() const { return is_loading_; }
    int GetLoadingProgress() const { return progress_percent_; }

    // Event hooks
    void SetTitleChangedCallback(std::function<void(const std::string&)> cb) { on_title_changed_ = std::move(cb); }
    void SetUrlChangedCallback(std::function<void(const std::string&)> cb) { on_url_changed_ = std::move(cb); }
    void SetProgressCallback(std::function<void(int)> cb) { on_progress_changed_ = std::move(cb); }

    // Space / Profile switching
    void SwitchToSpace(const Space& space, const Profile& profile);

    // Shields & cosmetic injection
    void InjectCosmeticCss();
    const ShieldsEngine& GetShieldsEngine() const { return shields_; }

    // Simulates navigation lifecycle (used in unit tests and host state management)
    void NotifyNavigationStarting(const std::string& url);
    void NotifyNavigationCompleted(bool success, int http_status = 200);
    void NotifyDocumentTitleChanged(const std::string& title);
    void NotifyProgress(int progress);

private:
    WebView2HostConfig config_;
    ShieldsEngine shields_;
    std::string current_space_id_ = "space-main";
    std::string current_profile_id_ = "profile-main";
    std::string current_url_ = "https://duckduckgo.com";
    std::string current_title_ = "DuckDuckGo";
    bool can_go_back_ = false;
    bool can_go_forward_ = false;
    bool is_loading_ = false;
    int progress_percent_ = 0;

    std::vector<std::string> history_;
    size_t history_index_ = 0;

    std::function<void(const std::string&)> on_title_changed_;
    std::function<void(const std::string&)> on_url_changed_;
    std::function<void(int)> on_progress_changed_;
};

} // namespace fireball::win
