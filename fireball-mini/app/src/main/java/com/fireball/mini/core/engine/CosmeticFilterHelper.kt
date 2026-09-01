package com.fireball.mini.core.engine

import android.webkit.WebView

/**
 * Brave Shields Style Cosmetic Filter & Scriptlet Injection Engine for Fireball Mini.
 * Combines EasyList, EasyPrivacy, and ABPVN regional cosmetic rules with
 * a real-time DOM MutationObserver to hide and remove ads dynamically.
 */
object CosmeticFilterHelper {

    // Minified comprehensive cosmetic CSS rules (Brave Shields + EasyList + ABPVN)
    private val COSMETIC_SELECTORS = listOf(
        // General Ad Containers & Banner Units
        ".adsbygoogle", ".ad-unit", ".ad-container", ".ad-banner", ".banner-ads",
        ".banner-top", ".top-banner", ".header-banner", ".ad-wrapper", ".ad-holder",
        "div[id*='google_ads']", "div[class*='google-ads']", "div[id*='ad-slot']",
        "div[class*='ad-slot']", "div[id^='ad_']", "div[class^='ad_']",
        "div[id*='banner_']", "div[class*='banner_']", "div[class*='Banner']",
        "div[class*='TopBanner']", "div[class*='HeaderBanner']", "div[class*='ad-box']",
        "div[id*='dfp']", "div[class*='dfp']", "[data-ad-slot]", "[data-ad-client]",

        // Vietnamese Regional Ad Networks (ABPVN / Tinhte / VnExpress / DanTri / GenK / TuoiTre)
        ".qc_wrapper", ".qc-header", ".qc-top", ".qc-body", ".adv-banner", ".adv-top",
        "[id*='admicro']", "[class*='admicro']", "[id*='eclick']", "[class*='eclick']",
        "[id*='ambient']", "[class*='ambient']", "[id*='quangcao']", "[class*='quangcao']",
        "[id*='banner-tinhte']", "[class*='banner-tinhte']", "[id*='masthead']", "[class*='masthead']",
        ".banner-fixed", ".banner-sticky", ".sponsor-post", ".sponsored-box", ".sponsor-badge",
        "div[class*='Sponsor']", "div[class*='Sponsored']", "div[class*='Promotion']",

        // Third-Party Ad & Recommendation Widgets
        ".outbrain", ".taboola", ".adblade", ".zergnet", ".hybrid-ad", ".pub_300x250",
        ".mgid-container", ".adtrue-unit", "#carbonads", "#advertisement", ".native-ad",

        // Cookie Banners & Overlay Annoyances
        "#onetrust-banner-sdk", ".cookie-banner", ".cookie-notice", ".didomi-popup-notice",
        ".ytp-ad-module", ".ytp-ad-overlay-container",

        // Tracking & Sponsored anchor wrappers
        "a[href*='admicro.vn']", "a[href*='eclick.vn']", "a[href*='ants.vn']",
        "a[href*='doubleclick.net']", "a[href*='taboola.com']"
    )

    private val MINIFIED_CSS = COSMETIC_SELECTORS.joinToString(",") +
            " { display: none !important; opacity: 0 !important; visibility: hidden !important; " +
            "pointer-events: none !important; height: 0 !important; max-height: 0 !important; " +
            "margin: 0 !important; padding: 0 !important; width: 0 !important; max-width: 0 !important; }"

    // Brave Shields DOM MutationObserver Scriptlet
    private val SHIELDS_SCRIPTLET = """
        (function() {
            if (window.__fireball_shields_active) return;
            window.__fireball_shields_active = true;

            function applyShieldsCss() {
                var style = document.getElementById('fireball-shields-css');
                if (!style) {
                    style = document.createElement('style');
                    style.id = 'fireball-shields-css';
                    style.type = 'text/css';
                    style.appendChild(document.createTextNode('$MINIFIED_CSS'));
                    (document.head || document.documentElement).appendChild(style);
                }
            }

            function zapAdElements() {
                var selectors = '${COSMETIC_SELECTORS.joinToString(",")}';
                try {
                    var nodes = document.querySelectorAll(selectors);
                    for (var i = 0; i < nodes.length; i++) {
                        nodes[i].style.setProperty('display', 'none', 'important');
                        nodes[i].style.setProperty('visibility', 'hidden', 'important');
                        nodes[i].style.setProperty('height', '0px', 'important');
                    }
                } catch(e) {}
            }

            // Apply immediately
            applyShieldsCss();
            zapAdElements();

            // Continuous MutationObserver (Brave Shields reactive element zapping)
            if (window.MutationObserver) {
                var observer = new MutationObserver(function(mutations) {
                    applyShieldsCss();
                    zapAdElements();
                });
                observer.observe(document.documentElement || document.body, {
                    childList: true,
                    subtree: true
                });
            }

            // Handle post-DOM load
            if (document.readyState === 'loading') {
                document.addEventListener('DOMContentLoaded', zapAdElements);
            }
        })();
    """.trimIndent()

    fun injectCosmeticCss(webView: WebView, profileId: String = "default", url: String = "") {
        webView.post {
            webView.evaluateJavascript(SHIELDS_SCRIPTLET, null)
        }
    }
}
