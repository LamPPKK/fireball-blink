package com.fireball.mini.core.models

enum class EgressMode {
    DIRECT,
    WARP,
    TOR
}

enum class BlockerMode {
    STANDARD,
    AGGRESSIVE,
    DISABLED
}

enum class FilterCategory {
    ADS,
    TRACKERS,
    ANNOYANCES,
    SECURITY,
    SOCIAL,
    COSMETIC
}

data class FilterListSubscription(
    val id: String,
    val name: String,
    val description: String,
    val category: FilterCategory,
    val rulesCount: Int,
    val isEnabled: Boolean = true,
    val iconName: String = "shield"
)

data class SiteShieldsPolicy(
    val hostname: String,
    val isEnabled: Boolean = true,
    val blockerMode: BlockerMode = BlockerMode.STANDARD,
    val blockTrackers: Boolean = true,
    val blockFingerprinting: Boolean = true,
    val blockScripts: Boolean = false,
    val blockCookieNotices: Boolean = true,
    val blockPopups: Boolean = true,
    val blockRedirects: Boolean = true,
    val upgradeHttps: Boolean = true
)

data class ShieldsStats(
    val totalAdsBlocked: Long = 0,
    val totalTrackersBlocked: Long = 0,
    val totalAnnoyancesBlocked: Long = 0,
    val totalMalwareBlocked: Long = 0,
    val totalPopupsBlocked: Long = 0,
    val totalRedirectsBlocked: Long = 0,
    val totalBandwidthSavedBytes: Long = 0,
    val totalTimeSavedMs: Long = 0
)

data class EgressStatus(
    val mode: EgressMode = EgressMode.DIRECT,
    val isConnected: Boolean = true,
    val egressIp: String = "Direct Connection",
    val latencyMs: Int = 18,
    val hasLeakProtection: Boolean = true
)
