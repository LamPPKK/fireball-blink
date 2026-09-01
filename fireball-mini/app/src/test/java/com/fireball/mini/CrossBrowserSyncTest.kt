package com.fireball.mini

import com.fireball.mini.core.engine.BraveSyncHelper
import com.fireball.mini.core.engine.FirefoxSyncHelper
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.HistoryItem
import com.fireball.mini.core.models.SyncProvider
import com.fireball.mini.core.models.SyncStatus
import com.fireball.mini.core.models.TabItem
import com.fireball.mini.data.SyncRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CrossBrowserSyncTest {

    @Test
    fun testBraveSyncWordsGenerationAndValidation() {
        val words = BraveSyncHelper.generateSyncWords()
        assertEquals(24, words.size)

        val wordsString = words.joinToString(" ")
        assertTrue(BraveSyncHelper.isValidSyncWords(wordsString))
        assertFalse(BraveSyncHelper.isValidSyncWords("short invalid phrase"))
    }

    @Test
    fun testBraveSyncPacketEncryptionAndDecryption() {
        val words = BraveSyncHelper.generateSyncWords()
        val bookmarks = listOf(
            BookmarkItem(id = "bm-1", url = "https://brave.com", title = "Brave Browser"),
            BookmarkItem(id = "bm-2", url = "https://github.com", title = "GitHub")
        )
        val history = listOf(
            HistoryItem(id = "h-1", url = "https://news.ycombinator.com", title = "Hacker News")
        )
        val tabs = listOf(
            TabItem(id = "t-1", spaceId = "space-main", profileId = "default", url = "https://rust-lang.org", title = "Rust Lang")
        )

        val encryptedPacket = BraveSyncHelper.buildEncryptedSyncPacket(
            words = words,
            bookmarks = bookmarks,
            history = history,
            openTabs = tabs,
            deviceName = "Fireball Android"
        )
        assertNotNull(encryptedPacket)
        assertTrue(encryptedPacket.contains("iv"))
        assertTrue(encryptedPacket.contains("payload"))

        val (decryptedBm, decryptedHist, decryptedTabs) = BraveSyncHelper.parseEncryptedSyncPacket(words, encryptedPacket)
        assertEquals(2, decryptedBm.size)
        assertEquals("https://brave.com", decryptedBm[0].url)
        assertEquals("Brave Browser", decryptedBm[0].title)

        assertEquals(1, decryptedHist.size)
        assertEquals("https://news.ycombinator.com", decryptedHist[0].url)

        assertEquals(1, decryptedTabs.size)
        assertEquals("https://rust-lang.org", decryptedTabs[0].url)
        assertEquals("Fireball Android", decryptedTabs[0].deviceName)
    }

    @Test
    fun testFirefoxSyncBsoEncryptionAndDecryption() {
        val syncKey = "9f8e7d6c5b4a3f2e1d0c9b8a7f6e5d4c"
        val bookmarks = listOf(
            BookmarkItem(id = "f-bm-1", url = "https://mozilla.org", title = "Mozilla Foundation")
        )
        val tabs = listOf(
            TabItem(id = "f-tab-1", spaceId = "space-main", profileId = "default", url = "https://developer.mozilla.org", title = "MDN Web Docs")
        )

        val bsoBookmarksJson = FirefoxSyncHelper.buildBookmarksCollectionBso(bookmarks, syncKey)
        assertNotNull(bsoBookmarksJson)
        assertTrue(bsoBookmarksJson.contains("payload"))

        val parsedBookmarks = FirefoxSyncHelper.parseBookmarksFromBso(bsoBookmarksJson, syncKey)
        assertEquals(1, parsedBookmarks.size)
        assertEquals("https://mozilla.org", parsedBookmarks[0].url)
        assertEquals("Mozilla Foundation", parsedBookmarks[0].title)

        val bsoTabsJson = FirefoxSyncHelper.buildTabsCollectionBso(tabs, "Firefox Desktop", syncKey)
        val parsedTabs = FirefoxSyncHelper.parseTabsFromBso(bsoTabsJson, syncKey)
        assertEquals(1, parsedTabs.size)
        assertEquals("https://developer.mozilla.org", parsedTabs[0].url)
        assertEquals("Firefox Desktop", parsedTabs[0].deviceName)
    }

    @Test
    fun testSyncRepositoryLifecycle() {
        val syncRepo = SyncRepository()

        // 1. Initial state
        assertEquals(SyncStatus.DISCONNECTED, syncRepo.syncState.value.status)

        // 2. Start Brave Sync Chain
        val words = syncRepo.generateNewBraveSyncChain()
        assertEquals(24, words.size)
        assertEquals(SyncStatus.SYNCED, syncRepo.syncState.value.status)
        assertEquals(SyncProvider.BRAVE_SYNC_CHAIN, syncRepo.syncState.value.provider)

        // 3. Perform Sync
        val result = syncRepo.performSyncNow(
            currentBookmarks = listOf(BookmarkItem(id = "1", url = "https://duckduckgo.com", title = "DuckDuckGo")),
            currentHistory = emptyList(),
            currentTabs = listOf(TabItem(id = "t1", spaceId = "space-main", profileId = "default", url = "https://wikipedia.org", title = "Wikipedia"))
        )
        assertEquals(1, result.newBookmarks.size)
        assertEquals(1, result.remoteTabs.size)
        assertEquals(SyncStatus.SYNCED, syncRepo.syncState.value.status)

        // 4. Disconnect
        syncRepo.disconnect()
        assertEquals(SyncStatus.DISCONNECTED, syncRepo.syncState.value.status)

        // 5. Connect Firefox Sync
        val fxaSuccess = syncRepo.connectFirefoxSync("user@mozilla.org", "secret_sync_key_123456")
        assertTrue(fxaSuccess)
        assertEquals(SyncProvider.FIREFOX_SYNC, syncRepo.syncState.value.provider)
        assertEquals(SyncStatus.SYNCED, syncRepo.syncState.value.status)
    }
}
