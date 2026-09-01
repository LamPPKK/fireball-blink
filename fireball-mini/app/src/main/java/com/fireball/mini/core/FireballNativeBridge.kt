package com.fireball.mini.core

import com.fireball.mini.core.models.MediaKind

object FireballNativeBridge {
    private const val TAG = "FireballNativeBridge"
    private var isNativeLoaded = false

    init {
        try {
            System.loadLibrary("fireball_core")
            isNativeLoaded = true
        } catch (e: Throwable) {
            isNativeLoaded = false
        }
    }

    // Native JNI signatures
    private external fun nativeCleanUrl(rawUrl: String): String
    private external fun nativeEvaluateRequest(
        profileId: String,
        requestUrl: String,
        sourceHost: String,
        destHost: String
    ): Int
    private external fun nativeGetCosmeticCss(profileId: String, hostname: String): String
    private external fun nativeSniffMedia(url: String, mimeType: String): Int

    fun cleanUrl(rawUrl: String): String {
        if (isNativeLoaded) {
            try {
                return nativeCleanUrl(rawUrl)
            } catch (e: Throwable) {
                // fallback
            }
        }
        return fallbackCleanUrl(rawUrl)
    }

    fun evaluateRequest(profileId: String, requestUrl: String, sourceHost: String, destHost: String): Boolean {
        return evaluateRequestCategory(profileId, requestUrl, sourceHost, destHost) == 0
    }

    fun evaluateRequestCategory(profileId: String, requestUrl: String, sourceHost: String, destHost: String): Int {
        // 0 = ALLOW, 1 = ADS, 2 = TRACKERS, 3 = ANNOYANCES, 4 = MALWARE, 5 = SOCIAL
        if (isNativeLoaded) {
            try {
                return nativeEvaluateRequest(profileId, requestUrl, sourceHost, destHost)
            } catch (e: Throwable) {
                // fallback
            }
        }
        return fallbackEvaluateRequestCategory(requestUrl, destHost)
    }

    fun getCosmeticCss(profileId: String, hostname: String): String {
        if (isNativeLoaded) {
            try {
                return nativeGetCosmeticCss(profileId, hostname)
            } catch (e: Throwable) {
                // fallback
            }
        }
        return "div[id^='google_ads_'], div[id^='ad-slot'], div[class*='ad-banner'], " +
                "ins.adsbygoogle, .advertisement, [data-ad-slot], [data-ad-client], .fb-ad, " +
                ".taboola, .outbrain, #onetrust-banner-sdk, .cookie-banner, .ytp-ad-module " +
                "{ display: none !important; opacity: 0 !important; visibility: hidden !important; }"
    }

    fun sniffMedia(url: String, mimeType: String): MediaKind? {
        if (isNativeLoaded) {
            try {
                return when (nativeSniffMedia(url, mimeType)) {
                    1 -> MediaKind.DIRECT_AUDIO
                    2 -> MediaKind.DIRECT_VIDEO
                    3 -> MediaKind.HLS_VOD
                    4 -> MediaKind.DASH_VOD
                    else -> null
                }
            } catch (e: Throwable) {
                // fallback
            }
        }
        return fallbackSniffMedia(url, mimeType)
    }

    // High-performance Kotlin fallbacks (Pure String parsing, 100% JVM & Android compatible)
    private fun fallbackCleanUrl(rawUrl: String): String {
        if (!rawUrl.contains("?")) return rawUrl
        val trackingKeys = setOf(
            "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
            "utm_id", "utm_source_platform", "fbclid", "gclid", "gbraid", "wbraid",
            "dclid", "msclkid", "mc_eid", "_ga", "_gl", "igshid", "si", "ref_src",
            "twclid", "yclid", "ttclid"
        )
        val queryIndex = rawUrl.indexOf('?')
        val base = rawUrl.substring(0, queryIndex)
        val queryWithHash = rawUrl.substring(queryIndex + 1)
        val hashIndex = queryWithHash.indexOf('#')
        val query = if (hashIndex != -1) queryWithHash.substring(0, hashIndex) else queryWithHash
        val fragment = if (hashIndex != -1) queryWithHash.substring(hashIndex) else ""

        val kept = query.split("&").filter { param ->
            if (param.isEmpty()) false
            else {
                val key = param.substringBefore('=').lowercase()
                !trackingKeys.contains(key)
            }
        }
        return if (kept.isEmpty()) "$base$fragment" else "$base?${kept.joinToString("&")}$fragment"
    }

