package com.fireball.mini.data

import com.fireball.mini.core.models.HistoryItem
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.util.UUID

class HistoryRepository {

    private val _history = MutableStateFlow<List<HistoryItem>>(
        listOf(
            HistoryItem(
                id = UUID.randomUUID().toString(),
                url = "https://duckduckgo.com",
                title = "DuckDuckGo — Privacy, simplified.",
                visitedAtTimestamp = System.currentTimeMillis() - 60000,
                visitCount = 5
            ),
            HistoryItem(
                id = UUID.randomUUID().toString(),
                url = "https://github.com",
                title = "GitHub: Let’s build from here",
                visitedAtTimestamp = System.currentTimeMillis() - 3600000,
                visitCount = 12
            ),
            HistoryItem(
                id = UUID.randomUUID().toString(),
                url = "https://news.ycombinator.com",
                title = "Hacker News",
                visitedAtTimestamp = System.currentTimeMillis() - 7200000,
                visitCount = 8
            )
        )
    )
    val history: StateFlow<List<HistoryItem>> = _history.asStateFlow()

    fun recordVisit(url: String, title: String?) {
        if (url.isEmpty() || url == "about:blank") return
        val finalTitle = title?.takeIf { it.isNotBlank() } ?: url

        _history.update { currentList ->
            val existing = currentList.find { it.url == url }
            if (existing != null) {
                val updated = existing.copy(
                    title = finalTitle,
                    visitedAtTimestamp = System.currentTimeMillis(),
                    visitCount = existing.visitCount + 1
                )
                listOf(updated) + currentList.filter { it.url != url }
            } else {
                val newItem = HistoryItem(
                    id = UUID.randomUUID().toString(),
                    url = url,
                    title = finalTitle,
                    visitedAtTimestamp = System.currentTimeMillis(),
                    visitCount = 1
                )
                listOf(newItem) + currentList
            }
        }
    }

    fun removeHistoryItem(id: String) {
        _history.update { list -> list.filter { it.id != id } }
    }

    fun clearAllHistory() {
        _history.value = emptyList()
    }

    fun restoreHistory(historyList: List<HistoryItem>) {
        _history.value = historyList
    }
}
