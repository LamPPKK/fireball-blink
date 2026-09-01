package com.fireball.mini.core.engine

import java.net.URLEncoder
import java.nio.charset.StandardCharsets

object UrlCleanerHelper {

    val TRACKING_PARAMETERS = setOf(
        // Google & Analytics
        "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
        "utm_id", "utm_source_platform", "utm_creative_format", "utm_marketing_tactic",
        "gclid", "gclsrc", "dclid", "wbraid", "gbraid", "_ga", "_gl",

        // Meta / Facebook & Instagram
        "fbclid", "igshid", "fb_action_ids", "fb_action_types", "fb_source", "fb_ref",

        // Microsoft / Bing
        "msclkid",

        // Twitter / X
        "twclid",

        // TikTok & Pinterest & Reddit
        "ttclid", "epik", "rdt_cid",

        // Yandex & Baidu
        "yclid", "ym_tracking", "_openstat", "baiduid",

        // Email & Marketing Automation
        "mc_eid", "mc_cid", "_hsenc", "_hsmi", "mkt_tok", "vero_id", "vero_conv",
        "mailchimp_link_id", "nr_email_referer", "ml_subscriber", "ml_subscriber_hash",

        // Affiliate & Ad Networks
        "sc_client_id", "sc_campaign", "zanpid", "s_kwcid", "s_cid", "si", "spm",
        "scm", "cmpid", "rb_clickid", "gdfms", "gdftrk", "gdffi", "aff_id",
        "aff_sub", "aff_sub2", "aff_sub3", "aff_sub4", "aff_sub5",

        // LinkedIn & Media & Portals
        "trk", "trkCampaign", "trkEmail", "linkId", "guce_referrer", "guce_referrer_sig",
        "ref_src", "ref_url", "_branch_match_id"
    )

    fun cleanUrl(rawUrl: String): String {
        val questionIndex = rawUrl.indexOf('?')
        if (questionIndex == -1 || questionIndex == rawUrl.length - 1) {
            return rawUrl
        }

        val baseUrl = rawUrl.substring(0, questionIndex)
        val queryAndFragment = rawUrl.substring(questionIndex + 1)
        val fragmentIndex = queryAndFragment.indexOf('#')
        val queryString = if (fragmentIndex != -1) queryAndFragment.substring(0, fragmentIndex) else queryAndFragment
        val fragment = if (fragmentIndex != -1) queryAndFragment.substring(fragmentIndex) else ""

        val pairs = queryString.split('&')
        val keptPairs = mutableListOf<String>()
        var hasTracking = false

        for (pair in pairs) {
            if (pair.isEmpty()) continue
            val key = pair.substringBefore('=').lowercase()
            if (TRACKING_PARAMETERS.contains(key) || key.startsWith("utm_")) {
                hasTracking = true
            } else {
                keptPairs.add(pair)
            }
        }

        if (!hasTracking) return rawUrl

        val newQuery = if (keptPairs.isNotEmpty()) "?" + keptPairs.joinToString("&") else ""
        return "$baseUrl$newQuery$fragment"
    }

    fun cleanUrlIfNeeded(url: String, isEnabled: Boolean = true): String {
        return if (isEnabled) cleanUrl(url) else url
    }

    fun isSearchQuery(input: String): Boolean {
        val trimmed = input.trim()
        if (trimmed.isEmpty()) return false
        if (trimmed.startsWith("http://") || trimmed.startsWith("https://") || trimmed.startsWith("about:") || trimmed.startsWith("file://")) {
            return false
        }
        val isDomainLike = trimmed.contains('.') &&
                !trimmed.contains(' ') &&
                trimmed.indexOf('.') > 0 &&
                trimmed.indexOf('.') < trimmed.length - 1
        val isLocalhost = trimmed.startsWith("localhost") || trimmed.startsWith("127.0.0.1")
        return !isDomainLike && !isLocalhost
    }

    fun buildUrlOrSearch(input: String, defaultSearchEngine: String = "https://duckduckgo.com/?q="): String {
        val trimmed = input.trim()
        if (trimmed.isEmpty()) return "about:blank"

        if (trimmed.startsWith("http://") || trimmed.startsWith("https://") || trimmed.startsWith("about:") || trimmed.startsWith("file://")) {
            return trimmed
        }

        if (isSearchQuery(trimmed)) {
            val encoded = try {
                URLEncoder.encode(trimmed, StandardCharsets.UTF_8.toString())
            } catch (e: Exception) {
                trimmed.replace(" ", "+")
            }
            return "$defaultSearchEngine$encoded"
        }

        return "https://$trimmed"
    }
}
