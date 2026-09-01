package com.fireball.mini

import com.fireball.mini.data.BrowserRepository
import com.fireball.mini.data.ShieldsRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TabletAndSecurityFeaturesTest {

    @Test
    fun testPopupAndRedirectBlockingStats() {
        val shieldsRepo = ShieldsRepository()

        // Verify initial state
        assertTrue(shieldsRepo.isPopupBlockingEnabled.value)
        assertTrue(shieldsRepo.isRedirectBlockingEnabled.value)
        val initialAds = shieldsRepo.stats.value.totalAdsBlocked
        val initialAnnoyances = shieldsRepo.stats.value.totalAnnoyancesBlocked

        // Record popup block
        shieldsRepo.recordPopupBlocked()
        assertEquals(1L, shieldsRepo.stats.value.totalPopupsBlocked)
        assertEquals(initialAnnoyances + 1, shieldsRepo.stats.value.totalAnnoyancesBlocked)

        // Record redirect block
        shieldsRepo.recordRedirectBlocked()
        assertEquals(1L, shieldsRepo.stats.value.totalRedirectsBlocked)
        assertEquals(initialAds + 1, shieldsRepo.stats.value.totalAdsBlocked)

        // Toggle settings
        shieldsRepo.setPopupBlockingEnabled(false)
        assertFalse(shieldsRepo.isPopupBlockingEnabled.value)

        shieldsRepo.setRedirectBlockingEnabled(false)
        assertFalse(shieldsRepo.isRedirectBlockingEnabled.value)
    }

    @Test
    fun testTabletMultiTabManagement() {
        val browserRepo = BrowserRepository()
        assertEquals(2, browserRepo.spaces.value.size) // Main + Incognito

        val initialTabs = browserRepo.tabs.value.size
        assertEquals(1, initialTabs)

        // Create multiple tabs as in tablet strip
        val tab2 = browserRepo.createTab("https://news.ycombinator.com")
        val tab3 = browserRepo.createTab("https://github.com")
        assertEquals(3, browserRepo.tabs.value.size)
        assertEquals(tab3.id, browserRepo.activeTabId.value)

        // Select tab2
        browserRepo.selectTab(tab2.id)
        assertEquals(tab2.id, browserRepo.activeTabId.value)

        // Close tab2
        browserRepo.closeTab(tab2.id)
        assertEquals(2, browserRepo.tabs.value.size)
    }
}
