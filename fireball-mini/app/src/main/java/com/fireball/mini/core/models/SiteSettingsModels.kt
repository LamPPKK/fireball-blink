package com.fireball.mini.core.models

/**
 * Granular permission status for a site feature.
 */
enum class PermissionStatus {
    ALLOW,
    BLOCK,
    ASK;

    val displayName: String
        get() = when (this) {
            ALLOW -> "Cho phép (Allow)"
            BLOCK -> "Chặn (Block)"
            ASK -> "Hỏi trước (Ask)"
        }
}

/**
 * Supported permission types for websites.
 */
enum class SitePermissionType(val displayName: String, val iconEmoji: String) {
    LOCATION("Vị trí (Location)", "📍"),
    CAMERA("Máy ảnh (Camera)", "📷"),
    MICROPHONE("Microphone", "🎙️"),
    NOTIFICATIONS("Thông báo (Notifications)", "🔔"),
    JAVASCRIPT("JavaScript", "⚡"),
    POPUPS("Cửa sổ bật lên (Popups & Redirects)", "🪟"),
    COOKIES("Cookie bên thứ ba (Third-party Cookies)", "🍪"),
    AUTO_PLAY("Tự động phát Video (Autoplay)", "▶️")
}

/**
 * Permission rule for a specific origin/domain.
 */
data class SitePermission(
    val domain: String,
    val permissionType: SitePermissionType,
    val status: PermissionStatus,
    val updatedTimestamp: Long = System.currentTimeMillis()
)

/**
 * Storage metrics and site data record.
 */
data class SiteStorageInfo(
    val domain: String,
    val cookieCount: Int = 0,
    val estimatedStorageBytes: Long = 0L,
    val isSecureHttps: Boolean = true,
    val lastVisitedTimestamp: Long = System.currentTimeMillis(),
    val permissions: Map<SitePermissionType, PermissionStatus> = emptyMap(),
    val trackersBlockedCount: Int = 0
) {
    val formattedStorageSize: String
        get() {
            return when {
                estimatedStorageBytes >= 1024 * 1024 -> String.format("%.1f MB", estimatedStorageBytes / (1024.0 * 1024.0))
                estimatedStorageBytes >= 1024 -> String.format("%.1f KB", estimatedStorageBytes / 1024.0)
                else -> "$estimatedStorageBytes B"
            }
        }
}
