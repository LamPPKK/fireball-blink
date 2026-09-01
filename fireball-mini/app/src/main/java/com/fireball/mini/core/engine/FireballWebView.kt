package com.fireball.mini.core.engine

import android.annotation.SuppressLint
import android.content.Context
import android.util.Log
import android.webkit.CookieManager
import android.webkit.WebSettings
import android.webkit.WebView
import androidx.webkit.ProfileStore
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature

@SuppressLint("SetJavaScriptEnabled")
class FireballWebView(
    context: Context,
    val spaceId: String = "space-main",
    val profileId: String = "profile-main",
    val isOffTheRecord: Boolean = false
) : WebView(context) {

    companion object {
        private const val TAG = "FireballWebView"
    }

    init {
        setupProfileAndSettings()
    }

    private fun setupProfileAndSettings() {
        // Multi-profile session and cookie isolation per Space
        val profileName = if (isOffTheRecord) "profile_incognito" else "profile_${spaceId.replace("-", "_")}"

        if (WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE)) {
            try {
                val profileStore = ProfileStore.getInstance()
                val profile = profileStore.getOrCreateProfile(profileName)
                WebViewCompat.setProfile(this, profileName)
                
                // Configure profile-specific cookie manager if needed
                val profileCookieManager = profile.cookieManager
                if (isOffTheRecord) {
                    profileCookieManager.setAcceptCookie(false)
                } else {
                    profileCookieManager.setAcceptCookie(true)
                }
                Log.d(TAG, "Multi-Profile isolation active for space: $spaceId, profileName: $profileName")
            } catch (e: Exception) {
                Log.w(TAG, "Failed to configure Multi-Profile isolation: ${e.message}")
            }
        } else {
            // Fallback for devices where MULTI_PROFILE is not supported
            val globalCookieManager = CookieManager.getInstance()
            if (isOffTheRecord) {
                globalCookieManager.setAcceptCookie(false)
                clearCache(true)
                clearFormData()
                clearHistory()
            } else {
                globalCookieManager.setAcceptCookie(true)
                globalCookieManager.setAcceptThirdPartyCookies(this, false)
            }
        }

        settings.apply {
            javaScriptEnabled = true
            domStorageEnabled = !isOffTheRecord // Disable persistent DOM storage in Incognito
            databaseEnabled = !isOffTheRecord
            useWideViewPort = true
            loadWithOverviewMode = true
            setSupportZoom(true)
            builtInZoomControls = true
            displayZoomControls = false
            allowFileAccess = false
            allowContentAccess = false
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
            cacheMode = if (isOffTheRecord) WebSettings.LOAD_NO_CACHE else WebSettings.LOAD_DEFAULT
            userAgentString = userAgentString.replace("; wv", "") // Modern browser UA
        }
    }

    fun setDesktopMode(enabled: Boolean) {
        if (enabled) {
            settings.userAgentString = "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"
            settings.useWideViewPort = true
            settings.loadWithOverviewMode = true
        } else {
            settings.userAgentString = "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36"
        }
    }

    fun cleanDestroy() {
        stopLoading()
        clearHistory()
        if (isOffTheRecord) {
            clearCache(true)
            clearFormData()
        }
        removeAllViews()
        destroy()
    }
}
