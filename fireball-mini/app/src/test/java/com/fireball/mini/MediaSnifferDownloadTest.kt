package com.fireball.mini

import com.fireball.mini.core.engine.MediaSnifferHelper
import com.fireball.mini.core.models.DiscoveredMedia
import com.fireball.mini.core.models.MediaKind
import com.fireball.mini.core.models.TransferStatus
import com.fireball.mini.data.TransferRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MediaSnifferDownloadTest {

    @Test
    fun testMediaSnifferDetection() {
        // Test MP4 video link detection
        val video = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4",
            mimeType = "video/mp4",
            pageTitle = "Big Buck Bunny"
        )
        assertNotNull(video)
        assertEquals(MediaKind.DIRECT_VIDEO, video?.mediaKind)
        assertTrue(video?.title?.contains("Big Buck Bunny") == true)

        // Test MP3 audio link detection
        val audio = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://example.com/podcast/episode42.mp3",
            mimeType = "audio/mpeg",
            pageTitle = "Tech Podcast"
        )
        assertNotNull(audio)
        assertEquals(MediaKind.DIRECT_AUDIO, audio?.mediaKind)

        // Test HLS stream VOD detection
        val hls = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://example.com/live/master.m3u8",
            mimeType = "application/x-mpegURL",
            pageTitle = "Live Stream"
        )
        assertNotNull(hls)
        assertEquals(MediaKind.HLS_VOD, hls?.mediaKind)
    }

    @Test
    fun testTransferRepositoryDownloadLifecycle() {
        val repo = TransferRepository()
        val sampleMedia = DiscoveredMedia(
            tabId = "tab-1",
            sourceUrl = "https://example.com/video.mp4",
            title = "Test Video Stream",
            mediaKind = MediaKind.DIRECT_VIDEO,
            mimeType = "video/mp4"
        )

        // Add discovered media
        repo.addDiscoveredMedia(sampleMedia)
        assertEquals(1, repo.discoveredMedia.value.size)

        // Start transfer
        val transfer = repo.startTransfer(sampleMedia)
        assertEquals(TransferStatus.ACTIVE, transfer.status)
        assertEquals(1, repo.transfers.value.size)

        // Pause transfer
        repo.pauseTransfer(transfer.id)
        val paused = repo.transfers.value.first()
        assertEquals(TransferStatus.PAUSED, paused.status)

        // Resume transfer
        repo.resumeTransfer(transfer.id)
        val resumed = repo.transfers.value.first()
        assertEquals(TransferStatus.ACTIVE, resumed.status)

        // Cancel transfer
        repo.cancelTransfer(transfer.id)
        val cancelled = repo.transfers.value.first()
        assertEquals(TransferStatus.CANCELLED, cancelled.status)
    }
}
