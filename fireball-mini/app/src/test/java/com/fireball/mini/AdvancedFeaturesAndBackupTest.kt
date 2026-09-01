package com.fireball.mini

import com.fireball.mini.core.engine.BrowserShortcutAction
import com.fireball.mini.core.engine.DesktopKeyShortcutHandler
import com.fireball.mini.core.engine.EncryptedBackupManager
import com.fireball.mini.core.engine.HlsStreamParserHelper
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.FireballBackupData
import com.fireball.mini.core.models.HistoryItem
import com.fireball.mini.core.models.MediaKind
import com.fireball.mini.core.models.Space
import com.fireball.mini.data.BookmarkRepository
import com.fireball.mini.data.BrowserRepository
import com.fireball.mini.data.TransferRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.util.UUID

class AdvancedFeaturesAndBackupTest {

    @Test
    fun testHlsMasterPlaylistParsing() {
        val sampleManifest = """
            #EXTM3U
            #EXT-X-VERSION:3
            #EXT-X-STREAM-INF:BANDWIDTH=5000000,RESOLUTION=1920x1080,CODECS="avc1.640028,mp4a.40.2"
            1080p/index.m3u8
            #EXT-X-STREAM-INF:BANDWIDTH=2500000,RESOLUTION=1280x720,CODECS="avc1.4d401f,mp4a.40.2"
            720p/index.m3u8
            #EXT-X-STREAM-INF:BANDWIDTH=1200000,RESOLUTION=854x480,CODECS="avc1.4d401e,mp4a.40.2"
            480p/index.m3u8
            #EXT-X-STREAM-INF:BANDWIDTH=192000,RESOLUTION=0x0,CODECS="mp4a.40.2"
            audio/index.m3u8
        """.trimIndent()

        val tracks = HlsStreamParserHelper.parseMasterPlaylist(sampleManifest, "https://stream.example.com/video/master.m3u8")
        assertEquals(4, tracks.size)

        val track1080 = tracks.find { it.resolution == "1920x1080" }
        assertNotNull(track1080)
        assertEquals("1080p FHD", track1080?.label)
        assertEquals("https://stream.example.com/video/1080p/index.m3u8", track1080?.streamUrl)

        val trackAudio = tracks.find { it.isAudioOnly }
        assertNotNull(trackAudio)
    }

    @Test
    fun testDirectVideoQualityGeneration() {
        val qualities = HlsStreamParserHelper.generateDirectVideoQualities("https://example.com/movie.mp4", 50_000_000L)
        assertEquals(4, qualities.size)
        assertTrue(qualities.any { it.label.contains("1080p") })
        assertTrue(qualities.any { it.isAudioOnly })
    }

    @Test
    fun testNetscapeBookmarkHtmlExportAndImport() {
        val bookmarks = listOf(
            BookmarkItem(id = "1", url = "https://github.com", title = "GitHub Platform"),
            BookmarkItem(id = "2", url = "https://news.ycombinator.com", title = "Hacker News & Tech")
        )

        val exportedHtml = EncryptedBackupManager.exportBookmarksToNetscapeHtml(bookmarks)
        assertTrue(exportedHtml.contains("<!DOCTYPE NETSCAPE-Bookmark-file-1>"))
        assertTrue(exportedHtml.contains("HREF=\"https://github.com\""))
        assertTrue(exportedHtml.contains("GitHub Platform"))

        val imported = EncryptedBackupManager.importBookmarksFromNetscapeHtml(exportedHtml)
        assertEquals(2, imported.size)
        assertEquals("https://github.com", imported[0].url)
        assertEquals("GitHub Platform", imported[0].title)
        assertEquals("https://news.ycombinator.com", imported[1].url)
        assertEquals("Hacker News & Tech", imported[1].title)
    }

    @Test
    fun testEncryptedBackupAndRestore() {
        val backupData = FireballBackupData(
            version = 1,
            exportedTimestampMs = 1772450000000L,
            spaces = listOf(
                Space(id = "space-1", name = "Research", profileId = "default-profile", iconName = "school"),
                Space(id = "space-2", name = "Secret", profileId = "burner-profile", isBurner = true, iconName = "fire")
            ),
            bookmarks = listOf(
                BookmarkItem(id = "b1", url = "https://duckduckgo.com", title = "DuckDuckGo")
            ),
            history = listOf(
                HistoryItem(id = "h1", url = "https://wikipedia.org", title = "Wikipedia")
            )
        )

        val passphrase = "MySecretMasterPassword2026!"
        val encryptedBundle = EncryptedBackupManager.encryptBackup(backupData, passphrase)
        assertNotNull(encryptedBundle)
        assertTrue(encryptedBundle.contains("cipherText"))
        assertTrue(encryptedBundle.contains("salt"))
        assertTrue(encryptedBundle.contains("iv"))

        val decrypted = EncryptedBackupManager.decryptBackup(encryptedBundle, passphrase)
        assertEquals(1, decrypted.version)
        assertEquals(2, decrypted.spaces.size)
        assertEquals("Research", decrypted.spaces[0].name)
        assertEquals("Secret", decrypted.spaces[1].name)
        assertEquals(1, decrypted.bookmarks.size)
        assertEquals("https://duckduckgo.com", decrypted.bookmarks[0].url)
        assertEquals(1, decrypted.history.size)
        assertEquals("Wikipedia", decrypted.history[0].title)
    }

    @Test
    fun testTabCyclingInSpace() {
        val repo = BrowserRepository()
        val space = repo.createSpace("Work Space", iconName = "work")
        val tab1 = repo.createTab("https://site1.com", spaceId = space.id)
        val tab2 = repo.createTab("https://site2.com", spaceId = space.id)
        val tab3 = repo.createTab("https://site3.com", spaceId = space.id)

        repo.selectTab(tab1.id)
        assertEquals(tab1.id, repo.activeTabId.value)

        repo.selectNextTabInSpace()
        assertEquals(tab2.id, repo.activeTabId.value)

        repo.selectNextTabInSpace()
        assertEquals(tab3.id, repo.activeTabId.value)

        // Loop around
        repo.selectNextTabInSpace()
        assertEquals(tab1.id, repo.activeTabId.value)

        // Previous tab
        repo.selectPrevTabInSpace()
        assertEquals(tab3.id, repo.activeTabId.value)

        // Direct index
        repo.selectTabByIndexInSpace(1)
        assertEquals(tab2.id, repo.activeTabId.value)
    }

    @Test
    fun testTransferRepositoryQualitySelection() {
        val transferRepo = TransferRepository()
        val media = com.fireball.mini.core.models.DiscoveredMedia(
            tabId = "tab-1",
            sourceUrl = "https://example.com/stream/index.m3u8",
            title = "Awesome Video Stream",
            mediaKind = MediaKind.HLS_VOD,
            mimeType = "application/x-mpegURL"
        )

        transferRepo.addDiscoveredMedia(media)
        val discoveredList = transferRepo.discoveredMedia.value
        assertEquals(1, discoveredList.size)
        assertTrue(discoveredList[0].availableQualities.isNotEmpty())

        val chosenQuality = discoveredList[0].availableQualities.first()
        val transfer = transferRepo.startTransfer(discoveredList[0], chosenQuality)
        assertNotNull(transfer)
        assertEquals(com.fireball.mini.core.models.TransferStatus.ACTIVE, transfer.status)
        assertTrue(transfer.totalBytes > 0)
    }
}
