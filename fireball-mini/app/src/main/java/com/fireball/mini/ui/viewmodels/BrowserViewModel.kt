package com.fireball.mini.ui.viewmodels

import android.content.Context
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.fireball.mini.core.ai.ArticleExtractorHelper
import com.fireball.mini.core.ai.TextToSpeechEngine
import com.fireball.mini.core.engine.UrlCleanerHelper
import com.fireball.mini.core.models.BlockerMode
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.DiscoveredMedia
import com.fireball.mini.core.models.EgressMode
import com.fireball.mini.core.models.HistoryItem
import com.fireball.mini.core.models.MediaQualityTrack
import com.fireball.mini.core.models.ReaderTheme
import com.fireball.mini.core.models.Space
import com.fireball.mini.core.models.AutoArchiveDuration
import com.fireball.mini.core.models.BrowserSettings
import com.fireball.mini.core.models.PreferredVideoQuality
import com.fireball.mini.core.models.SearchEngine
import com.fireball.mini.core.models.SearchEngineDefaults
import com.fireball.mini.core.models.SavedCredential
import com.fireball.mini.core.models.DecryptedCredential
import com.fireball.mini.core.models.SitePermissionType
import com.fireball.mini.core.models.PermissionStatus
import com.fireball.mini.core.models.SiteStorageInfo
import com.fireball.mini.core.models.TabItem
import com.fireball.mini.core.models.TtsPlaybackStatus
import com.fireball.mini.core.models.TtsState
import com.fireball.mini.core.models.SyncProvider
import com.fireball.mini.core.models.SyncState
import com.fireball.mini.core.models.SyncStatus
import com.fireball.mini.data.AiAssistantRepository
import com.fireball.mini.data.BookmarkRepository
import com.fireball.mini.data.BrowserRepository
import com.fireball.mini.data.HistoryRepository
import com.fireball.mini.data.ShieldsRepository
import com.fireball.mini.data.SyncRepository
import com.fireball.mini.data.TransferRepository
import com.fireball.mini.data.SearchEngineRepository
import com.fireball.mini.data.SiteSettingsRepository
import com.fireball.mini.data.PasswordVaultRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import kotlinx.coroutines.launch
import java.net.URLEncoder

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers

data class BrowserUiState(
    val currentUrl: String = "https://duckduckgo.com",
    val currentTitle: String = "DuckDuckGo",
    val inputUrlText: String = "https://duckduckgo.com",
    val isEditingUrl: Boolean = false,
    val pageProgress: Int = 0,
    val isLoading: Boolean = false,
    val canGoBack: Boolean = false,
    val canGoForward: Boolean = false,
    val activeTab: TabItem? = null,
    val activeSpace: Space? = null,
    val totalTabCount: Int = 1,
    val spaceTabs: List<TabItem> = emptyList(),
    val discoveredMediaCount: Int = 0,
    val adsBlockedThisSession: Long = 0,
    val isBurnerMode: Boolean = false,
    val isBookmarked: Boolean = false,
    val engineType: com.fireball.mini.core.models.BrowserEngineType = com.fireball.mini.core.models.BrowserEngineType.NATIVE_WEBVIEW
)

