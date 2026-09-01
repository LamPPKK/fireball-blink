package com.fireball.mini.core.models

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
    val searchEngine: SearchEngine = SearchEngineDefaults.DUCKDUCKGO,
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
