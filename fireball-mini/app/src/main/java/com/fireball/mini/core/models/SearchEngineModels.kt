package com.fireball.mini.core.models

import java.net.URLEncoder
import java.nio.charset.StandardCharsets

/**
 * Represents a search engine definition with support for custom engines and bang shortcuts.
 */
data class SearchEngine(
    val id: String,
    val name: String,
    val searchUrlTemplate: String, // e.g. "https://duckduckgo.com/?q=%s"
    val homeUrl: String = "https://" + searchUrlTemplate.substringAfter("://").substringBefore("/").substringBefore("?"),
    val suggestUrlTemplate: String? = null,
    val iconEmoji: String = "🔍",
    val iconName: String = "search",
    val bangPrefix: String? = null, // e.g. "!d"
    val isCustom: Boolean = false,
    val isDefault: Boolean = false
) {
    val displayName: String get() = name

    /**
     * Builds the full search URL for a given user query.
     */
    fun buildSearchUrl(query: String): String {
        val encodedQuery = URLEncoder.encode(query.trim(), StandardCharsets.UTF_8.name())
        return if (searchUrlTemplate.contains("%s")) {
            searchUrlTemplate.replace("%s", encodedQuery)
        } else {
            "$searchUrlTemplate$encodedQuery"
        }
    }
}

/**
 * Built-in default search engines and DuckDuckGo/Brave style bang shortcut registry.
 */
object SearchEngineDefaults {
    val DUCKDUCKGO = SearchEngine(
        id = "duckduckgo",
        name = "DuckDuckGo",
        searchUrlTemplate = "https://duckduckgo.com/?q=%s",
        homeUrl = "https://duckduckgo.com",
        suggestUrlTemplate = "https://duckduckgo.com/ac/?q=%s&type=list",
        iconEmoji = "🦆",
        iconName = "privacy_shield",
        bangPrefix = "!d",
        isDefault = true
    )

    val GOOGLE = SearchEngine(
        id = "google",
        name = "Google",
        searchUrlTemplate = "https://www.google.com/search?q=%s",
        homeUrl = "https://www.google.com",
        suggestUrlTemplate = "https://suggestqueries.google.com/complete/search?client=chrome&q=%s",
        iconEmoji = "🔍",
        iconName = "search",
        bangPrefix = "!g"
    )

    val BRAVE = SearchEngine(
        id = "brave",
        name = "Brave Search",
        searchUrlTemplate = "https://search.brave.com/search?q=%s",
        homeUrl = "https://search.brave.com",
        suggestUrlTemplate = "https://search.brave.com/api/suggest?q=%s",
        iconEmoji = "🦁",
        iconName = "lion",
        bangPrefix = "!b"
    )

    val BING = SearchEngine(
        id = "bing",
        name = "Microsoft Bing",
        searchUrlTemplate = "https://www.bing.com/search?q=%s",
        homeUrl = "https://www.bing.com",
        suggestUrlTemplate = "https://api.bing.com/osjson.aspx?query=%s",
        iconEmoji = "🌐",
        iconName = "bing",
        bangPrefix = "!bing"
    )

    val ECOSIA = SearchEngine(
        id = "ecosia",
        name = "Ecosia",
        searchUrlTemplate = "https://www.ecosia.org/search?q=%s",
        homeUrl = "https://www.ecosia.org",
        suggestUrlTemplate = "https://ac.ecosia.org/autocomplete?q=%s&type=list",
        iconEmoji = "🌲",
        iconName = "tree",
        bangPrefix = "!e"
    )

    val STARTPAGE = SearchEngine(
        id = "startpage",
        name = "Startpage",
        searchUrlTemplate = "https://www.startpage.com/sp/search?query=%s",
        homeUrl = "https://www.startpage.com",
        suggestUrlTemplate = "https://www.startpage.com/osjson.aspx?query=%s",
        iconEmoji = "🔒",
        iconName = "lock",
        bangPrefix = "!sp"
    )

