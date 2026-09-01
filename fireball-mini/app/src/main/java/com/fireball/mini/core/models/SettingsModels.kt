package com.fireball.mini.core.models

enum class SearchEngine(
    val displayName: String,
    val searchUrlTemplate: String,
    val homeUrl: String,
    val iconName: String
) {
    DUCKDUCKGO(
        displayName = "DuckDuckGo",
        searchUrlTemplate = "https://duckduckgo.com/?q=%s",
        homeUrl = "https://duckduckgo.com",
        iconName = "privacy_shield"
    ),
    GOOGLE(
        displayName = "Google",
        searchUrlTemplate = "https://www.google.com/search?q=%s",
        homeUrl = "https://www.google.com",
        iconName = "search"
    ),
    BRAVE_SEARCH(
        displayName = "Brave Search",
        searchUrlTemplate = "https://search.brave.com/search?q=%s",
        homeUrl = "https://search.brave.com",
        iconName = "lion"
    ),
    BING(
        displayName = "Microsoft Bing",
        searchUrlTemplate = "https://www.bing.com/search?q=%s",
        homeUrl = "https://www.bing.com",
        iconName = "bing"
    ),
    STARTPAGE(
        displayName = "Startpage",
        searchUrlTemplate = "https://www.startpage.com/sp/search?query=%s",
        homeUrl = "https://www.startpage.com",
        iconName = "lock"
    ),
    ECOSIA(
        displayName = "Ecosia",
        searchUrlTemplate = "https://www.ecosia.org/search?q=%s",
        homeUrl = "https://www.ecosia.org",
        iconName = "tree"
    )
}

enum class AutoArchiveDuration(
    val displayName: String,
    val hours: Int
) {
    HOURS_6("After 6 Hours", 6),
    HOURS_12("After 12 Hours (Default)", 12),
    HOURS_24("After 24 Hours", 24),
    DAYS_3("After 3 Days", 72),
    NEVER("Never Auto-Archive", 0)
}

enum class PreferredVideoQuality(
    val displayName: String
) {
    ASK_EVERY_TIME("Ask Every Time (Recommended)"),
    QUALITY_1080P("Prefer 1080p Full HD"),
    QUALITY_720P("Prefer 720p HD"),
    QUALITY_480P("Prefer 480p SD (Data Saver)"),
    AUDIO_ONLY("Prefer Audio Only (MP3/AAC)")
}

data class BrowserSettings(
    val searchEngine: SearchEngine = SearchEngine.DUCKDUCKGO,
    val isHttpsOnly: Boolean = true,
    val isUrlCleanerEnabled: Boolean = true,
    val isMediaSnifferEnabled: Boolean = true,
    val isDoNotTrackEnabled: Boolean = true,
    val isAutoArchiveEnabled: Boolean = true,
    val autoArchiveDuration: AutoArchiveDuration = AutoArchiveDuration.HOURS_12,
    val preferredVideoQuality: PreferredVideoQuality = PreferredVideoQuality.ASK_EVERY_TIME,
    val downloadThreads: Int = 4,
    val isDesktopModeDefault: Boolean = false,
    val isAggressiveTabDiscarding: Boolean = true
)
