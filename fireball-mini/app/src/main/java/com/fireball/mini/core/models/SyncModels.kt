package com.fireball.mini.core.models

import java.util.UUID

enum class SyncProvider {
    BRAVE_SYNC_CHAIN,
    FIREFOX_SYNC,
    FIREBALL_E2EE
}

enum class SyncStatus {
    DISCONNECTED,
    CONNECTING,
    SYNCING,
    SYNCED,
    AUTH_ERROR,
    NETWORK_ERROR
}

data class SyncDevice(
    val id: String = UUID.randomUUID().toString(),
    val name: String,
    val clientType: String = "Android", // "Brave Desktop", "Firefox Mobile", "Fireball Android", etc.
    val lastActiveTimestampMs: Long = System.currentTimeMillis()
)

data class SyncState(
    val status: SyncStatus = SyncStatus.DISCONNECTED,
    val provider: SyncProvider? = null,
    val accountIdentifier: String? = null, // Email for Firefox, Sync Chain ID for Brave
    val braveSyncWords: List<String> = emptyList(),
    val firefoxServerUrl: String = "https://sync.services.mozilla.com/1.5/",
    val lastSyncedTimestampMs: Long = 0,
    val isAutoSyncEnabled: Boolean = true,
    val syncBookmarks: Boolean = true,
    val syncHistory: Boolean = true,
    val syncOpenTabs: Boolean = true,
    val connectedDevices: List<SyncDevice> = emptyList(),
    val errorMessage: String? = null
)

data class RemoteTab(
    val id: String,
    val title: String,
    val url: String,
    val deviceName: String,
    val timestampMs: Long
)

data class SyncMergeResult(
    val newBookmarks: List<BookmarkItem> = emptyList(),
    val newHistory: List<HistoryItem> = emptyList(),
    val remoteTabs: List<RemoteTab> = emptyList(),
    val totalItemsMerged: Int = 0
)
