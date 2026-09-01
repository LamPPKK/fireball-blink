package com.fireball.mini

import com.fireball.mini.core.FireballNativeBridge
import com.fireball.mini.core.engine.CosmeticFilterHelper
import com.fireball.mini.core.userscripts.UserscriptEngine
import org.junit.Assert.*
import org.junit.Test

class BraveShieldsAndUserscriptTest {

    @Test
    fun testBraveShieldsAdNetworksMatching() {
        // Test Vietnamese ABPVN ad networks
        assertEquals(1, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://eclick.vn/banner.js", "tinhte.vn", "eclick.vn"))
        assertEquals(1, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://admicro.vn/delivery", "dantri.com.vn", "admicro.vn"))
        assertEquals(1, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://media.tinhte.vn/ads/ifa2026.jpg", "tinhte.vn", "media.tinhte.vn"))
        assertEquals(1, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://ants.vn/ad.js", "vnexpress.net", "ants.vn"))

        // Test Global ad networks
        assertEquals(1, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://securepubads.g.doubleclick.net/gampad/ads", "example.com", "doubleclick.net"))
        assertEquals(1, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://pagead2.googlesyndication.com/pagead/js/adsbygoogle.js", "example.com", "pagead2.googlesyndication.com"))

        // Test Normal Web Traffic (Must return 0 = ALLOW)
        assertEquals(0, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://tinhte.vn/thread/123", "tinhte.vn", "tinhte.vn"))
        assertEquals(0, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://google.com/search?q=fireball", "google.com", "google.com"))
        assertEquals(0, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://github.com/LamPPKK", "github.com", "github.com"))
    }

    @Test
    fun testTrackersAndMalwareCategories() {
        // Trackers (Category 2)
        assertEquals(2, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://www.google-analytics.com/analytics.js", "example.com", "google-analytics.com"))
        assertEquals(2, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://www.clarity.ms/tag", "example.com", "clarity.ms"))

        // Malware / Miners (Category 4)
        assertEquals(4, FireballNativeBridge.evaluateRequestCategory("profile-1", "https://coinhive.com/lib/miner.js", "example.com", "coinhive.com"))
    }

    @Test
    fun testUserscriptEnginePayloadGeneration() {
        val scripts = UserscriptEngine.getScriptsForUrl("https://youtube.com/watch?v=abc")
        assertTrue(scripts.isNotEmpty())
        assertEquals("default_enhancer", scripts[0].id)

        val payload = UserscriptEngine.generateInjectionPayload("https://tinhte.vn")
        assertTrue(payload.contains("Fireball Userscript"))
        assertTrue(payload.contains("function()"))
    }
}
