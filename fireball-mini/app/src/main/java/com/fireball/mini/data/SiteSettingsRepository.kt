package com.fireball.mini.data

import android.content.Context
import android.content.SharedPreferences
import android.webkit.CookieManager
import android.webkit.WebStorage
import com.fireball.mini.FireballApp
import com.fireball.mini.core.models.PermissionStatus
import com.fireball.mini.core.models.SitePermission
import com.fireball.mini.core.models.SitePermissionType
import com.fireball.mini.core.models.SiteStorageInfo
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONObject
import java.net.URI

class SiteSettingsRepository(context: Context? = null) {
    private val resolvedContext: Context? = context ?: FireballApp.instance
    private val prefs: SharedPreferences? = try {
        resolvedContext?.getSharedPreferences("fireball_site_settings", Context.MODE_PRIVATE)
    } catch (_: Throwable) {
        null
    }

    private val _sitePermissions = MutableStateFlow<Map<String, Map<SitePermissionType, PermissionStatus>>>(emptyMap())
    val sitePermissions: StateFlow<Map<String, Map<SitePermissionType, PermissionStatus>>> = _sitePermissions.asStateFlow()

    private val _visitedSites = MutableStateFlow<List<SiteStorageInfo>>(emptyList())
    val visitedSites: StateFlow<List<SiteStorageInfo>> = _visitedSites.asStateFlow()

    init {
        loadPermissions()
    }

    private fun loadPermissions() {
        val json = prefs?.getString("permissions_data", "{}") ?: "{}"
        val resultMap = mutableMapOf<String, MutableMap<SitePermissionType, PermissionStatus>>()
        try {
            val root = JSONObject(json)
            val keys = root.keys()
            while (keys.hasNext()) {
                val domain = keys.next()
                val domainObj = root.getJSONObject(domain)
                val permMap = mutableMapOf<SitePermissionType, PermissionStatus>()
                for (type in SitePermissionType.values()) {
                    if (domainObj.has(type.name)) {
                        val statusStr = domainObj.getString(type.name)
                        try {
                            permMap[type] = PermissionStatus.valueOf(statusStr)
                        } catch (_: Exception) {}
                    }
                }
                resultMap[domain] = permMap
            }
        } catch (_: Exception) {}
        _sitePermissions.value = resultMap
    }

    fun getPermission(domain: String, type: SitePermissionType): PermissionStatus {
        val cleanDomain = extractCleanDomain(domain)
        return _sitePermissions.value[cleanDomain]?.get(type) ?: when (type) {
            SitePermissionType.LOCATION,
            SitePermissionType.CAMERA,
            SitePermissionType.MICROPHONE,
            SitePermissionType.NOTIFICATIONS -> PermissionStatus.ASK
            SitePermissionType.JAVASCRIPT,
            SitePermissionType.AUTO_PLAY -> PermissionStatus.ALLOW
            SitePermissionType.POPUPS,
            SitePermissionType.COOKIES -> PermissionStatus.BLOCK
        }
    }

    fun setPermission(domain: String, type: SitePermissionType, status: PermissionStatus) {
        val cleanDomain = extractCleanDomain(domain)
        val current = _sitePermissions.value.toMutableMap()
        val domainMap = current[cleanDomain]?.toMutableMap() ?: mutableMapOf()
        domainMap[type] = status
        current[cleanDomain] = domainMap
        _sitePermissions.value = current
        savePermissions()
    }

    fun resetSitePermissions(domain: String) {
        val cleanDomain = extractCleanDomain(domain)
        val current = _sitePermissions.value.toMutableMap()
        current.remove(cleanDomain)
        _sitePermissions.value = current
        savePermissions()
    }

    fun getSiteInfo(urlOrDomain: String): SiteStorageInfo {
        val cleanDomain = extractCleanDomain(urlOrDomain)
        val isHttps = urlOrDomain.startsWith("https://", ignoreCase = true) || !urlOrDomain.startsWith("http://", ignoreCase = true)
        val perms = _sitePermissions.value[cleanDomain] ?: emptyMap()

        var cookieCount = 0
        try {
            val cookieMgr = CookieManager.getInstance()
            val cookies = cookieMgr.getCookie(if (urlOrDomain.startsWith("http")) urlOrDomain else "https://$cleanDomain")
            if (!cookies.isNullOrBlank()) {
                cookieCount = cookies.split(";").size
            }
        } catch (_: Throwable) {}

        return SiteStorageInfo(
            domain = cleanDomain,
            cookieCount = cookieCount,
            estimatedStorageBytes = if (cookieCount > 0) (cookieCount * 128L) + 4096L else 0L,
            isSecureHttps = isHttps,
            permissions = perms
        )
    }

    fun clearSiteData(domain: String, onCleared: () -> Unit = {}) {
        val cleanDomain = extractCleanDomain(domain)
        try {
            val cookieMgr = CookieManager.getInstance()
            val httpUrl = "http://$cleanDomain"
            val httpsUrl = "https://$cleanDomain"
            cookieMgr.setCookie(httpUrl, "")
            cookieMgr.setCookie(httpsUrl, "")
            cookieMgr.flush()

            WebStorage.getInstance().deleteOrigin(httpsUrl)
            WebStorage.getInstance().deleteOrigin(httpUrl)
        } catch (_: Throwable) {}

        resetSitePermissions(cleanDomain)
        onCleared()
    }

    fun clearAllCookiesAndStorage(onCleared: () -> Unit = {}) {
        try {
            CookieManager.getInstance().removeAllCookies {
                CookieManager.getInstance().flush()
            }
            WebStorage.getInstance().deleteAllData()
        } catch (_: Throwable) {}
        onCleared()
    }

    private fun savePermissions() {
        val root = JSONObject()
        for ((domain, perms) in _sitePermissions.value) {
            val domainObj = JSONObject()
            for ((type, status) in perms) {
                domainObj.put(type.name, status.name)
            }
            root.put(domain, domainObj)
        }
        prefs?.edit()?.putString("permissions_data", root.toString())?.apply()
    }

    companion object {
        fun extractCleanDomain(urlOrDomain: String): String {
            val trimmed = urlOrDomain.trim()
            if (trimmed.isEmpty()) return "about:blank"
            return try {
                if (trimmed.startsWith("http://") || trimmed.startsWith("https://")) {
                    URI(trimmed).host ?: trimmed
                } else if (trimmed.contains("/")) {
                    trimmed.substringBefore("/")
                } else {
                    trimmed
                }
            } catch (_: Exception) {
                trimmed.substringBefore("/").substringBefore(":")
            }
        }
    }
}
