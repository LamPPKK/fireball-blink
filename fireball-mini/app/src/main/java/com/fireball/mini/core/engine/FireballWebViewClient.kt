package com.fireball.mini.core.engine

import android.graphics.Bitmap
import android.net.http.SslError
import android.os.Handler
import android.os.Looper
import android.webkit.SslErrorHandler
import android.webkit.WebResourceRequest
import android.webkit.WebResourceResponse
import android.webkit.WebView
import android.webkit.WebViewClient
import com.fireball.mini.core.FireballNativeBridge
import com.fireball.mini.core.models.DiscoveredMedia
import java.io.ByteArrayInputStream

class FireballWebViewClient(
    private val tabId: String,
    private val profileId: String,
    private val onPageStartedCallback: (url: String, favicon: Bitmap?) -> Unit,
    private val onPageFinishedCallback: (url: String, title: String?) -> Unit,
    private val onAdBlockedCallback: (categoryCode: Int) -> Unit,
    private val onMediaDiscoveredCallback: (DiscoveredMedia) -> Unit,
    private val isRedirectBlockingEnabled: () -> Boolean = { true },
    private val onRedirectBlockedCallback: (url: String) -> Unit = {}
) : WebViewClient() {

    private val mainHandler = Handler(Looper.getMainLooper())
    @Volatile
    private var currentPageTitle: String = "Page"
    @Volatile
    private var currentSourceHost: String = ""

    override fun shouldOverrideUrlLoading(view: WebView?, request: WebResourceRequest?): Boolean {
        val url = request?.url?.toString() ?: return false
        val uri = request.url

        // 1. Intercept abusive app store / intent redirect schemes
        val scheme = uri.scheme?.lowercase() ?: ""
        if (scheme != "http" && scheme != "https" && scheme != "about" && scheme != "data" && scheme != "blob") {
            if (isRedirectBlockingEnabled()) {
                mainHandler.post {
                    onRedirectBlockedCallback(url)
                }
                return true // Drop abusive redirect intent
            }
        }

        // 2. Check if the redirected target is an ad/malware network
        val destHost = uri.host ?: ""
        if (isRedirectBlockingEnabled() && destHost.isNotEmpty() && currentSourceHost.isNotEmpty() && destHost != currentSourceHost) {
            val cat = FireballNativeBridge.evaluateRequestCategory(profileId, url, currentSourceHost, destHost)
            if (cat != 0) {
                mainHandler.post {
                    onRedirectBlockedCallback(url)
                    onAdBlockedCallback(cat)
                }
                return true // Block redirect to ad/malware domain
            }
        }

        if (request.isForMainFrame) {
            val cleanedUrl = UrlCleanerHelper.cleanUrlIfNeeded(url)
            if (cleanedUrl != url) {
                view?.loadUrl(cleanedUrl)
                return true
            }
        }
        return false
    }

    override fun shouldInterceptRequest(view: WebView?, request: WebResourceRequest?): WebResourceResponse? {
        val uri = request?.url ?: return null
        val url = uri.toString()
        val destHost = uri.host ?: ""
        val referer = request.requestHeaders["Referer"]
        val sourceHost = if (!referer.isNullOrEmpty()) {
            try { java.net.URI(referer).host ?: currentSourceHost } catch (e: Exception) { currentSourceHost }
        } else {
            currentSourceHost
        }

        // 1. Sniff Media Candidate
        val mimeType = request.requestHeaders["Accept"] ?: ""
        val discovered = MediaSnifferHelper.inspectResource(
            tabId = tabId,
            url = url,
            mimeType = mimeType,
            pageTitle = currentPageTitle
        )
        if (discovered != null) {
            mainHandler.post {
                onMediaDiscoveredCallback(discovered)
            }
        }

        // 2. Evaluate Multi-Category Adblock Engine
        val blockCategory = FireballNativeBridge.evaluateRequestCategory(profileId, url, sourceHost, destHost)
        if (blockCategory != 0) {
            mainHandler.post {
                onAdBlockedCallback(blockCategory)
            }
            // Return empty response to drop the request
            return WebResourceResponse("text/plain", "UTF-8", ByteArrayInputStream(ByteArray(0)))
        }

        return super.shouldInterceptRequest(view, request)
    }

    override fun onPageStarted(view: WebView?, url: String?, favicon: Bitmap?) {
        super.onPageStarted(view, url, favicon)
        url?.let {
            try {
                currentSourceHost = java.net.URI(it).host ?: ""
            } catch (e: Exception) {
                currentSourceHost = ""
            }
            onPageStartedCallback(it, favicon)
        }
    }

    private fun injectSecurityGuards(view: WebView) {
        view.evaluateJavascript(
            """
            (function() {
                if (window.__fireball_guard_injected) return;
                window.__fireball_guard_injected = true;
                const origOpen = window.open;
                window.open = function(url, target, features) {
                    if (!window.event || !window.event.isTrusted) {
                        console.warn('[Fireball] Blocked untrusted script popup:', url);
                        return null;
                    }
                    return origOpen ? origOpen.apply(this, arguments) : null;
                };
            })();
            """.trimIndent(),
            null
        )
    }

    override fun onPageCommitVisible(view: WebView?, url: String?) {
        super.onPageCommitVisible(view, url)
        if (view != null && url != null) {
            CosmeticFilterHelper.injectCosmeticCss(view, profileId, url)
            injectSecurityGuards(view)
        }
    }

    override fun onPageFinished(view: WebView?, url: String?) {
        super.onPageFinished(view, url)
        if (view != null && url != null) {
            currentPageTitle = view.title ?: "Page"
            CosmeticFilterHelper.injectCosmeticCss(view, profileId, url)
            injectSecurityGuards(view)
            onPageFinishedCallback(url, view.title)
        }
    }

    override fun onReceivedSslError(view: WebView?, handler: SslErrorHandler?, error: SslError?) {
        handler?.cancel()
    }
}
