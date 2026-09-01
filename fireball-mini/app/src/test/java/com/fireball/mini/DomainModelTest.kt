package com.fireball.mini

import com.fireball.mini.core.models.TabSection
import com.fireball.mini.data.BrowserRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class DomainModelTest {

    private lateinit var repository: BrowserRepository

    @Before
    fun setUp() {
        repository = BrowserRepository()
    }

    @Test
    fun testInitialSpacesAndTabs() {
        val spaces = repository.spaces.value
        assertEquals(2, spaces.size)
        assertTrue(spaces.any { it.name == "Main" })
        assertTrue(spaces.any { it.name == "Incognito" && it.isBurner })

        val tabs = repository.tabs.value
        assertEquals(1, tabs.size)
        assertEquals("space-main", tabs.first().spaceId)
    }

    @Test
    fun testCreateAndCloseTab() {
        val created = repository.createTab("https://github.com")
        assertEquals(2, repository.tabs.value.size)
        assertEquals("https://github.com", created.url)
        assertEquals(created.id, repository.activeTabId.value)

        repository.closeTab(created.id)
        assertEquals(1, repository.tabs.value.size)
    }

    @Test
    fun testDiscardPriorityOrder() {
        repository.createTab("https://pinned.com", section = TabSection.PINNED)
        repository.createTab("https://today.com", section = TabSection.TODAY)
        repository.createTab("https://active.com", section = TabSection.TODAY)

        val candidates = repository.selectDiscardCandidates(limit = 3)
        assertNotNull(candidates)
        assertTrue(candidates.isNotEmpty())
        val firstCandidate = candidates.first()
        assertEquals(TabSection.TODAY, firstCandidate.section)
    }

    @Test
    fun testTogglePinAndFavorite() {
        val tab = repository.createTab("https://example.com", section = TabSection.TODAY)
        assertEquals(TabSection.TODAY, repository.tabs.value.find { it.id == tab.id }?.section)

        repository.togglePinTab(tab.id)
        assertEquals(TabSection.PINNED, repository.tabs.value.find { it.id == tab.id }?.section)

        repository.togglePinTab(tab.id)
        assertEquals(TabSection.TODAY, repository.tabs.value.find { it.id == tab.id }?.section)
    }
}
