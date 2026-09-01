package com.fireball.mini.core.engine

import android.webkit.WebView

object CosmeticFilterHelper {

    private const val COSMETIC_CSS = """
        .adsbygoogle, .ad-unit, .ad-container, .ad-banner, .banner-ads,
        div[id*="google_ads"], div[class*="google-ads"], div[id*="ad-slot"],
        div[class*="ad-slot"], iframe[src*="doubleclick"], iframe[src*="googlesyndication"],
        iframe[src*="adservice"], iframe[src*="adsystem"], .outbrain, .taboola,
        .adblade, .zergnet, .hybrid-ad, .sponsor-badge, .promoted-tweet,
        .pub_300x250, [data-ad-slot], [data-ad-client], .ad-wrapper,
        #carbonads, #advertisement, .native-ad, .sponsor-card {
            display: none !important;
            opacity: 0 !important;
            visibility: hidden !important;
            pointer-events: none !important;
            height: 0 !important;
            max-height: 0 !important;
            margin: 0 !important;
            padding: 0 !important;
        }
    """

    private val INJECTION_SCRIPT = """
        (function() {
            try {
                if (document.getElementById('fireball-cosmetic-shield')) return;
                var style = document.createElement('style');
                style.id = 'fireball-cosmetic-shield';
                style.type = 'text/css';
                style.innerHTML = ${"\"\"\""}$COSMETIC_CSS${"\"\"\""};
                (document.head || document.documentElement).appendChild(style);
            } catch(e) {}
        })();
    """.trimIndent()

    fun injectCosmeticCss(webView: WebView, profileId: String = "default", url: String = "") {
        webView.post {
            webView.evaluateJavascript(INJECTION_SCRIPT, null)
        }
    }
}
