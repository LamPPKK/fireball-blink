package com.fireball.mini.data

import com.fireball.mini.core.models.BlockerMode
import com.fireball.mini.core.models.EgressMode
import com.fireball.mini.core.models.EgressStatus
import com.fireball.mini.core.models.FilterCategory
import com.fireball.mini.core.models.FilterListSubscription
import com.fireball.mini.core.models.ShieldsStats
import com.fireball.mini.core.models.SiteShieldsPolicy
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update

class ShieldsRepository {

    private val defaultFilterLists = listOf(
        FilterListSubscription(
            id = "easylist_core",
            name = "EasyList Core (Ads & Video Ads)",
            description = "Blocks banners, popups, video pre-rolls, and native ad networks.",
            category = FilterCategory.ADS,
            rulesCount = 58420,
            isEnabled = true,
            iconName = "shield"
        ),
        FilterListSubscription(
            id = "easyprivacy_trackers",
            name = "EasyPrivacy (Trackers & Telemetry)",
            description = "Stops behavioral tracking, analytics pixels, and fingerprinting scripts.",
            category = FilterCategory.TRACKERS,
            rulesCount = 36980,
            isEnabled = true,
            iconName = "fingerprint"
        ),
        FilterListSubscription(
            id = "fanboy_annoyances",
            name = "Fanboy's Annoyances (Cookie Banners & GDPR)",
            description = "Eliminates annoying cookie consent dialogs, newsletter modals, and push spam.",
            category = FilterCategory.ANNOYANCES,
            rulesCount = 24150,
            isEnabled = true,
            iconName = "cookie"
        ),
        FilterListSubscription(
            id = "anti_malware_crypto",
            name = "Malware & Cryptomining Shield",
            description = "Shields against phishing endpoints, cryptojacking, and scam redirects.",
            category = FilterCategory.SECURITY,
            rulesCount = 14320,
            isEnabled = true,
            iconName = "security"
        ),
        FilterListSubscription(
            id = "social_widgets_blocker",
            name = "Social Media Pixels & Widgets",
            description = "Disables Meta, TikTok, X, and LinkedIn tracking beacons.",
            category = FilterCategory.SOCIAL,
            rulesCount = 11840,
            isEnabled = true,
            iconName = "people"
        ),
        FilterListSubscription(
            id = "cosmetic_element_hiding",
            name = "Cosmetic Element & Whitespace Collapser",
            description = "Deep CSS rules to collapse blank ad spaces and floating sponsor bars.",
            category = FilterCategory.COSMETIC,
            rulesCount = 42100,
            isEnabled = true,
            iconName = "cleaning"
        )
    )

    private val _filterLists = MutableStateFlow(defaultFilterLists)
    val filterLists: StateFlow<List<FilterListSubscription>> = _filterLists.asStateFlow()

    private val _blockerMode = MutableStateFlow(BlockerMode.STANDARD)
    val blockerMode: StateFlow<BlockerMode> = _blockerMode.asStateFlow()

    private val _stats = MutableStateFlow(
        ShieldsStats(
            totalAdsBlocked = 128,
            totalTrackersBlocked = 64,
            totalAnnoyancesBlocked = 32,
            totalMalwareBlocked = 12,
            totalBandwidthSavedBytes = 38500000,
            totalTimeSavedMs = 3800
        )
    )
    val stats: StateFlow<ShieldsStats> = _stats.asStateFlow()

    private val _egressStatus = MutableStateFlow(EgressStatus(mode = EgressMode.DIRECT, isConnected = true, egressIp = "Direct Network"))
    val egressStatus: StateFlow<EgressStatus> = _egressStatus.asStateFlow()

    private val _sitePolicies = MutableStateFlow<Map<String, SiteShieldsPolicy>>(emptyMap())
    val sitePolicies: StateFlow<Map<String, SiteShieldsPolicy>> = _sitePolicies.asStateFlow()

    private val _isPopupBlockingEnabled = MutableStateFlow(true)
    val isPopupBlockingEnabled: StateFlow<Boolean> = _isPopupBlockingEnabled.asStateFlow()

