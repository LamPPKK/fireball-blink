package com.fireball.mini

import com.fireball.mini.core.models.AutoArchiveDuration
import com.fireball.mini.core.models.PreferredVideoQuality
import com.fireball.mini.core.models.SearchEngineDefaults
import com.fireball.mini.ui.viewmodels.BrowserViewModel
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import java.net.URLEncoder

class SettingsAndSearchTest {

    @Test
    fun testSearchEngineTemplates() {
        val query = "Kotlin Multiplatform 2026"
        val encoded = URLEncoder.encode(query, "UTF-8")

        val ddgUrl = SearchEngineDefaults.DUCKDUCKGO.searchUrlTemplate.replace("%s", encoded)
        assertTrue(ddgUrl.contains("duckduckgo.com/?q="))
        assertTrue(ddgUrl.contains("Kotlin"))

        val braveUrl = SearchEngineDefaults.BRAVE.searchUrlTemplate.replace("%s", encoded)
        assertTrue(braveUrl.contains("search.brave.com/search?q="))

        val googleUrl = SearchEngineDefaults.GOOGLE.searchUrlTemplate.replace("%s", encoded)
        assertTrue(googleUrl.contains("google.com/search?q="))

        val bingUrl = SearchEngineDefaults.BING.searchUrlTemplate.replace("%s", encoded)
        assertTrue(bingUrl.contains("bing.com/search?q="))

        val startpageUrl = SearchEngineDefaults.STARTPAGE.searchUrlTemplate.replace("%s", encoded)
        assertTrue(startpageUrl.contains("startpage.com/sp/search?query="))

        val ecosiaUrl = SearchEngineDefaults.ECOSIA.searchUrlTemplate.replace("%s", encoded)
        assertTrue(ecosiaUrl.contains("ecosia.org/search?q="))
    }

    @Test
    fun testBrowserSettingsUpdates() {
        val viewModel = BrowserViewModel()

        assertEquals(SearchEngineDefaults.DUCKDUCKGO, viewModel.browserSettings.value.searchEngine)
        assertTrue(viewModel.browserSettings.value.isHttpsOnly)
        assertTrue(viewModel.browserSettings.value.isUrlCleanerEnabled)
        assertTrue(viewModel.browserSettings.value.isMediaSnifferEnabled)

        viewModel.setSearchEngine(SearchEngineDefaults.BRAVE)
        assertEquals(SearchEngineDefaults.BRAVE, viewModel.browserSettings.value.searchEngine)

        viewModel.setHttpsOnly(false)
        assertFalse(viewModel.browserSettings.value.isHttpsOnly)

        viewModel.setUrlCleanerEnabled(false)
        assertFalse(viewModel.browserSettings.value.isUrlCleanerEnabled)

        viewModel.setMediaSnifferEnabled(false)
        assertFalse(viewModel.browserSettings.value.isMediaSnifferEnabled)

        viewModel.setAutoArchiveDuration(AutoArchiveDuration.DAYS_3)
        assertEquals(AutoArchiveDuration.DAYS_3, viewModel.browserSettings.value.autoArchiveDuration)

        viewModel.setPreferredVideoQuality(PreferredVideoQuality.QUALITY_1080P)
        assertEquals(PreferredVideoQuality.QUALITY_1080P, viewModel.browserSettings.value.preferredVideoQuality)

        viewModel.setDownloadThreads(8)
        assertEquals(8, viewModel.browserSettings.value.downloadThreads)
    }
}
