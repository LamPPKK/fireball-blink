package com.fireball.mini.core.models

import java.util.UUID

enum class PresentationLayout {
    CLASSIC,
    FLOATING,
    VERTICAL,
    GRID
}

enum class TabSection {
    FAVORITE,
    PINNED,
    TODAY
}

enum class DiscardState {
    LOADED,
    DISCARDED
}

data class Profile(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val isOffTheRecord: Boolean = false,
    val autoArchiveThresholdHours: Int = 12
)

data class Space(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val profileId: String,
    val isBurner: Boolean = false,
    val accentColorHex: String = "#FF5A1F",
    val iconName: String = "globe"
)

data class TabItem(
    val id: String = UUID.randomUUID().toString(),
    val spaceId: String,
    val profileId: String,
    val url: String,
    val title: String = "New Tab",
    val section: TabSection = TabSection.TODAY,
    val discardState: DiscardState = DiscardState.LOADED,
    val lastAccessedTimestampMs: Long = System.currentTimeMillis(),
    val faviconUrl: String? = null,
    val previewThumbnailPath: String? = null,
    val isAudible: Boolean = false,
    val isCapturingMedia: Boolean = false,
    val hasUnsavedFormData: Boolean = false
) {
    val isSafeToDiscard: Boolean
        get() = !isAudible && !isCapturingMedia && !hasUnsavedFormData && discardState == DiscardState.LOADED
}
