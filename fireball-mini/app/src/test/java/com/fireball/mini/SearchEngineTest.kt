package com.fireball.mini

import com.fireball.mini.core.models.SearchEngine
import com.fireball.mini.core.models.SearchEngineDefaults
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class SearchEngineTest {

    @Test
    fun testBuildSearchUrl() {
        val ddg = SearchEngineDefaults.DUCKDUCKGO
        val url = ddg.buildSearchUrl("kotlin compose m3")
        assertEquals("https://duckduckgo.com/?q=kotlin+compose+m3", url)

        val google = SearchEngineDefaults.GOOGLE
        val gUrl = google.buildSearchUrl("fireball browser")
        assertEquals("https://www.google.com/search?q=fireball+browser", gUrl)
    }

    @Test
    fun testResolveQueryDirectUrl() {
        val url = "https://github.com/LamPPKK/fireball-blink"
        val resolved = SearchEngineDefaults.resolveQueryOrUrl(url)
        assertEquals(url, resolved)

        val httpUrl = "http://example.com/test"
        assertEquals(httpUrl, SearchEngineDefaults.resolveQueryOrUrl(httpUrl))
    }

    @Test
    fun testResolveDomainLikeInput() {
        assertEquals("https://github.com", SearchEngineDefaults.resolveQueryOrUrl("github.com"))
        assertEquals("https://duckduckgo.com/about", SearchEngineDefaults.resolveQueryOrUrl("duckduckgo.com/about"))
        assertEquals("https://localhost:8080", SearchEngineDefaults.resolveQueryOrUrl("localhost:8080"))
    }

    @Test
    fun testResolveBangShortcuts() {
        // !g -> Google
        val gResolved = SearchEngineDefaults.resolveQueryOrUrl("!g android material 3")
        assertEquals("https://www.google.com/search?q=android+material+3", gResolved)

        // !b -> Brave
        val bResolved = SearchEngineDefaults.resolveQueryOrUrl("!b rust adblock")
        assertEquals("https://search.brave.com/search?q=rust+adblock", bResolved)

        // !yt -> YouTube
        val ytResolved = SearchEngineDefaults.resolveQueryOrUrl("!yt lofi beats")
        assertEquals("https://www.youtube.com/results?search_query=lofi+beats", ytResolved)

        // !w -> Wikipedia
        val wResolved = SearchEngineDefaults.resolveQueryOrUrl("!w Chromium")
        assertEquals("https://en.wikipedia.org/wiki/Special:Search?search=Chromium", wResolved)

        // !gh -> GitHub
        val ghResolved = SearchEngineDefaults.resolveQueryOrUrl("!gh Jetpack Compose")
        assertEquals("https://github.com/search?q=Jetpack+Compose", ghResolved)
    }

    @Test
    fun testCustomEngineQuery() {
        val custom = SearchEngine(
            id = "custom_searxng",
            name = "SearXNG",
            searchUrlTemplate = "https://searx.be/search?q=%s",
            isCustom = true
        )
        val customUrl = custom.buildSearchUrl("privacy search")
        assertEquals("https://searx.be/search?q=privacy+search", customUrl)
    }
}
