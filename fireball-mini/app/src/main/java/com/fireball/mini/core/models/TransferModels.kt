package com.fireball.mini.core.models

import java.util.UUID

enum class MediaKind {
    DIRECT_AUDIO,
    DIRECT_VIDEO,
    HLS_VOD,
    DASH_VOD
}

enum class TransferStatus {
    QUEUED,
    ACTIVE,
    PAUSED,
    COMPLETE,
    FAILED,
    CANCELLED
}

data class MediaQualityTrack(
    val id: String = UUID.randomUUID().toString(),
    val label: String,
    val resolution: String? = null,
    val bandwidthBps: Long = 0,
    val estimatedSizeBytes: Long = 0,
    val codecs: String? = null,
    val streamUrl: String,
    val isAudioOnly: Boolean = false
)

data class DiscoveredMedia(
    val id: String = UUID.randomUUID().toString(),
    val tabId: String,
    val sourceUrl: String,
    val title: String,
    val mediaKind: MediaKind,
    val mimeType: String,
    val estimatedSizeBytes: Long = 0,
    val durationSeconds: Long = 0,
    val resolution: String? = null,
    val timestampMs: Long = System.currentTimeMillis(),
    val availableQualities: List<MediaQualityTrack> = emptyList(),
    val selectedQualityId: String? = null
)

data class TransferItem(
    val id: String = UUID.randomUUID().toString(),
    val title: String,
    val targetPath: String,
    val mediaKind: MediaKind,
    val status: TransferStatus = TransferStatus.QUEUED,
    val totalBytes: Long = 0,
    val downloadedBytes: Long = 0,
    val downloadSpeedBytesPerSec: Long = 0,
    val connectionCount: Int = 4,
    val selectedQualityLabel: String? = null,
    val errorCode: String? = null,
    val startedTimestampMs: Long = System.currentTimeMillis()
) {
    val progressFraction: Float
        get() = if (totalBytes > 0) (downloadedBytes.toFloat() / totalBytes.toFloat()).coerceIn(0f, 1f) else 0f

    val estimatedRemainingSeconds: Long
        get() {
            if (downloadSpeedBytesPerSec <= 0 || totalBytes <= downloadedBytes) return 0L
            val remainingBytes = totalBytes - downloadedBytes
            return remainingBytes / downloadSpeedBytesPerSec
        }
}
