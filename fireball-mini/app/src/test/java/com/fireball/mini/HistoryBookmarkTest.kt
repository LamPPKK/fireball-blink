package com.fireball.mini

import com.fireball.mini.data.BookmarkRepository
import com.fireball.mini.data.HistoryRepository
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HistoryBookmarkTest {

    @Test
    fun testHistoryRecordAndClear() {
        val repo = HistoryRepository()
        repo.recordVisit("https://example.org", "Example Domain")

        val items = repo.history.value
        assertTrue(items.any { it.url == "https://example.org" })

        val item = items.first { it.url == "https://example.org" }
        assertEquals("Example Domain", item.title)

        repo.removeHistoryItem(item.id)
        assertFalse(repo.history.value.any { it.id == item.id })

        repo.clearAllHistory()
        assertTrue(repo.history.value.isEmpty())
    }

    @Test
    fun testBookmarkToggle() {
        val repo = BookmarkRepository()
        val url = "https://flutter.dev"
        val title = "Flutter - Build apps for any screen"

        assertFalse(repo.isBookmarked(url))

        val isAdded = repo.toggleBookmark(url, title)
        assertTrue(isAdded)
        assertTrue(repo.isBookmarked(url))

        val isRemoved = repo.toggleBookmark(url, title)
        assertFalse(isRemoved)
        assertFalse(repo.isBookmarked(url))
    }
}
