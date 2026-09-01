package com.fireball.mini.core.models

data class FireballBackupData(
    val version: Int = 1,
    val exportedTimestampMs: Long = System.currentTimeMillis(),
    val spaces: List<Space> = emptyList(),
    val bookmarks: List<BookmarkItem> = emptyList(),
    val history: List<HistoryItem> = emptyList()
)

data class BackupEncryptionBundle(
    val saltBase64: String,
    val ivBase64: String,
    val cipherTextBase64: String,
    val version: Int = 1
)
