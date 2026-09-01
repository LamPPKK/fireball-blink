package com.fireball.mini.core.engine

import android.annotation.SuppressLint
import android.content.Context
import android.util.Log
import android.view.GestureDetector
import android.view.MotionEvent
import android.webkit.CookieManager
import android.webkit.WebSettings
import android.webkit.WebView
import androidx.webkit.ProfileStore
import androidx.webkit.WebViewCompat
import androidx.webkit.WebViewFeature
import kotlin.math.abs

@SuppressLint("SetJavaScriptEnabled")
class FireballWebView(
    context: Context,
    val spaceId: String = "space-main",
    val profileId: String = "profile-main",
    val isOffTheRecord: Boolean = false
) : WebView(context) {

    companion object {
        private const val TAG = "FireballWebView"
        private const val SWIPE_THRESHOLD = 120f
        private const val SWIPE_VELOCITY_THRESHOLD = 150f
    }

    private val gestureDetector = GestureDetector(context, object : GestureDetector.SimpleOnGestureListener() {
        override fun onFling(
            e1: MotionEvent?,
            e2: MotionEvent,
            velocityX: Float,
            velocityY: Float
        ): Boolean {
            if (e1 == null) return false
            val diffX = e2.x - e1.x
            val diffY = e2.y - e1.y

            // Horizontal edge swipe detection
            if (abs(diffX) > abs(diffY) && abs(diffX) > SWIPE_THRESHOLD && abs(velocityX) > SWIPE_VELOCITY_THRESHOLD) {
                if (diffX > 0 && e1.x < 120f) {
                    // Swiped from left edge -> Go Back
                    if (canGoBack()) {
                        goBack()
                        return true
                    }
                } else if (diffX < 0 && e1.x > (width - 120f)) {
                    // Swiped from right edge -> Go Forward
                    if (canGoForward()) {
                        goForward()
                        return true
                    }
                }
            }
            return false
        }
    })

    init {
        isFocusable = true
        isFocusableInTouchMode = true
        isNestedScrollingEnabled = true
        setupProfileAndSettings()
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        gestureDetector.onTouchEvent(event)
        return super.onTouchEvent(event)
    }

    private fun setupProfileAndSettings() {
        // Multi-profile session and cookie isolation per Space
        val profileName = if (isOffTheRecord) "profile_incognito" else "profile_${spaceId.replace("-", "_")}"

        if (WebViewFeature.isFeatureSupported(WebViewFeature.MULTI_PROFILE)) {
            try {
                val profileStore = ProfileStore.getInstance()
                val profile = profileStore.getOrCreateProfile(profileName)
                WebViewCompat.setProfile(this, profileName)

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
            domStorageEnabled = true // Required for all modern websites (Google, YouTube, Twitter, SPAs)
            databaseEnabled = true
            useWideViewPort = true
            loadWithOverviewMode = true
            setSupportZoom(true)
            builtInZoomControls = true
            displayZoomControls = false
            allowFileAccess = false
            allowContentAccess = false
            mixedContentMode = WebSettings.MIXED_CONTENT_COMPATIBILITY_MODE
            cacheMode = if (isOffTheRecord) WebSettings.LOAD_NO_CACHE else WebSettings.LOAD_DEFAULT
            userAgentString = "Mozilla/5.0 (Linux; Android 14; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Mobile Safari/537.36"
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
