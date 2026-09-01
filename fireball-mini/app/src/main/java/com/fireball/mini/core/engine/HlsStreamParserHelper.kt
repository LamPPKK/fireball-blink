package com.fireball.mini.core.engine

import com.fireball.mini.core.models.MediaKind
import com.fireball.mini.core.models.MediaQualityTrack
import java.net.URI

object HlsStreamParserHelper {

    /**
     * Parses an HLS Master Playlist content (#EXTM3U) into distinct quality options.
     */
    fun parseMasterPlaylist(manifestContent: String, masterUrl: String): List<MediaQualityTrack> {
        val tracks = mutableListOf<MediaQualityTrack>()
        val lines = manifestContent.lines().map { it.trim() }.filter { it.isNotEmpty() }

        var currentBandwidth: Long = 0
        var currentResolution: String? = null
        var currentCodecs: String? = null

        for (i in lines.indices) {
            val line = lines[i]
            if (line.startsWith("#EXT-X-STREAM-INF:")) {
                val params = line.removePrefix("#EXT-X-STREAM-INF:")
                currentBandwidth = extractNumericParam(params, "BANDWIDTH")
                currentResolution = extractStringParam(params, "RESOLUTION")
                currentCodecs = extractStringParam(params, "CODECS")
            } else if (!line.startsWith("#") && currentBandwidth > 0) {
                val streamUrl = resolveUrl(masterUrl, line)
                val label = buildQualityLabel(currentResolution, currentBandwidth)
                val estimatedSize = calculateEstimatedSize(currentBandwidth, durationSeconds = 180)

                tracks.add(
                    MediaQualityTrack(
                        label = label,
                        resolution = currentResolution,
                        bandwidthBps = currentBandwidth,
                        estimatedSizeBytes = estimatedSize,
                        codecs = currentCodecs,
                        streamUrl = streamUrl,
                        isAudioOnly = currentResolution == null || currentResolution.startsWith("0x")
                    )
                )
                // Reset for next stream
                currentBandwidth = 0
                currentResolution = null
                currentCodecs = null
            }
        }

        // If no multi-stream tracks were extracted, return a fallback track
        if (tracks.isEmpty()) {
            tracks.add(
                MediaQualityTrack(
                    label = "Auto Quality (Source)",
                    resolution = "Auto",
                    bandwidthBps = 2_500_000,
                    estimatedSizeBytes = 35_000_000,
                    streamUrl = masterUrl
                )
            )
        }

        return tracks.sortedByDescending { it.bandwidthBps }
    }

    /**
     * Creates standard synthetic quality options for direct MP4/WebM videos
     */
    fun generateDirectVideoQualities(sourceUrl: String, estimatedSizeBytes: Long = 25_000_000): List<MediaQualityTrack> {
        return listOf(
            MediaQualityTrack(
                label = "1080p FHD (Source)",
                resolution = "1920x1080",
                bandwidthBps = 4_500_000,
                estimatedSizeBytes = estimatedSizeBytes,
                streamUrl = sourceUrl
            ),
            MediaQualityTrack(
                label = "720p HD",
                resolution = "1280x720",
                bandwidthBps = 2_400_000,
                estimatedSizeBytes = (estimatedSizeBytes * 0.55).toLong(),
                streamUrl = sourceUrl
            ),
            MediaQualityTrack(
                label = "480p SD",
                resolution = "854x480",
                bandwidthBps = 1_200_000,
                estimatedSizeBytes = (estimatedSizeBytes * 0.28).toLong(),
                streamUrl = sourceUrl
            ),
            MediaQualityTrack(
                label = "Audio Only (MP3/AAC)",
                resolution = "Audio",
                bandwidthBps = 160_000,
                estimatedSizeBytes = 4_800_000,
                streamUrl = sourceUrl,
                isAudioOnly = true
            )
        )
    }

    /**
     * Determines the MediaKind from URL or MIME type.
     */
    fun detectMediaKind(url: String, mimeType: String): MediaKind {
        val lowerUrl = url.lowercase()
        val lowerMime = mimeType.lowercase()

        return when {
            lowerUrl.contains(".m3u8") || lowerMime.contains("mpegurl") -> MediaKind.HLS_VOD
            lowerUrl.contains(".mpd") || lowerMime.contains("dash+xml") -> MediaKind.DASH_VOD
            lowerMime.startsWith("audio/") || lowerUrl.endsWith(".mp3") || lowerUrl.endsWith(".aac") || lowerUrl.endsWith(".ogg") || lowerUrl.endsWith(".m4a") -> MediaKind.DIRECT_AUDIO
            else -> MediaKind.DIRECT_VIDEO
        }
    }

    private fun extractNumericParam(paramStr: String, key: String): Long {
        val regex = "$key=(\\d+)".toRegex()
        return regex.find(paramStr)?.groupValues?.get(1)?.toLongOrNull() ?: 0L
    }

    private fun extractStringParam(paramStr: String, key: String): String? {
        val regex = "$key=\"?([^,\"]+)\"?".toRegex()
        return regex.find(paramStr)?.groupValues?.get(1)
    }

    private fun buildQualityLabel(resolution: String?, bandwidthBps: Long): String {
        return when {
            resolution != null && resolution.contains("1920x1080") -> "1080p FHD"
            resolution != null && resolution.contains("1280x720") -> "720p HD"
            resolution != null && resolution.contains("854x480") -> "480p SD"
            resolution != null && resolution.contains("640x360") -> "360p"
            resolution != null -> resolution
            bandwidthBps > 3_000_000 -> "High Quality (${bandwidthBps / 1_000_000} Mbps)"
            bandwidthBps > 1_000_000 -> "Medium Quality (${bandwidthBps / 1_000} Kbps)"
            bandwidthBps > 0 -> "Low Quality (${bandwidthBps / 1_000} Kbps)"
            else -> "Source Stream"
        }
    }

    private fun calculateEstimatedSize(bandwidthBps: Long, durationSeconds: Long): Long {
        return (bandwidthBps / 8) * durationSeconds
    }

    private fun resolveUrl(baseUrl: String, relativeOrAbsolute: String): String {
        return try {
            if (relativeOrAbsolute.startsWith("http://") || relativeOrAbsolute.startsWith("https://")) {
                relativeOrAbsolute
            } else {
                URI(baseUrl).resolve(relativeOrAbsolute).toString()
            }
        } catch (_: Exception) {
            relativeOrAbsolute
        }
    }
}