class BrowserViewModel(
    val beamClient: com.fireball.mini.core.beam.BeamStreamingClient = com.fireball.mini.core.beam.BeamStreamingClient(),
    private val browserRepo: BrowserRepository = BrowserRepository(),
    private val shieldsRepo: ShieldsRepository = ShieldsRepository(),
    private val transferRepo: TransferRepository = TransferRepository(),
    private val historyRepo: HistoryRepository = HistoryRepository(),
    private val bookmarkRepo: BookmarkRepository = BookmarkRepository(),
    private val aiRepo: AiAssistantRepository = AiAssistantRepository(),
    private val syncRepo: SyncRepository = SyncRepository(),
    private val searchEngineRepo: SearchEngineRepository = SearchEngineRepository(),
    private val siteSettingsRepo: SiteSettingsRepository = SiteSettingsRepository(),
    private val passwordVaultRepo: PasswordVaultRepository = PasswordVaultRepository(),
    private val scope: CoroutineScope = CoroutineScope(Dispatchers.Default)
) : ViewModel() {

    private val _browserSettings = MutableStateFlow(BrowserSettings())
    val browserSettings: StateFlow<BrowserSettings> = _browserSettings.asStateFlow()

    val spaces = browserRepo.spaces
    val tabs = browserRepo.tabs
    val activeSpaceId = browserRepo.activeSpaceId
    val activeTabId = browserRepo.activeTabId
    val stats = shieldsRepo.stats
    val egressStatus = shieldsRepo.egressStatus
    val filterLists = shieldsRepo.filterLists
    val blockerMode = shieldsRepo.blockerMode
    val isPopupBlockingEnabled = shieldsRepo.isPopupBlockingEnabled
    val isRedirectBlockingEnabled = shieldsRepo.isRedirectBlockingEnabled
    val discoveredMedia = transferRepo.discoveredMedia
    val transfers = transferRepo.transfers
    val history = historyRepo.history
    val bookmarks = bookmarkRepo.bookmarks
    val syncState = syncRepo.syncState
    val remoteTabs = syncRepo.remoteTabs

    val availableSearchEngines = searchEngineRepo.availableEngines
    val savedCredentials = passwordVaultRepo.credentials
    val sitePermissions = siteSettingsRepo.sitePermissions

    // AI Assistant & Reader Mode States
    val currentArticle = aiRepo.currentArticle
    val aiSummary = aiRepo.summary
    val isGeneratingSummary = aiRepo.isGeneratingSummary
    val aiChatMessages = aiRepo.chatMessages
    val isAiChatLoading = aiRepo.isChatLoading
    val isReaderModeActive = aiRepo.isReaderModeActive
    val readerTheme = aiRepo.readerTheme
    val readerFontSizeSp = aiRepo.readerFontSizeSp

    private val _ttsState = MutableStateFlow(TtsState())
    val ttsState: StateFlow<TtsState> = _ttsState.asStateFlow()
    private var ttsEngine: TextToSpeechEngine? = null

    private val _uiState = MutableStateFlow(BrowserUiState())
    val uiState: StateFlow<BrowserUiState> = _uiState.asStateFlow()

    init {
        scope.launch {
            launch {
                browserRepo.spaces.collect { syncState() }
            }
            launch {
                browserRepo.activeSpaceId.collect { syncState() }
            }
            launch {
                shieldsRepo.stats.collect { syncState() }
            }
            launch {
                transferRepo.discoveredMedia.collect { syncState() }
            }
        }
    }

    private fun syncState() {
        val currentTabs = browserRepo.tabs.value
        val currentActiveTabId = browserRepo.activeTabId.value
        val currentSpaces = browserRepo.spaces.value
        val currentActiveSpaceId = browserRepo.activeSpaceId.value
        val currentStats = shieldsRepo.stats.value
        val currentMediaList = transferRepo.discoveredMedia.value

        val activeTab = currentTabs.find { it.id == currentActiveTabId }
        val activeSpace = currentSpaces.find { it.id == currentActiveSpaceId }
        val spaceTabs = currentTabs.filter { it.spaceId == currentActiveSpaceId }
        val currentMedia = currentMediaList.filter { it.tabId == currentActiveTabId }
        val currentUrl = activeTab?.url ?: "about:blank"

        _uiState.value = _uiState.value.copy(
            currentUrl = currentUrl,
            currentTitle = activeTab?.title ?: "New Tab",
            inputUrlText = if (!_uiState.value.isEditingUrl) currentUrl else _uiState.value.inputUrlText,
            activeTab = activeTab,
            activeSpace = activeSpace,
            totalTabCount = spaceTabs.size,
            spaceTabs = spaceTabs,
            discoveredMediaCount = currentMedia.size,
            adsBlockedThisSession = currentStats.totalAdsBlocked,
            isBurnerMode = activeSpace?.isBurner ?: false,
            isBookmarked = bookmarkRepo.isBookmarked(currentUrl)
        )
    }

    fun onUrlInputChanged(newText: String) {
        _uiState.value = _uiState.value.copy(inputUrlText = newText)
    }

    fun setEditingUrl(isEditing: Boolean) {
        _uiState.value = _uiState.value.copy(isEditingUrl = isEditing)
    }

    fun submitUrl(input: String, onUrlReady: (String) -> Unit) {
        val targetUrl = SearchEngineDefaults.resolveQueryOrUrl(input, _browserSettings.value.searchEngine)

        val cleanedUrl = if (_browserSettings.value.isUrlCleanerEnabled) {
            UrlCleanerHelper.cleanUrl(targetUrl)
        } else {
            targetUrl
        }

        val finalUrl = if (_browserSettings.value.isHttpsOnly && cleanedUrl.startsWith("http://")) {
            cleanedUrl.replaceFirst("http://", "https://")
        } else {
            cleanedUrl
        }

        _uiState.value = _uiState.value.copy(
            currentUrl = finalUrl,
            inputUrlText = finalUrl,
            isEditingUrl = false,
            isLoading = true,
            pageProgress = 10
        )
        _uiState.value.activeTab?.id?.let { activeId ->
            browserRepo.updateTab(activeId, url = finalUrl, title = "Loading...")
        }
        onUrlReady(finalUrl)
    }

    fun onPageStarted(url: String) {
        _uiState.value = _uiState.value.copy(
            currentUrl = url,
            inputUrlText = url,
            isLoading = true,
            pageProgress = 15,
            isBookmarked = bookmarkRepo.isBookmarked(url)
        )
        _uiState.value.activeTab?.id?.let { activeId ->
            browserRepo.updateTab(activeId, url = url)
            if (_uiState.value.isBurnerMode.not()) {
                historyRepo.recordVisit(url, _uiState.value.currentTitle)
            }
        }
    }

    fun onPageFinished(url: String, title: String? = null) {
        _uiState.value = _uiState.value.copy(
            isLoading = false,
            pageProgress = 100,
            currentUrl = url,
            currentTitle = title ?: _uiState.value.currentTitle,
            isBookmarked = bookmarkRepo.isBookmarked(url)
        )
        _uiState.value.activeTab?.id?.let { activeId ->
            browserRepo.updateTab(activeId, url = url, title = title ?: "Untitled")
        }
    }

    fun saveTabSnapshot(tabId: String?, bitmap: android.graphics.Bitmap, context: Context) {
        if (tabId == null) return
        viewModelScope.launch(Dispatchers.IO) {
            try {
                val file = java.io.File(context.cacheDir, "tab_preview_${tabId}.jpg")
                val fos = java.io.FileOutputStream(file)
                bitmap.compress(android.graphics.Bitmap.CompressFormat.JPEG, 85, fos)
                fos.flush()
                fos.close()
                browserRepo.updateTab(tabId, previewThumbnailPath = file.absolutePath)
            } catch (_: Exception) {}
        }
    }

    fun onProgressChanged(progress: Int) {
        _uiState.value = _uiState.value.copy(
            pageProgress = progress,
            isLoading = progress < 100
        )
    }

    fun onAdBlocked(categoryCode: Int) {
        shieldsRepo.recordAdBlocked(categoryCode)
    }

    fun recordPopupBlocked() {
        shieldsRepo.recordPopupBlocked()
    }

    fun recordRedirectBlocked() {
        shieldsRepo.recordRedirectBlocked()
    }

    fun setPopupBlockingEnabled(enabled: Boolean) {
        shieldsRepo.setPopupBlockingEnabled(enabled)
    }

    fun setRedirectBlockingEnabled(enabled: Boolean) {
        shieldsRepo.setRedirectBlockingEnabled(enabled)
    }

    fun setBlockerMode(mode: BlockerMode) {
        shieldsRepo.setBlockerMode(mode)
    }

    fun toggleFilterList(filterId: String) {
        shieldsRepo.toggleFilterList(filterId)
    }

    fun setEgressMode(mode: EgressMode) {
        shieldsRepo.setEgressMode(mode)
    }

    fun toggleShieldsForHost(hostname: String) {
        shieldsRepo.toggleShieldsForHost(hostname)
    }

    fun toggleScriptBlockingForHost(hostname: String) {
        shieldsRepo.toggleScriptBlockingForHost(hostname)
    }

    fun onMediaDiscovered(media: DiscoveredMedia) {
        transferRepo.addDiscoveredMedia(media)
    }

    fun onArticleExtracted(rawJson: String, url: String) {
        val article = ArticleExtractorHelper.parseExtractedJson(rawJson, url)
        aiRepo.setExtractedArticle(article)
    }

    fun generateAiSummary() {
        viewModelScope.launch {
            aiRepo.generateSummaryForCurrentArticle()
        }
    }

    fun sendAiChatMessage(query: String) {
        viewModelScope.launch {
            aiRepo.sendMessage(query)
        }
    }

    fun setReaderModeActive(active: Boolean) {
        aiRepo.setReaderModeActive(active)
    }

    fun setReaderTheme(theme: ReaderTheme) {
        aiRepo.setReaderTheme(theme)
    }

    fun setReaderFontSize(sizeSp: Int) {
        aiRepo.setReaderFontSize(sizeSp)
    }

    fun initTts(context: Context) {
        if (ttsEngine == null) {
            ttsEngine = TextToSpeechEngine(context).apply {
                viewModelScope.launch {
                    ttsState.collect { state ->
                        _ttsState.value = state
                    }
                }
            }
        }
    }

    fun playArticleTts() {
        val plainText = aiRepo.currentArticle.value?.plainText ?: return
        ttsEngine?.startReading(plainText)
    }

    fun pauseArticleTts() {
        ttsEngine?.pause()
    }

    fun resumeArticleTts() {
        ttsEngine?.resume()
    }

    fun stopArticleTts() {
        ttsEngine?.stop()
    }

    fun setTtsSpeedRate(rate: Float) {
        ttsEngine?.setSpeedRate(rate)
    }

    fun selectTab(tabId: String) {
        browserRepo.selectTab(tabId)
    }

    fun selectNextTab() {
        browserRepo.selectNextTabInSpace()
    }

    fun selectPrevTab() {
        browserRepo.selectPrevTabInSpace()
    }

    fun selectTabByIndex(index: Int) {
        browserRepo.selectTabByIndexInSpace(index)
    }

    fun selectSpace(spaceId: String) {
        browserRepo.selectSpace(spaceId)
    }

    fun createNewTab(url: String = "https://duckduckgo.com") {
        browserRepo.createTab(url = url, spaceId = browserRepo.activeSpaceId.value)
    }

    fun createSpace(name: String, accentColorHex: String = "#FF5A1F", isBurner: Boolean = false, iconName: String = "globe") {
        browserRepo.createSpace(name, accentColorHex, isBurner, iconName)
    }

    fun deleteSpace(spaceId: String) {
        browserRepo.deleteSpace(spaceId)
    }

    fun updateSpace(spaceId: String, name: String, accentColorHex: String, iconName: String) {
        browserRepo.updateSpace(spaceId, name, accentColorHex, iconName)
    }

    fun closeTab(tabId: String) {
        browserRepo.closeTab(tabId)
        transferRepo.clearDiscoveredMediaForTab(tabId)
    }

    fun closeAllTabsInSpace(spaceId: String = browserRepo.activeSpaceId.value) {
        browserRepo.closeAllTabsInSpace(spaceId)
    }

    fun moveTabToSpace(tabId: String, targetSpaceId: String) {
        browserRepo.moveTabToSpace(tabId, targetSpaceId)
    }

    fun duplicateTab(tabId: String) {
        browserRepo.duplicateTab(tabId)
    }

    fun togglePinTab(tabId: String) {
        browserRepo.togglePinTab(tabId)
    }

    fun toggleFavoriteTab(tabId: String) {
        browserRepo.toggleFavoriteTab(tabId)
    }

    fun selectMediaQuality(mediaId: String, qualityId: String) {
        transferRepo.selectMediaQuality(mediaId, qualityId)
    }

    fun startMediaDownload(media: DiscoveredMedia, qualityTrack: MediaQualityTrack? = null) {
        transferRepo.startTransfer(media, qualityTrack)
    }

    fun pauseTransfer(transferId: String) {
        transferRepo.pauseTransfer(transferId)
    }

    fun resumeTransfer(transferId: String) {
        transferRepo.resumeTransfer(transferId)
    }

    fun cancelTransfer(transferId: String) {
        transferRepo.cancelTransfer(transferId)
    }

    fun removeTransfer(transferId: String) {
        transferRepo.removeTransfer(transferId)
    }

    fun toggleBookmark(url: String = _uiState.value.currentUrl, title: String? = _uiState.value.currentTitle): Boolean {
        val isNowBookmarked = bookmarkRepo.toggleBookmark(url, title)
        _uiState.value = _uiState.value.copy(isBookmarked = isNowBookmarked)
        return isNowBookmarked
    }

    fun removeBookmark(id: String) {
        bookmarkRepo.removeBookmark(id)
    }

    fun removeHistoryItem(id: String) {
        historyRepo.removeHistoryItem(id)
    }

    fun clearAllHistory() {
        historyRepo.clearAllHistory()
    }

    fun exportBookmarksHtml(): String {
        return bookmarkRepo.exportBookmarksHtml()
    }

    fun importBookmarksHtml(htmlString: String): Int {
        return bookmarkRepo.importBookmarksHtml(htmlString)
    }

    fun exportEncryptedBackup(passphrase: String): String {
        val backup = com.fireball.mini.core.models.FireballBackupData(
            version = 1,
            exportedTimestampMs = System.currentTimeMillis(),
            spaces = browserRepo.spaces.value,
            bookmarks = bookmarkRepo.bookmarks.value,
            history = historyRepo.history.value
        )
        return com.fireball.mini.core.engine.EncryptedBackupManager.encryptBackup(backup, passphrase)
    }

    fun importEncryptedBackup(bundleJson: String, passphrase: String): Boolean {
        return try {
            val backup = com.fireball.mini.core.engine.EncryptedBackupManager.decryptBackup(bundleJson, passphrase)
            if (backup.bookmarks.isNotEmpty()) {
                bookmarkRepo.restoreBookmarks(backup.bookmarks)
            }
            if (backup.history.isNotEmpty()) {
                historyRepo.restoreHistory(backup.history)
            }
            true
        } catch (_: Exception) {
            false
        }
    }

    fun generateNewBraveSyncChain(): List<String> {
        return syncRepo.generateNewBraveSyncChain()
    }

    fun joinBraveSyncChain(words: String): Boolean {
        return syncRepo.joinBraveSyncChain(words)
    }

    fun connectFirefoxSync(accountEmail: String, syncKey: String, serverUrl: String = "https://sync.services.mozilla.com/1.5/"): Boolean {
        return syncRepo.connectFirefoxSync(accountEmail, syncKey, serverUrl)
    }

    fun performSyncNow() {
        val result = syncRepo.performSyncNow(
            currentBookmarks = bookmarkRepo.bookmarks.value,
            currentHistory = historyRepo.history.value,
            currentTabs = browserRepo.tabs.value
        )
        if (result.newBookmarks.isNotEmpty()) {
            bookmarkRepo.restoreBookmarks(result.newBookmarks)
        }
    }

    fun toggleSyncCategory(syncBookmarks: Boolean? = null, syncHistory: Boolean? = null, syncTabs: Boolean? = null) {
        syncRepo.toggleSyncCategory(syncBookmarks, syncHistory, syncTabs)
    }

    fun setAutoSyncEnabled(enabled: Boolean) {
        syncRepo.setAutoSyncEnabled(enabled)
    }

    fun disconnectSync() {
        syncRepo.disconnect()
    }

    fun setSearchEngine(engine: SearchEngine) {
        _browserSettings.update { it.copy(searchEngine = engine) }
        searchEngineRepo.setDefaultEngine(engine.id)
    }

    fun addCustomSearchEngine(name: String, searchUrl: String, suggestUrl: String? = null, bang: String? = null): SearchEngine {
        return searchEngineRepo.addCustomEngine(name, searchUrl, suggestUrl, bang)
    }

    fun deleteCustomSearchEngine(id: String) {
        searchEngineRepo.deleteCustomEngine(id)
    }

    // Passwords Vault
    fun saveCredential(domain: String, username: String, plainPassword: String): SavedCredential {
        return passwordVaultRepo.saveCredential(domain, username, plainPassword)
    }

    fun decryptCredential(cred: SavedCredential): DecryptedCredential? {
        return passwordVaultRepo.decryptCredential(cred)
    }

    fun deleteCredential(id: String) {
        passwordVaultRepo.deleteCredential(id)
    }

    // Site Permissions & Data
    fun getSiteInfo(urlOrDomain: String): SiteStorageInfo {
        return siteSettingsRepo.getSiteInfo(urlOrDomain)
    }

    fun setSitePermission(domain: String, type: SitePermissionType, status: PermissionStatus) {
        siteSettingsRepo.setPermission(domain, type, status)
    }

    fun clearSiteData(domain: String, onCleared: () -> Unit = {}) {
        siteSettingsRepo.clearSiteData(domain, onCleared)
    }

    fun setHttpsOnly(enabled: Boolean) {
        _browserSettings.update { it.copy(isHttpsOnly = enabled) }
    }

    fun setUrlCleanerEnabled(enabled: Boolean) {
        _browserSettings.update { it.copy(isUrlCleanerEnabled = enabled) }
    }

    fun setMediaSnifferEnabled(enabled: Boolean) {
        _browserSettings.update { it.copy(isMediaSnifferEnabled = enabled) }
    }

    fun setDoNotTrackEnabled(enabled: Boolean) {
        _browserSettings.update { it.copy(isDoNotTrackEnabled = enabled) }
    }

    fun setAutoArchiveEnabled(enabled: Boolean) {
        _browserSettings.update { it.copy(isAutoArchiveEnabled = enabled) }
    }

    fun setAutoArchiveDuration(duration: AutoArchiveDuration) {
        _browserSettings.update { it.copy(autoArchiveDuration = duration) }
    }

    fun setPreferredVideoQuality(quality: PreferredVideoQuality) {
        _browserSettings.update { it.copy(preferredVideoQuality = quality) }
    }

    fun setDownloadThreads(threads: Int) {
        _browserSettings.update { it.copy(downloadThreads = threads) }
    }

    fun setDesktopModeDefault(enabled: Boolean) {
        _browserSettings.update { it.copy(isDesktopModeDefault = enabled) }
    }

    fun setAggressiveTabDiscarding(enabled: Boolean) {
        _browserSettings.update { it.copy(isAggressiveTabDiscarding = enabled) }
    }

    fun clearBrowsingData(clearHistory: Boolean = true, clearBookmarks: Boolean = false, clearTransfers: Boolean = false) {
        if (clearHistory) {
            historyRepo.clearAllHistory()
        }
        if (clearBookmarks) {
            bookmarkRepo.restoreBookmarks(emptyList())
        }
        if (clearTransfers) {
            val list = transferRepo.transfers.value
            for (t in list) {
                transferRepo.removeTransfer(t.id)
            }
        }
    }

    fun toggleBrowserEngine() {
        val nextEngine = if (_uiState.value.engineType == com.fireball.mini.core.models.BrowserEngineType.NATIVE_WEBVIEW) {
            com.fireball.mini.core.models.BrowserEngineType.FIREBALL_BEAM_STREAM
        } else {
            com.fireball.mini.core.models.BrowserEngineType.NATIVE_WEBVIEW
        }
        setBrowserEngine(nextEngine)
    }

    fun setBrowserEngine(type: com.fireball.mini.core.models.BrowserEngineType) {
        _uiState.value = _uiState.value.copy(engineType = type)
        if (type == com.fireball.mini.core.models.BrowserEngineType.FIREBALL_BEAM_STREAM) {
            beamClient.startStream()
        } else {
            beamClient.stopStream()
        }
    }

    override fun onCleared() {
        super.onCleared()
        ttsEngine?.release()
        beamClient.destroy()
    }
}

