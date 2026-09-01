package com.fireball.mini

import com.fireball.mini.core.FireballNativeBridge
import com.fireball.mini.core.models.BlockerMode
import com.fireball.mini.data.ShieldsRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ShieldsFilterTest {

    @Test
    fun testMultiCategoryEvaluation() {
        // 1 = Ads
        val adResult = FireballNativeBridge.evaluateRequestCategory("default", "https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js", "example.com", "pagead2.googlesyndication.com")
        assertEquals(1, adResult)

        // 2 = Trackers & Telemetry
        val trackerResult = FireballNativeBridge.evaluateRequestCategory("default", "https://google-analytics.com/analytics.js", "example.com", "google-analytics.com")
        assertEquals(2, trackerResult)

        // 3 = Cookie Banners & Annoyances
        val cookieResult = FireballNativeBridge.evaluateRequestCategory("default", "https://cdn.cookielaw.org/scripttemplates/otSDKStub.js", "example.com", "cdn.cookielaw.org")
        assertEquals(3, cookieResult)

        // 4 = Cryptominer / Malware
        val minerResult = FireballNativeBridge.evaluateRequestCategory("default", "https://coinhive.com/lib/coinhive.min.js", "example.com", "coinhive.com")
        assertEquals(4, minerResult)

        // 5 = Social Pixels
        val socialResult = FireballNativeBridge.evaluateRequestCategory("default", "https://connect.facebook.net/en_US/fbevents.js", "example.com", "connect.facebook.net")
        assertEquals(5, socialResult)

        // 0 = Allowed Content
        val allowResult = FireballNativeBridge.evaluateRequestCategory("default", "https://example.com/main.js", "example.com", "example.com")
        assertEquals(0, allowResult)
    }

    @Test
    fun testFilterSubscriptionsAndModes() {
        val repo = ShieldsRepository()
        val filters = repo.filterLists.value
        assertEquals(6, filters.size)
        assertTrue(filters.all { it.isEnabled })

        // Toggle a filter list
        val target = filters.first().id
        repo.toggleFilterList(target)
        val updated = repo.filterLists.value.find { it.id == target }
        assertFalse(updated?.isEnabled ?: true)

        // Blocker Mode Switching
        repo.setBlockerMode(BlockerMode.AGGRESSIVE)
        assertEquals(BlockerMode.AGGRESSIVE, repo.blockerMode.value)
    }

    @Test
    fun testExtendedTrackingUrlCleaning() {
        val dirtyUrl = "https://news.ycombinator.com?utm_source=twitter&utm_medium=social&utm_id=123&fbclid=IwAR0&gclid=Cj0K&_ga=GA1.2.3&real_param=active#main"
        val cleaned = FireballNativeBridge.cleanUrl(dirtyUrl)
        assertEquals("https://news.ycombinator.com?real_param=active#main", cleaned)
    }
}
