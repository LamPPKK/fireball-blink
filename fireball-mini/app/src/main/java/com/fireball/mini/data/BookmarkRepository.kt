package com.fireball.mini.data

import com.fireball.mini.core.models.BookmarkItem
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.util.UUID

class BookmarkRepository {

    private val _bookmarks = MutableStateFlow<List<BookmarkItem>>(
        listOf(
            BookmarkItem(
                id = UUID.randomUUID().toString(),
                url = "https://github.com",
                title = "GitHub"
            ),
            BookmarkItem(
                id = UUID.randomUUID().toString(),
                url = "https://duckduckgo.com",
                title = "DuckDuckGo"
            ),
            BookmarkItem(
                id = UUID.randomUUID().toString(),
                url = "https://news.ycombinator.com",
                title = "Hacker News"
            ),
            BookmarkItem(
                id = UUID.randomUUID().toString(),
                url = "https://reddit.com",
                title = "Reddit"
            )
        )
    )
    val bookmarks: StateFlow<List<BookmarkItem>> = _bookmarks.asStateFlow()

    fun isBookmarked(url: String): Boolean {
        return _bookmarks.value.any { it.url == url }
    }

    fun toggleBookmark(url: String, title: String?): Boolean {
        if (url.isEmpty() || url == "about:blank") return false
        val currentlyBookmarked = isBookmarked(url)

        if (currentlyBookmarked) {
            _bookmarks.update { list -> list.filter { it.url != url } }
            return false
        } else {
            val item = BookmarkItem(
                id = UUID.randomUUID().toString(),
                url = url,
                title = title?.takeIf { it.isNotBlank() } ?: url
            )
            _bookmarks.update { listOf(item) + it }
            return true
        }
    }

    fun removeBookmark(id: String) {
        _bookmarks.update { list -> list.filter { it.id != id } }
    }

    fun exportBookmarksHtml(): String {
        return com.fireball.mini.core.engine.EncryptedBackupManager.exportBookmarksToNetscapeHtml(_bookmarks.value)
    }

    fun importBookmarksHtml(htmlString: String): Int {
        val imported = com.fireball.mini.core.engine.EncryptedBackupManager.importBookmarksFromNetscapeHtml(htmlString)
        if (imported.isNotEmpty()) {
            _bookmarks.update { current ->
                val existingUrls = current.map { it.url }.toSet()
                val newItems = imported.filter { it.url !in existingUrls }
                newItems + current
            }
        }
        return imported.size
    }

    fun restoreBookmarks(bookmarksList: List<BookmarkItem>) {
        _bookmarks.value = bookmarksList
    }
}
