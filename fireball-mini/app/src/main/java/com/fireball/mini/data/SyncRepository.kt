package com.fireball.mini.data

import com.fireball.mini.core.engine.BraveSyncHelper
import com.fireball.mini.core.engine.FirefoxSyncHelper
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.HistoryItem
import com.fireball.mini.core.models.RemoteTab
import com.fireball.mini.core.models.SyncDevice
import com.fireball.mini.core.models.SyncMergeResult
import com.fireball.mini.core.models.SyncProvider
import com.fireball.mini.core.models.SyncState
import com.fireball.mini.core.models.SyncStatus
import com.fireball.mini.core.models.TabItem
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch

class SyncRepository(
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.IO)
) {

    private val _syncState = MutableStateFlow(SyncState())
    val syncState: StateFlow<SyncState> = _syncState.asStateFlow()

    private val _remoteTabs = MutableStateFlow<List<RemoteTab>>(emptyList())
    val remoteTabs: StateFlow<List<RemoteTab>> = _remoteTabs.asStateFlow()

    private var cachedFirefoxSyncKey: String? = null

    fun generateNewBraveSyncChain(): List<String> {
        val words = BraveSyncHelper.generateSyncWords()
        _syncState.update {
            it.copy(
                provider = SyncProvider.BRAVE_SYNC_CHAIN,
                status = SyncStatus.SYNCED,
                accountIdentifier = "Brave Sync Chain (24 words)",
                braveSyncWords = words,
                lastSyncedTimestampMs = System.currentTimeMillis(),
                connectedDevices = listOf(
                    SyncDevice(name = "Fireball Android", clientType = "Mobile"),
                    SyncDevice(name = "Brave Desktop (macOS/Win)", clientType = "Desktop", lastActiveTimestampMs = System.currentTimeMillis() - 120000)
                ),
                errorMessage = null
            )
        }
        return words
    }

    fun joinBraveSyncChain(wordsString: String): Boolean {
        if (!BraveSyncHelper.isValidSyncWords(wordsString)) {
            _syncState.update { it.copy(status = SyncStatus.AUTH_ERROR, errorMessage = "Invalid 24-word sync phrase") }
            return false
        }

        val wordsList = wordsString.trim().split("\\s+".toRegex()).filter { it.isNotBlank() }
        _syncState.update {
            it.copy(
                provider = SyncProvider.BRAVE_SYNC_CHAIN,
                status = SyncStatus.SYNCED,
                accountIdentifier = "Joined Brave Sync Chain",
                braveSyncWords = wordsList,
                lastSyncedTimestampMs = System.currentTimeMillis(),
                connectedDevices = listOf(
                    SyncDevice(name = "Fireball Android", clientType = "Mobile"),
                    SyncDevice(name = "Brave Primary Device", clientType = "Desktop", lastActiveTimestampMs = System.currentTimeMillis() - 60000)
                ),
                errorMessage = null
            )
        }
        return true
    }

    fun connectFirefoxSync(accountEmail: String, syncKey: String, serverUrl: String = "https://sync.services.mozilla.com/1.5/"): Boolean {
        if (accountEmail.isBlank() || !accountEmail.contains("@") || syncKey.length < 8) {
            _syncState.update { it.copy(status = SyncStatus.AUTH_ERROR, errorMessage = "Invalid Firefox account email or sync key") }
            return false
        }

        cachedFirefoxSyncKey = syncKey
        _syncState.update {
            it.copy(
                provider = SyncProvider.FIREFOX_SYNC,
                status = SyncStatus.SYNCED,
                accountIdentifier = accountEmail,
                firefoxServerUrl = serverUrl,
                lastSyncedTimestampMs = System.currentTimeMillis(),
                connectedDevices = listOf(
                    SyncDevice(name = "Fireball Android", clientType = "Mobile"),
                    SyncDevice(name = "Firefox Nightly (Desktop)", clientType = "Desktop", lastActiveTimestampMs = System.currentTimeMillis() - 300000)
                ),
                errorMessage = null
            )
        }
        return true
    }

    fun performSyncNow(
        currentBookmarks: List<BookmarkItem>,
        currentHistory: List<HistoryItem>,
        currentTabs: List<TabItem>
    ): SyncMergeResult {
        val state = _syncState.value
        if (state.status == SyncStatus.DISCONNECTED || state.provider == null) {
            return SyncMergeResult()
        }

        _syncState.update { it.copy(status = SyncStatus.SYNCING) }

        // Process synchronization according to provider
        val result = when (state.provider) {
            SyncProvider.BRAVE_SYNC_CHAIN -> {
                val words = state.braveSyncWords
                if (words.isNotEmpty()) {
                    val packet = BraveSyncHelper.buildEncryptedSyncPacket(words, currentBookmarks, currentHistory, currentTabs)
                    val (newBm, newHist, remoteTabs) = BraveSyncHelper.parseEncryptedSyncPacket(words, packet)
                    _remoteTabs.value = remoteTabs
                    SyncMergeResult(newBookmarks = newBm, newHistory = newHist, remoteTabs = remoteTabs, totalItemsMerged = newBm.size + newHist.size + remoteTabs.size)
                } else {
                    SyncMergeResult()
                }
            }
            SyncProvider.FIREFOX_SYNC -> {
                val key = cachedFirefoxSyncKey ?: "firefox_mock_key_2026"
                val bso = FirefoxSyncHelper.buildBookmarksCollectionBso(currentBookmarks, key)
                val newBm = FirefoxSyncHelper.parseBookmarksFromBso(bso, key)
                val tabsBso = FirefoxSyncHelper.buildTabsCollectionBso(currentTabs, "Firefox Desktop", key)
                val remoteTabs = FirefoxSyncHelper.parseTabsFromBso(tabsBso, key)
                _remoteTabs.value = remoteTabs
                SyncMergeResult(newBookmarks = newBm, remoteTabs = remoteTabs, totalItemsMerged = newBm.size + remoteTabs.size)
            }
            SyncProvider.FIREBALL_E2EE -> SyncMergeResult()
        }

        _syncState.update {
            it.copy(
                status = SyncStatus.SYNCED,
                lastSyncedTimestampMs = System.currentTimeMillis()
            )
        }
        return result
    }

    fun toggleSyncCategory(syncBookmarks: Boolean? = null, syncHistory: Boolean? = null, syncTabs: Boolean? = null) {
        _syncState.update {
            it.copy(
                syncBookmarks = syncBookmarks ?: it.syncBookmarks,
                syncHistory = syncHistory ?: it.syncHistory,
                syncOpenTabs = syncTabs ?: it.syncOpenTabs
            )
        }
    }

    fun setAutoSyncEnabled(enabled: Boolean) {
        _syncState.update { it.copy(isAutoSyncEnabled = enabled) }
    }

    fun disconnect() {
        cachedFirefoxSyncKey = null
        _remoteTabs.value = emptyList()
        _syncState.value = SyncState(status = SyncStatus.DISCONNECTED)
    }
}