    val KAGI = SearchEngine(
        id = "kagi",
        name = "Kagi",
        searchUrlTemplate = "https://kagi.com/search?q=%s",
        homeUrl = "https://kagi.com",
        iconEmoji = "⚡",
        iconName = "bolt",
        bangPrefix = "!k"
    )

    val WIKIPEDIA = SearchEngine(
        id = "wikipedia",
        name = "Wikipedia",
        searchUrlTemplate = "https://en.wikipedia.org/wiki/Special:Search?search=%s",
        homeUrl = "https://wikipedia.org",
        iconEmoji = "📚",
        iconName = "book",
        bangPrefix = "!w"
    )

    val YOUTUBE = SearchEngine(
        id = "youtube",
        name = "YouTube",
        searchUrlTemplate = "https://www.youtube.com/results?search_query=%s",
        homeUrl = "https://youtube.com",
        iconEmoji = "▶️",
        iconName = "video",
        bangPrefix = "!yt"
    )

    val GITHUB = SearchEngine(
        id = "github",
        name = "GitHub",
        searchUrlTemplate = "https://github.com/search?q=%s",
        homeUrl = "https://github.com",
        iconEmoji = "🐙",
        iconName = "code",
        bangPrefix = "!gh"
    )

    val BUILT_IN_ENGINES = listOf(
        DUCKDUCKGO,
        GOOGLE,
        BRAVE,
        BING,
        ECOSIA,
        STARTPAGE,
        KAGI
    )

    val BANG_SHORTCUTS = listOf(
        DUCKDUCKGO,
        GOOGLE,
        BRAVE,
        BING,
        ECOSIA,
        STARTPAGE,
        KAGI,
        WIKIPEDIA,
        YOUTUBE,
        GITHUB
    )

    /**
     * Resolves a user input string that might start with a bang (!g, !yt, !w) or regular query.
     * Returns the target URL to load.
     */
    fun resolveQueryOrUrl(
        input: String,
        defaultEngine: SearchEngine = DUCKDUCKGO
    ): String {
        val trimmed = input.trim()
        if (trimmed.isEmpty()) return "about:blank"

        // Check if already a full URL
        if (trimmed.startsWith("http://", ignoreCase = true) ||
            trimmed.startsWith("https://", ignoreCase = true) ||
            trimmed.startsWith("file://", ignoreCase = true) ||
            trimmed.startsWith("about:", ignoreCase = true)
        ) {
            return trimmed
        }

        // Check if it's a domain name (e.g. "github.com", "duckduckgo.com/about", "localhost:8080")
        val isDomainLike = trimmed.matches(Regex("""^([a-zA-Z0-9-]+\.)+[a-zA-Z]{2,}(/.*)?$""")) ||
                trimmed.matches(Regex("""^localhost(:\d+)?(/.*)?$"""))
        if (isDomainLike && !trimmed.contains(" ")) {
            return "https://$trimmed"
        }

        // Check for Bang shortcut (!g query, !yt trailer, etc.)
        val firstSpaceIndex = trimmed.indexOf(' ')
        val bangCandidate = if (firstSpaceIndex != -1) trimmed.substring(0, firstSpaceIndex) else trimmed
        val queryAfterBang = if (firstSpaceIndex != -1) trimmed.substring(firstSpaceIndex + 1).trim() else ""

        val matchedBangEngine = BANG_SHORTCUTS.firstOrNull { it.bangPrefix.equals(bangCandidate, ignoreCase = true) }
        if (matchedBangEngine != null) {
            return if (queryAfterBang.isNotEmpty()) {
                matchedBangEngine.buildSearchUrl(queryAfterBang)
            } else {
                matchedBangEngine.homeUrl
            }
        }

        // Fallback to active default search engine
        return defaultEngine.buildSearchUrl(trimmed)
    }
}
