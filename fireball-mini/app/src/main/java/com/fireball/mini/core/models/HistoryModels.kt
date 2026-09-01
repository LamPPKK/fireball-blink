package com.fireball.mini.core.models

data class HistoryItem(
    val id: String,
    val url: String,
    val title: String,
    val visitedAtTimestamp: Long = System.currentTimeMillis(),
    val visitCount: Int = 1,
    val faviconUrl: String? = null
)

data class BookmarkItem(
    val id: String,
    val url: String,
    val title: String,
    val createdAtTimestamp: Long = System.currentTimeMillis(),
    val folder: String = "Mobile Bookmarks"
)
