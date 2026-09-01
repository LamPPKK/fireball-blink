package com.fireball.mini

import com.fireball.mini.core.models.AutoArchiveDuration
import com.fireball.mini.core.models.PreferredVideoQuality
import com.fireball.mini.core.models.SearchEngine
import com.fireball.mini.data.BookmarkRepository
import com.fireball.mini.data.BrowserRepository
import com.fireball.mini.data.HistoryRepository
import com.fireball.mini.data.ShieldsRepository
import com.fireball.mini.data.TransferRepository
import com.fireball.mini.ui.viewmodels.BrowserViewModel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.URLEncoder

class SettingsAndSearchTest {

    @Test
    fun testSearchEngineTemplates() {
        val query = "Kotlin Multiplatform 2026"
        val encoded = URLEncoder.encode(query, "UTF-8")

        val ddgUrl = SearchEngine.DUCKDUCKGO.searchUrlTemplate.format(encoded)
        assertTrue(ddgUrl.contains("duckduckgo.com/?q="))
        assertTrue(ddgUrl.contains("Kotlin"))

        val braveUrl = SearchEngine.BRAVE_SEARCH.searchUrlTemplate.format(encoded)
        assertTrue(braveUrl.contains("search.brave.com/search?q="))

        val googleUrl = SearchEngine.GOOGLE.searchUrlTemplate.format(encoded)
        assertTrue(googleUrl.contains("google.com/search?q="))

        val bingUrl = SearchEngine.BING.searchUrlTemplate.format(encoded)
        assertTrue(bingUrl.contains("bing.com/search?q="))

        val startpageUrl = SearchEngine.STARTPAGE.searchUrlTemplate.format(encoded)
        assertTrue(startpageUrl.contains("startpage.com/sp/search?query="))

        val ecosiaUrl = SearchEngine.ECOSIA.searchUrlTemplate.format(encoded)
        assertTrue(ecosiaUrl.contains("ecosia.org/search?q="))
    }

    @Test
    fun testBrowserSettingsUpdates() {
        val viewModel = BrowserViewModel()

        assertEquals(SearchEngine.DUCKDUCKGO, viewModel.browserSettings.value.searchEngine)
        assertTrue(viewModel.browserSettings.value.isHttpsOnly)
        assertTrue(viewModel.browserSettings.value.isUrlCleanerEnabled)
        assertTrue(viewModel.browserSettings.value.isMediaSnifferEnabled)

        viewModel.setSearchEngine(SearchEngine.BRAVE_SEARCH)
        assertEquals(SearchEngine.BRAVE_SEARCH, viewModel.browserSettings.value.searchEngine)

        viewModel.setHttpsOnly(false)
        assertFalse(viewModel.browserSettings.value.isHttpsOnly)

        viewModel.setUrlCleanerEnabled(false)
        assertFalse(viewModel.browserSettings.value.isUrlCleanerEnabled)

        viewModel.setDoNotTrackEnabled(false)
        assertFalse(viewModel.browserSettings.value.isDoNotTrackEnabled)

        viewModel.setAutoArchiveDuration(AutoArchiveDuration.HOURS_24)
        assertEquals(AutoArchiveDuration.HOURS_24, viewModel.browserSettings.value.autoArchiveDuration)

        viewModel.setPreferredVideoQuality(PreferredVideoQuality.QUALITY_1080P)
        assertEquals(PreferredVideoQuality.QUALITY_1080P, viewModel.browserSettings.value.preferredVideoQuality)

        viewModel.setDownloadThreads(8)
        assertEquals(8, viewModel.browserSettings.value.downloadThreads)
    }

    @Test
    fun testSubmitUrlWithSearchEngine() {
        val viewModel = BrowserViewModel()
        viewModel.setSearchEngine(SearchEngine.BRAVE_SEARCH)

        var loadedUrl: String? = null
        viewModel.submitUrl("android webview security") { url ->
            loadedUrl = url
        }

        assertNotNull(loadedUrl)
        assertTrue(loadedUrl?.contains("search.brave.com") == true)
        assertTrue(loadedUrl?.contains("android") == true)
    }

    @Test
    fun testClearBrowsingData() {
        val historyRepo = HistoryRepository()
        val bookmarkRepo = BookmarkRepository()
        val transferRepo = TransferRepository()
        val viewModel = BrowserViewModel(
            historyRepo = historyRepo,
            bookmarkRepo = bookmarkRepo,
            transferRepo = transferRepo
        )

        historyRepo.recordVisit("https://site1.com", "Site 1")
        bookmarkRepo.toggleBookmark("https://site2.com", "Site 2")
        assertTrue(historyRepo.history.value.isNotEmpty())
        assertTrue(bookmarkRepo.bookmarks.value.isNotEmpty())

        viewModel.clearBrowsingData(clearHistory = true, clearBookmarks = true, clearTransfers = true)
        assertEquals(0, historyRepo.history.value.size)
        assertEquals(0, bookmarkRepo.bookmarks.value.size)
    }
}
