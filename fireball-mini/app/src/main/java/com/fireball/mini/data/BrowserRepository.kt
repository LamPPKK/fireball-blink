package com.fireball.mini.data

import com.fireball.mini.core.models.DiscardState
import com.fireball.mini.core.models.Profile
import com.fireball.mini.core.models.Space
import com.fireball.mini.core.models.TabItem
import com.fireball.mini.core.models.TabSection
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import java.util.UUID

class BrowserRepository {

    private val defaultProfile = Profile(
        id = "default-profile",
        name = "Main Profile",
        isOffTheRecord = false
    )

    private val burnerProfile = Profile(
        id = "burner-profile",
        name = "Incognito Profile",
        isOffTheRecord = true
    )

    private val defaultSpaces = listOf(
        Space(
            id = "space-main",
            name = "Main",
            profileId = defaultProfile.id,
            accentColorHex = "#B8FF3D",
            iconName = "globe"
        ),
        Space(
            id = "space-incognito",
            name = "Incognito",
            profileId = burnerProfile.id,
            isBurner = true,
            accentColorHex = "#FF5A1F",
            iconName = "fire"
        )
    )

    private val _profiles = MutableStateFlow(listOf(defaultProfile, burnerProfile))
    val profiles: StateFlow<List<Profile>> = _profiles.asStateFlow()

    private val _spaces = MutableStateFlow(defaultSpaces)
    val spaces: StateFlow<List<Space>> = _spaces.asStateFlow()

    private val _activeSpaceId = MutableStateFlow(defaultSpaces.first().id)
    val activeSpaceId: StateFlow<String> = _activeSpaceId.asStateFlow()

    private val initialTabs = listOf(
        TabItem(
            id = "tab-home",
            spaceId = "space-main",
            profileId = defaultProfile.id,
            url = "https://duckduckgo.com",
            title = "DuckDuckGo — Privacy, simplified.",
            section = TabSection.TODAY
        )
    )

    private val _tabs = MutableStateFlow<List<TabItem>>(initialTabs)
    val tabs: StateFlow<List<TabItem>> = _tabs.asStateFlow()

    private val _activeTabId = MutableStateFlow<String?>(initialTabs.first().id)
    val activeTabId: StateFlow<String?> = _activeTabId.asStateFlow()

    fun selectSpace(spaceId: String) {
        _activeSpaceId.value = spaceId
        val spaceTabs = _tabs.value.filter { it.spaceId == spaceId }
        if (spaceTabs.isNotEmpty() && (activeTabId.value == null || spaceTabs.none { it.id == activeTabId.value })) {
            _activeTabId.value = spaceTabs.first().id
        }
    }

    fun createSpace(name: String, accentColorHex: String = "#B8FF3D", isBurner: Boolean = false, iconName: String = "globe"): Space {
        val profileId = if (isBurner) burnerProfile.id else defaultProfile.id
        val newSpace = Space(
            id = "space-${UUID.randomUUID()}",
            name = name,
            profileId = profileId,
            isBurner = isBurner,
            accentColorHex = accentColorHex,
            iconName = iconName
        )
        _spaces.update { it + newSpace }
        _activeSpaceId.value = newSpace.id
        return newSpace
    }

    fun updateSpace(spaceId: String, name: String? = null, iconName: String? = null, accentColorHex: String? = null) {
        _spaces.update { list ->
            list.map {
                if (it.id == spaceId) {
                    it.copy(
                        name = name ?: it.name,
                        iconName = iconName ?: it.iconName,
                        accentColorHex = accentColorHex ?: it.accentColorHex
                    )
                } else it
            }
        }
    }

    fun deleteSpace(spaceId: String) {
        if (spaceId == "space-main" || spaceId == "space-incognito") return
        _spaces.update { list -> list.filter { it.id != spaceId } }
        _tabs.update { list -> list.filter { it.spaceId != spaceId } }
        if (_activeSpaceId.value == spaceId) {
            _activeSpaceId.value = "space-main"
            val mainTabs = _tabs.value.filter { it.spaceId == "space-main" }
            _activeTabId.value = mainTabs.firstOrNull()?.id
        }
    }

    fun selectTab(tabId: String) {
        val tab = _tabs.value.find { it.id == tabId } ?: return
        _activeTabId.value = tabId
        _activeSpaceId.value = tab.spaceId
        _tabs.update { list ->
            list.map {
                if (it.id == tabId) it.copy(lastAccessedTimestampMs = System.currentTimeMillis(), discardState = DiscardState.LOADED) else it
            }
        }
    }

    fun selectNextTabInSpace() {
        val currentSpaceTabs = _tabs.value.filter { it.spaceId == _activeSpaceId.value }
        if (currentSpaceTabs.size <= 1) return
        val currentIndex = currentSpaceTabs.indexOfFirst { it.id == _activeTabId.value }
        val nextIndex = if (currentIndex >= 0) (currentIndex + 1) % currentSpaceTabs.size else 0
        selectTab(currentSpaceTabs[nextIndex].id)
    }

