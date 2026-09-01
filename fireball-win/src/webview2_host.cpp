#include "fireball/win/webview2_host.h"
#include <iostream>

namespace fireball::win {

WebView2Host::WebView2Host(const WebView2HostConfig& config)
    : config_(config) {
    history_.push_back(current_url_);
    history_index_ = 0;
}

WebView2Host::~WebView2Host() = default;

std::string WebView2Host::GetUserDataFolderForProfile(const Profile& profile) const {
    if (profile.is_off_the_record) {
        // Ephemeral temp directory for Burner / Incognito space
        return config_.base_data_directory + "\\Temp\\" + profile.user_data_folder;
    }
    return config_.base_data_directory + "\\" + profile.user_data_folder;
}

void WebView2Host::Navigate(const std::string& url) {
    std::string clean_url = config_.enable_tracking_protection ? shields_.CleanTrackingParameters(url) : url;
    NotifyNavigationStarting(clean_url);

    if (history_index_ + 1 < history_.size()) {
        history_.erase(history_.begin() + history_index_ + 1, history_.end());
    }
    history_.push_back(clean_url);
    history_index_ = history_.size() - 1;

    can_go_back_ = (history_index_ > 0);
    can_go_forward_ = false;

    NotifyNavigationCompleted(true, 200);
}

void WebView2Host::Reload() {
    Navigate(current_url_);
}

void WebView2Host::GoBack() {
    if (can_go_back_ && history_index_ > 0) {
        history_index_--;
        current_url_ = history_[history_index_];
        can_go_back_ = (history_index_ > 0);
        can_go_forward_ = (history_index_ + 1 < history_.size());
        if (on_url_changed_) on_url_changed_(current_url_);
    }
}

void WebView2Host::GoForward() {
    if (can_go_forward_ && history_index_ + 1 < history_.size()) {
        history_index_++;
        current_url_ = history_[history_index_];
        can_go_back_ = (history_index_ > 0);
        can_go_forward_ = (history_index_ + 1 < history_.size());
        if (on_url_changed_) on_url_changed_(current_url_);
    }
}

void WebView2Host::Stop() {
    is_loading_ = false;
    progress_percent_ = 100;
    if (on_progress_changed_) on_progress_changed_(100);
}

void WebView2Host::SwitchToSpace(const Space& space, const Profile& profile) {
    current_space_id_ = space.id;
    current_profile_id_ = profile.id;
    // Session isolation: reset in-memory history when switching to fresh space
    history_.clear();
    current_url_ = "https://duckduckgo.com";
    history_.push_back(current_url_);
    history_index_ = 0;
    can_go_back_ = false;
    can_go_forward_ = false;
    if (on_url_changed_) on_url_changed_(current_url_);
}

void WebView2Host::InjectCosmeticCss() {
    std::string script = shields_.GenerateCosmeticCssScript();
    // Executed via ICoreWebView2::ExecuteScript or AddScriptToExecuteOnDocumentCreated
}

void WebView2Host::NotifyNavigationStarting(const std::string& url) {
    current_url_ = url;
    is_loading_ = true;
    progress_percent_ = 15;
    if (on_url_changed_) on_url_changed_(current_url_);
    if (on_progress_changed_) on_progress_changed_(progress_percent_);
}

void WebView2Host::NotifyNavigationCompleted(bool success, int http_status) {
    (void)http_status;
    is_loading_ = false;
    progress_percent_ = 100;
    if (success) {
        InjectCosmeticCss();
    }
    if (on_progress_changed_) on_progress_changed_(100);
}

void WebView2Host::NotifyDocumentTitleChanged(const std::string& title) {
    current_title_ = title;
    if (on_title_changed_) on_title_changed_(current_title_);
}

void WebView2Host::NotifyProgress(int progress) {
    progress_percent_ = progress;
    is_loading_ = (progress < 100);
    if (on_progress_changed_) on_progress_changed_(progress_percent_);
}

} // namespace fireball::win
