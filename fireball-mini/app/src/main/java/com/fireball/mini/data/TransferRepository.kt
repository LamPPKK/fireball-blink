package com.fireball.mini.data

import com.fireball.mini.core.engine.HlsStreamParserHelper
import com.fireball.mini.core.models.DiscoveredMedia
import com.fireball.mini.core.models.MediaKind
import com.fireball.mini.core.models.MediaQualityTrack
import com.fireball.mini.core.models.TransferItem
import com.fireball.mini.core.models.TransferStatus
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap

class TransferRepository(
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO)
) {

    private val _discoveredMedia = MutableStateFlow<List<DiscoveredMedia>>(emptyList())
    val discoveredMedia: StateFlow<List<DiscoveredMedia>> = _discoveredMedia.asStateFlow()

    private val _transfers = MutableStateFlow<List<TransferItem>>(emptyList())
    val transfers: StateFlow<List<TransferItem>> = _transfers.asStateFlow()

    private val downloadJobs = ConcurrentHashMap<String, Job>()

    fun addDiscoveredMedia(media: DiscoveredMedia) {
        val enrichedMedia = if (media.availableQualities.isEmpty()) {
            val qualities = when (media.mediaKind) {
                MediaKind.HLS_VOD -> HlsStreamParserHelper.parseMasterPlaylist("", media.sourceUrl)
                MediaKind.DIRECT_VIDEO -> HlsStreamParserHelper.generateDirectVideoQualities(media.sourceUrl, media.estimatedSizeBytes.takeIf { it > 0 } ?: 25_000_000L)
                MediaKind.DIRECT_AUDIO -> listOf(
                    MediaQualityTrack(
                        label = "HQ Audio 320kbps",
                        resolution = "Audio",
                        bandwidthBps = 320_000,
                        estimatedSizeBytes = 7_200_000L,
                        streamUrl = media.sourceUrl,
                        isAudioOnly = true
                    ),
                    MediaQualityTrack(
                        label = "Standard 128kbps",
                        resolution = "Audio",
                        bandwidthBps = 128_000,
                        estimatedSizeBytes = 3_800_000L,
                        streamUrl = media.sourceUrl,
                        isAudioOnly = true
                    )
                )
                MediaKind.DASH_VOD -> HlsStreamParserHelper.generateDirectVideoQualities(media.sourceUrl, 35_000_000L)
            }
            media.copy(availableQualities = qualities, selectedQualityId = qualities.firstOrNull()?.id)
        } else {
            media
        }

        _discoveredMedia.update { list ->
            if (list.any { it.sourceUrl == enrichedMedia.sourceUrl }) {
                list
            } else {
                (listOf(enrichedMedia) + list).take(32)
            }
        }
    }

    fun selectMediaQuality(mediaId: String, qualityId: String) {
        _discoveredMedia.update { list ->
            list.map {
                if (it.id == mediaId) it.copy(selectedQualityId = qualityId) else it
            }
        }
    }

    fun clearDiscoveredMediaForTab(tabId: String) {
        _discoveredMedia.update { list -> list.filter { it.tabId != tabId } }
    }

    fun startTransfer(
        media: DiscoveredMedia,
        qualityTrack: MediaQualityTrack? = null,
        saveDirectory: String = "/storage/emulated/0/Download"
    ): TransferItem {
        val chosenQuality = qualityTrack 
            ?: media.availableQualities.find { it.id == media.selectedQualityId }
            ?: media.availableQualities.firstOrNull()

        val isAudio = chosenQuality?.isAudioOnly == true || media.mediaKind == MediaKind.DIRECT_AUDIO
        val extension = if (isAudio) ".mp3" else ".mp4"
        val safeTitle = media.title.replace(Regex("[^a-zA-Z0-9._-]"), "_")
        val qualitySuffix = chosenQuality?.label?.replace(" ", "_")?.let { "_$it" } ?: ""
        val targetPath = "$saveDirectory/$safeTitle$qualitySuffix$extension"

        val totalBytes = chosenQuality?.estimatedSizeBytes?.takeIf { it > 0 } ?: when (media.mediaKind) {
            MediaKind.HLS_VOD -> 35_000_000L
            MediaKind.DASH_VOD -> 48_000_000L
            MediaKind.DIRECT_VIDEO -> 18_500_000L
            MediaKind.DIRECT_AUDIO -> 5_200_000L
        }

        val item = TransferItem(
            id = UUID.randomUUID().toString(),
            title = "${media.title} ${chosenQuality?.label?.let { "($it)" } ?: ""}".trim(),
            targetPath = targetPath,
            mediaKind = if (isAudio) MediaKind.DIRECT_AUDIO else media.mediaKind,
            status = TransferStatus.ACTIVE,
            totalBytes = totalBytes,
            downloadedBytes = 0L,
            downloadSpeedBytesPerSec = 3_200_000L,
            selectedQualityLabel = chosenQuality?.label,
            connectionCount = 4
        )

        _transfers.update { listOf(item) + it }
        startDownloadJob(item.id, totalBytes)
        return item
    }

    private fun startDownloadJob(transferId: String, totalBytes: Long) {
        downloadJobs[transferId]?.cancel()
        val job = scope.launch {
            val chunkSize = (totalBytes / 20).coerceAtLeast(200_000L)
            while (isActive) {
                delay(400)
                var isDone = false
                _transfers.update { list ->
                    list.map { item ->
                        if (item.id == transferId && item.status == TransferStatus.ACTIVE) {
                            val newDownloaded = (item.downloadedBytes + chunkSize).coerceAtMost(totalBytes)
                            if (newDownloaded >= totalBytes) {
                                isDone = true
                                item.copy(
                                    downloadedBytes = totalBytes,
                                    status = TransferStatus.COMPLETE,
                                    downloadSpeedBytesPerSec = 0
                                )
                            } else {
                                item.copy(
                                    downloadedBytes = newDownloaded,
                                    downloadSpeedBytesPerSec = (2_800_000L..4_500_000L).random()
                                )
                            }
                        } else {
                            item
                        }
                    }
                }
                if (isDone) break
            }
        }
        downloadJobs[transferId] = job
    }

    fun pauseTransfer(transferId: String) {
        downloadJobs[transferId]?.cancel()
        _transfers.update { list ->
            list.map {
                if (it.id == transferId) it.copy(status = TransferStatus.PAUSED, downloadSpeedBytesPerSec = 0) else it
            }
        }
    }

    fun resumeTransfer(transferId: String) {
        val currentItem = _transfers.value.find { it.id == transferId } ?: return
        _transfers.update { list ->
            list.map {
                if (it.id == transferId) it.copy(status = TransferStatus.ACTIVE, downloadSpeedBytesPerSec = 3_500_000L) else it
            }
        }
        startDownloadJob(transferId, currentItem.totalBytes)
    }

    fun cancelTransfer(transferId: String) {
        downloadJobs[transferId]?.cancel()
        _transfers.update { list ->
            list.map {
                if (it.id == transferId) it.copy(status = TransferStatus.CANCELLED, downloadSpeedBytesPerSec = 0) else it
            }
        }
    }

    fun removeTransfer(transferId: String) {
        downloadJobs[transferId]?.cancel()
        _transfers.update { list -> list.filter { it.id != transferId } }
    }
}
