package com.fireball.mini

import com.fireball.mini.core.engine.MediaSnifferHelper
import com.fireball.mini.core.models.MediaKind
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class MediaSnifferTest {

    @Test
    fun testDetectHlsStream() {
        val media = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://cdn.video.example/master.m3u8",
            mimeType = "application/x-mpegURL",
            pageTitle = "Streaming Video"
        )
        assertNotNull(media)
        assertEquals(MediaKind.HLS_VOD, media?.mediaKind)
    }

    @Test
    fun testDetectDashStream() {
        val media = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://cdn.video.example/manifest.mpd",
            mimeType = "application/dash+xml",
            pageTitle = "Dash Video"
        )
        assertNotNull(media)
        assertEquals(MediaKind.DASH_VOD, media?.mediaKind)
    }

    @Test
    fun testDetectDirectMp4() {
        val media = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://cdn.video.example/movie_1080p.mp4",
            mimeType = "video/mp4",
            pageTitle = "Action Movie"
        )
        assertNotNull(media)
        assertEquals(MediaKind.DIRECT_VIDEO, media?.mediaKind)
    }

    @Test
    fun testIgnoreStandardWebResources() {
        val media = MediaSnifferHelper.inspectResource(
            tabId = "tab-1",
            url = "https://example.com/bundle.js",
            mimeType = "application/javascript",
            pageTitle = "Webpage"
        )
        assertNull(media)
    }
}