    private val _isRedirectBlockingEnabled = MutableStateFlow(true)
    val isRedirectBlockingEnabled: StateFlow<Boolean> = _isRedirectBlockingEnabled.asStateFlow()

    fun recordAdBlocked(categoryCode: Int = 1) {
        _stats.update {
            when (categoryCode) {
                1 -> it.copy(
                    totalAdsBlocked = it.totalAdsBlocked + 1,
                    totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 85000,
                    totalTimeSavedMs = it.totalTimeSavedMs + 45
                )
                2 -> it.copy(
                    totalTrackersBlocked = it.totalTrackersBlocked + 1,
                    totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 25000,
                    totalTimeSavedMs = it.totalTimeSavedMs + 20
                )
                3 -> it.copy(
                    totalAnnoyancesBlocked = it.totalAnnoyancesBlocked + 1,
                    totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 35000,
                    totalTimeSavedMs = it.totalTimeSavedMs + 30
                )
                4 -> it.copy(
                    totalMalwareBlocked = it.totalMalwareBlocked + 1,
                    totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 120000,
                    totalTimeSavedMs = it.totalTimeSavedMs + 50
                )
                else -> it.copy(
                    totalAdsBlocked = it.totalAdsBlocked + 1,
                    totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 50000,
                    totalTimeSavedMs = it.totalTimeSavedMs + 30
                )
            }
        }
    }

    fun recordPopupBlocked() {
        _stats.update {
            it.copy(
                totalPopupsBlocked = it.totalPopupsBlocked + 1,
                totalAnnoyancesBlocked = it.totalAnnoyancesBlocked + 1,
                totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 150000,
                totalTimeSavedMs = it.totalTimeSavedMs + 60
            )
        }
    }

    fun recordRedirectBlocked() {
        _stats.update {
            it.copy(
                totalRedirectsBlocked = it.totalRedirectsBlocked + 1,
                totalAdsBlocked = it.totalAdsBlocked + 1,
                totalBandwidthSavedBytes = it.totalBandwidthSavedBytes + 200000,
                totalTimeSavedMs = it.totalTimeSavedMs + 80
            )
        }
    }

    fun setPopupBlockingEnabled(enabled: Boolean) {
        _isPopupBlockingEnabled.value = enabled
    }

    fun setRedirectBlockingEnabled(enabled: Boolean) {
        _isRedirectBlockingEnabled.value = enabled
    }

    fun toggleFilterList(filterId: String) {
        _filterLists.update { list ->
            list.map {
                if (it.id == filterId) it.copy(isEnabled = !it.isEnabled) else it
            }
        }
    }

    fun setBlockerMode(mode: BlockerMode) {
        _blockerMode.value = mode
    }

    fun setEgressMode(mode: EgressMode) {
        val (ip, latency) = when (mode) {
            EgressMode.DIRECT -> "Direct Network" to 15
            EgressMode.WARP -> "Cloudflare WARP (104.28.19.42)" to 24
            EgressMode.TOR -> "Tor Onion Circuit (185.220.101.5)" to 210
        }
        _egressStatus.value = EgressStatus(
            mode = mode,
            isConnected = true,
            egressIp = ip,
            latencyMs = latency,
            hasLeakProtection = true
        )
    }

    fun getPolicyForHost(hostname: String): SiteShieldsPolicy {
        return _sitePolicies.value[hostname] ?: SiteShieldsPolicy(hostname = hostname)
    }

    fun toggleShieldsForHost(hostname: String) {
        _sitePolicies.update { map ->
            val current = map[hostname] ?: SiteShieldsPolicy(hostname = hostname)
            map + (hostname to current.copy(isEnabled = !current.isEnabled))
        }
    }

    fun toggleScriptBlockingForHost(hostname: String) {
        _sitePolicies.update { map ->
            val current = map[hostname] ?: SiteShieldsPolicy(hostname = hostname)
            map + (hostname to current.copy(blockScripts = !current.blockScripts))
        }
    }
}