    private fun fallbackEvaluateRequestCategory(url: String, destHost: String): Int {
        val lowerUrl = url.lowercase()
        val lowerHost = destHost.lowercase()

        val adDomains = listOf(
            "doubleclick.net", "googlesyndication.com", "googleadservices.com",
            "adnxs.com", "criteo.com", "adservice.google", "pagead2.googlesyndication.com",
            "taboola.com", "outbrain.com", "amazon-adsystem.com", "rubiconproject.com",
            "pubmatic.com", "openx.net", "applovin.com", "unityads.unity3d.com",
            "ironsrc.com", "vungle.com", "mintegral.com", "popads.net", "propellerads.com",
            "exoclick.com", "trafficstars.com", "zedo.com", "adroll.com", "smartadserver.com",
            "moatads.com", "serving-sys.com", "flashtalking.com", "adform.net", "inmobi.com",
            // Vietnamese & Regional Ad Networks (ABPVN)
            "admicro.vn", "eclick.vn", "ambientplatform.vn", "ants.vn", "novanet.vn",
            "vietnamnetad.vn", "admatic.vn", "adpia.vn", "accesstrade.vn", "mgid.com",
            "adtrue.com", "adtarget.me", "vlit.vn", "blueseed.tv", "yomedia.vn",
            "adnetwork.vn", "innity.com", "adclick.vn", "vcmedia.vn/ad", "vnecdn.net/ads",
            "ad.tinhte.vn", "tinhte.vn/ads", "media.tinhte.vn/ads"
        )
        for (domain in adDomains) {
            if (lowerHost.contains(domain) || lowerUrl.contains(domain)) return 1
        }

        val trackerDomains = listOf(
            "google-analytics.com", "analytics.google.com", "hotjar.com", "clarity.ms",
            "mixpanel.com", "segment.io", "amplitude.com", "appsflyer.com", "adjust.com",
            "mc.yandex.ru", "scorecardresearch.com", "branch.io", "kochava.com", "singular.net"
        )
        for (domain in trackerDomains) {
            if (lowerHost.contains(domain) || lowerUrl.contains(domain)) return 2
        }

        val annoyanceDomains = listOf(
            "onetrust.com", "cookielaw.org", "cookiebot.com", "didomi.io", "trustarc.com",
            "usercentrics.eu", "iubenda.com", "quantserve.com"
        )
        for (domain in annoyanceDomains) {
            if (lowerHost.contains(domain) || lowerUrl.contains(domain)) return 3
        }

        val malwareDomains = listOf(
            "coinhive.com", "crypto-loot.com", "jsecoin.com", "authedmine.com", "badsite-phishing.com"
        )
        for (domain in malwareDomains) {
            if (lowerHost.contains(domain) || lowerUrl.contains(domain)) return 4
        }

        val socialDomains = listOf(
            "connect.facebook.net", "facebook.net/tr", "analytics.tiktok.com", "platform.twitter.com"
        )
        for (domain in socialDomains) {
            if (lowerHost.contains(domain) || lowerUrl.contains(domain)) return 5
        }

        return 0

    }

    private fun fallbackSniffMedia(url: String, mimeType: String): MediaKind? {
        val lowerUrl = url.lowercase()
        val lowerMime = mimeType.lowercase()
        return when {
            lowerUrl.contains(".m3u8") || lowerMime.contains("mpegurl") -> MediaKind.HLS_VOD
            lowerUrl.contains(".mpd") || lowerMime.contains("dash+xml") -> MediaKind.DASH_VOD
            lowerUrl.contains(".mp4") || lowerMime.contains("video/mp4") || lowerMime.contains("video/webm") -> MediaKind.DIRECT_VIDEO
            lowerUrl.contains(".mp3") || lowerMime.contains("audio/mpeg") -> MediaKind.DIRECT_AUDIO
            else -> null
        }
    }
}
