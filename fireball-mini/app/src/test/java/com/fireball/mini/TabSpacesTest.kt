package com.fireball.mini

import com.fireball.mini.data.BrowserRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Test

class TabSpacesTest {

    @Test
    fun testSpaceCreationDeletionAndSelection() {
        val repo = BrowserRepository()
        val spacesBefore = repo.spaces.value.size
        assertEquals(2, spacesBefore) // Main and Incognito

        val newSpace = repo.createSpace("Research & Crypto", iconName = "bolt", accentColorHex = "#4285F4")
        assertEquals(3, repo.spaces.value.size)
        assertEquals(newSpace.id, repo.activeSpaceId.value)
        assertEquals("bolt", newSpace.iconName)
        assertEquals("#4285F4", newSpace.accentColorHex)

        // Update space icon and color
        repo.updateSpace(newSpace.id, name = "Crypto Hub", iconName = "rocket", accentColorHex = "#00F0FF")
        val updated = repo.spaces.value.find { it.id == newSpace.id }
        assertNotNull(updated)
        assertEquals("Crypto Hub", updated?.name)
        assertEquals("rocket", updated?.iconName)
        assertEquals("#00F0FF", updated?.accentColorHex)

        repo.selectSpace("space-main")
        assertEquals("space-main", repo.activeSpaceId.value)

        // Delete custom space
        repo.deleteSpace(newSpace.id)
        assertEquals(2, repo.spaces.value.size)
    }

    @Test
    fun testTabMultiSpaceMoveAndDuplicate() {
        val repo = BrowserRepository()
        val tab = repo.createTab("https://example.com", spaceId = "space-main")

        repo.moveTabToSpace(tab.id, "space-incognito")
        val movedTab = repo.tabs.value.find { it.id == tab.id }
        assertNotNull(movedTab)
        assertEquals("space-incognito", movedTab?.spaceId)

        val duplicated = repo.duplicateTab(tab.id)
        assertNotNull(duplicated)
        assertEquals("https://example.com", duplicated?.url)
        assertEquals("space-incognito", duplicated?.spaceId)
    }

    @Test
    fun testCloseAllTabsInSpace() {
        val repo = BrowserRepository()
        repo.createTab("https://a.com", spaceId = "space-main")
        repo.createTab("https://b.com", spaceId = "space-main")

        repo.closeAllTabsInSpace("space-main")
        val remaining = repo.tabs.value.filter { it.spaceId == "space-main" }
        assertEquals(1, remaining.size)
    }
}
