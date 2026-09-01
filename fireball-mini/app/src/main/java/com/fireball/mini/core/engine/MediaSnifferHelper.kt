package com.fireball.mini.core.engine

import com.fireball.mini.core.FireballNativeBridge
import com.fireball.mini.core.models.DiscoveredMedia
import com.fireball.mini.core.models.MediaKind

object MediaSnifferHelper {
    fun inspectResource(
        tabId: String,
        url: String,
        mimeType: String?,
        pageTitle: String
    ): DiscoveredMedia? {
        val kind = FireballNativeBridge.sniffMedia(url, mimeType ?: "") ?: return null
        val title = when (kind) {
            MediaKind.HLS_VOD -> "HLS Stream - $pageTitle"
            MediaKind.DASH_VOD -> "DASH Stream - $pageTitle"
            MediaKind.DIRECT_VIDEO -> "Video - $pageTitle"
            MediaKind.DIRECT_AUDIO -> "Audio - $pageTitle"
        }

        return DiscoveredMedia(
            tabId = tabId,
            sourceUrl = url,
            title = title,
            mediaKind = kind,
            mimeType = mimeType ?: "video/mp4",
            resolution = if (kind == MediaKind.HLS_VOD || kind == MediaKind.DASH_VOD) "Adaptive HD" else "Direct"
        )
    }
}