    fun selectPrevTabInSpace() {
        val currentSpaceTabs = _tabs.value.filter { it.spaceId == _activeSpaceId.value }
        if (currentSpaceTabs.size <= 1) return
        val currentIndex = currentSpaceTabs.indexOfFirst { it.id == _activeTabId.value }
        val prevIndex = if (currentIndex > 0) currentIndex - 1 else currentSpaceTabs.size - 1
        selectTab(currentSpaceTabs[prevIndex].id)
    }

    fun selectTabByIndexInSpace(index: Int) {
        val currentSpaceTabs = _tabs.value.filter { it.spaceId == _activeSpaceId.value }
        if (index in currentSpaceTabs.indices) {
            selectTab(currentSpaceTabs[index].id)
        }
    }

    fun createTab(url: String = "https://duckduckgo.com", spaceId: String = _activeSpaceId.value, section: TabSection = TabSection.TODAY): TabItem {
        val space = _spaces.value.find { it.id == spaceId } ?: _spaces.value.first()
        val newTab = TabItem(
            id = UUID.randomUUID().toString(),
            spaceId = space.id,
            profileId = space.profileId,
            url = url,
            title = if (url == "https://duckduckgo.com") "DuckDuckGo" else url.removePrefix("https://").removePrefix("http://").substringBefore('/'),
            section = if (space.isBurner) TabSection.TODAY else section
        )
        _tabs.update { it + newTab }
        _activeTabId.value = newTab.id
        return newTab
    }

    fun updateTab(
        tabId: String,
        url: String? = null,
        title: String? = null,
        faviconUrl: String? = null,
        previewThumbnailPath: String? = null
    ) {
        _tabs.update { list ->
            list.map {
                if (it.id == tabId) {
                    it.copy(
                        url = url ?: it.url,
                        title = title ?: it.title,
                        faviconUrl = faviconUrl ?: it.faviconUrl,
                        previewThumbnailPath = previewThumbnailPath ?: it.previewThumbnailPath,
                        lastAccessedTimestampMs = System.currentTimeMillis()
                    )
                } else it
            }
        }
    }

    fun closeTab(tabId: String) {
        val currentTabs = _tabs.value
        val tabToClose = currentTabs.find { it.id == tabId } ?: return
        val remaining = currentTabs.filter { it.id != tabId }
        _tabs.value = remaining

        if (_activeTabId.value == tabId) {
            val nextInSameSpace = remaining.filter { it.spaceId == tabToClose.spaceId }
            _activeTabId.value = nextInSameSpace.firstOrNull()?.id ?: remaining.firstOrNull()?.id
        }
    }

    fun closeAllTabsInSpace(spaceId: String) {
        val remaining = _tabs.value.filter { it.spaceId != spaceId }
        val newTab = TabItem(
            id = UUID.randomUUID().toString(),
            spaceId = spaceId,
            profileId = if (spaceId == "space-incognito") burnerProfile.id else defaultProfile.id,
            url = "https://duckduckgo.com",
            title = "DuckDuckGo",
            section = TabSection.TODAY
        )
        val updated = remaining + newTab
        _tabs.value = updated
        if (_activeSpaceId.value == spaceId) {
            _activeTabId.value = newTab.id
        } else if (remaining.isNotEmpty()) {
            _activeTabId.value = remaining.first().id
        }
    }

    fun moveTabToSpace(tabId: String, targetSpaceId: String) {
        val space = _spaces.value.find { it.id == targetSpaceId } ?: return
        _tabs.update { list ->
            list.map {
                if (it.id == tabId) {
                    it.copy(
                        spaceId = targetSpaceId,
                        profileId = space.profileId,
                        section = if (space.isBurner) TabSection.TODAY else it.section
                    )
                } else it
            }
        }
    }

    fun duplicateTab(tabId: String): TabItem? {
        val tab = _tabs.value.find { it.id == tabId } ?: return null
        val duplicated = tab.copy(
            id = UUID.randomUUID().toString(),
            lastAccessedTimestampMs = System.currentTimeMillis()
        )
        _tabs.update { it + duplicated }
        _activeTabId.value = duplicated.id
        return duplicated
    }

    fun togglePinTab(tabId: String) {
        _tabs.update { list ->
            list.map {
                if (it.id == tabId) {
                    val nextSection = if (it.section == TabSection.PINNED) TabSection.TODAY else TabSection.PINNED
                    it.copy(section = nextSection)
                } else it
            }
        }
    }

    fun toggleFavoriteTab(tabId: String) {
        _tabs.update { list ->
            list.map {
                if (it.id == tabId) {
                    val nextSection = if (it.section == TabSection.FAVORITE) TabSection.TODAY else TabSection.FAVORITE
                    it.copy(section = nextSection)
                } else it
            }
        }
    }

    fun selectDiscardCandidates(limit: Int): List<TabItem> {
        val activeId = _activeTabId.value
        return _tabs.value
            .filter { it.id != activeId && it.isSafeToDiscard }
            .sortedWith(
                compareBy<TabItem> {
                    when (it.section) {
                        TabSection.TODAY -> 0
                        TabSection.PINNED -> 1
                        TabSection.FAVORITE -> 2
                    }
                }.thenBy { it.lastAccessedTimestampMs }
            )
            .take(limit)
    }

    fun markTabDiscarded(tabId: String) {
        _tabs.update { list ->
            list.map { if (it.id == tabId) it.copy(discardState = DiscardState.DISCARDED) else it }
        }
    }
}
