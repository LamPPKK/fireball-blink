package com.fireball.mini.core.engine

import android.graphics.Bitmap
import android.view.View
import android.webkit.GeolocationPermissions
import android.webkit.WebChromeClient
import android.webkit.WebView

class FireballWebChromeClient(
    private val onProgressChangedCallback: (progress: Int) -> Unit,
    private val onTitleReceivedCallback: (title: String) -> Unit,
    private val onIconReceivedCallback: (icon: Bitmap) -> Unit,
    private val onCustomViewShowCallback: (view: View, callback: CustomViewCallback) -> Unit,
    private val onCustomViewHideCallback: () -> Unit,
    private val isPopupBlockingEnabled: () -> Boolean = { true },
    private val onPopupBlockedCallback: () -> Unit = {},
    private val onNewTabRequestedCallback: (url: String) -> Unit = {}
) : WebChromeClient() {

    override fun onProgressChanged(view: WebView?, newProgress: Int) {
        super.onProgressChanged(view, newProgress)
        onProgressChangedCallback(newProgress)
    }

    override fun onReceivedTitle(view: WebView?, title: String?) {
        super.onReceivedTitle(view, title)
        title?.let { onTitleReceivedCallback(it) }
    }

    override fun onReceivedIcon(view: WebView?, icon: Bitmap?) {
        super.onReceivedIcon(view, icon)
        icon?.let { onIconReceivedCallback(it) }
    }

    override fun onShowCustomView(view: View?, callback: CustomViewCallback?) {
        if (view != null && callback != null) {
            onCustomViewShowCallback(view, callback)
        }
    }

    override fun onHideCustomView() {
        onCustomViewHideCallback()
    }

    override fun onCreateWindow(
        view: WebView?,
        isDialog: Boolean,
        isUserGesture: Boolean,
        resultMsg: android.os.Message?
    ): Boolean {
        if (isPopupBlockingEnabled()) {
            if (!isUserGesture) {
                // Block abusive automated script popup
                onPopupBlockedCallback()
                return false
            }
        }

        // If user tapped a link meant for new tab
        val hrefMsg = view?.handler?.obtainMessage()
        if (hrefMsg != null) {
            view.requestFocusNodeHref(hrefMsg)
            val url = hrefMsg.data.getString("url")
            if (!url.isNullOrEmpty()) {
                onNewTabRequestedCallback(url)
                return false
            }
        }

        return false
    }

    override fun onGeolocationPermissionsShowPrompt(
        origin: String?,
        callback: GeolocationPermissions.Callback?
    ) {
        // Safe default: deny geolocation unless explicitly authorized
        callback?.invoke(origin, false, false)
    }
}
