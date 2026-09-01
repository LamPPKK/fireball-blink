package com.fireball.mini

import com.fireball.mini.core.engine.UrlCleanerHelper
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class UrlCleanerTest {

    @Test
    fun testBuildUrlOrSearch() {
        val search = UrlCleanerHelper.buildUrlOrSearch("how to build android browser")
        assertTrue(search.contains("duckduckgo.com"))
        assertTrue(search.contains("how%20to%20build") || search.contains("how+to+build"))

        val direct = UrlCleanerHelper.buildUrlOrSearch("github.com")
        assertEquals("https://github.com", direct)

        val localhost = UrlCleanerHelper.buildUrlOrSearch("localhost:8080")
        assertEquals("https://localhost:8080", localhost)
    }

    @Test
    fun testCleanTrackingParams() {
        val dirtyUrl = "https://example.com/article?utm_source=facebook&utm_medium=cpc&id=12345&fbclid=abcdef&mc_eid=9876&gclid=xyz"
        val cleaned = UrlCleanerHelper.cleanUrlIfNeeded(dirtyUrl)
        assertFalse(cleaned.contains("utm_source"))
        assertFalse(cleaned.contains("fbclid"))
        assertFalse(cleaned.contains("mc_eid"))
        assertFalse(cleaned.contains("gclid"))
        assertTrue(cleaned.contains("id=12345"))
    }
}
