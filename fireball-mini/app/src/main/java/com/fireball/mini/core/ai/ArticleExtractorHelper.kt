package com.fireball.mini.core.ai

import com.fireball.mini.core.models.ReaderArticle
import org.json.JSONObject

object ArticleExtractorHelper {

    val extractionJs: String = """
        (function() {
            try {
                // 1. Get Title
                let title = document.title || '';
                const ogTitle = document.querySelector('meta[property="og:title"]');
                if (ogTitle && ogTitle.content) {
                    title = ogTitle.content;
                } else {
                    const h1 = document.querySelector('h1');
                    if (h1 && h1.innerText.trim().length > 0) {
                        title = h1.innerText.trim();
                    }
                }

                // 2. Get Author / Byline
                let byline = '';
                const authorMeta = document.querySelector('meta[name="author"], meta[property="article:author"]');
                if (authorMeta && authorMeta.content) {
                    byline = authorMeta.content;
                }

                // 3. Find Article Content Container
                const candidates = [
                    'article',
                    '[role="main"]',
                    '.article-content',
                    '.post-content',
                    '.entry-content',
                    '.story-body',
                    'main',
                    '#content',
                    '.content'
                ];

                let mainElement = null;
                for (const selector of candidates) {
                    const el = document.querySelector(selector);
                    if (el && el.innerText.trim().length > 200) {
                        mainElement = el.cloneNode(true);
                        break;
                    }
                }

                if (!mainElement) {
                    mainElement = document.body.cloneNode(true);
                }

                // Strip unwanted elements
                const selectorsToRemove = [
                    'script', 'style', 'iframe', 'noscript', 'nav', 'header', 'footer',
                    '.advertisement', '.ad', '.ads', '.banner', '.social-share',
                    '.comments', '#comments', '.sidebar', '#sidebar', '.widget'
                ];
                for (const sel of selectorsToRemove) {
                    const junk = mainElement.querySelectorAll(sel);
                    junk.forEach(e => e.remove());
                }

                // Extract cleaned paragraphs and HTML
                const paragraphs = Array.from(mainElement.querySelectorAll('p, h2, h3, blockquote, ul, ol'))
                    .map(p => p.innerText.trim())
                    .filter(text => text.length > 20);

                const plainText = paragraphs.join('\n\n');
                const contentHtml = mainElement.innerHTML;

                return JSON.stringify({
                    title: title,
                    byline: byline,
                    url: window.location.href,
                    domain: window.location.hostname,
                    plainText: plainText,
                    contentHtml: contentHtml,
                    wordCount: plainText.split(/\s+/).filter(w => w.length > 0).length
                });
            } catch (e) {
                return JSON.stringify({
                    title: document.title || 'Page',
                    byline: '',
                    url: window.location.href,
                    domain: window.location.hostname,
                    plainText: document.body.innerText.substring(0, 4000),
                    contentHtml: '',
                    wordCount: 100
                });
            }
        })();
    """.trimIndent()

    fun parseExtractedJson(rawJson: String, fallbackUrl: String): ReaderArticle {
        return try {
            val unquoted = if (rawJson.startsWith("\"") && rawJson.endsWith("\"")) {
                // Remove outer JSON string escaping if returned by evaluateJavascript
                try {
                    org.json.JSONTokener(rawJson).nextValue().toString()
                } catch (_: Throwable) {
                    rawJson.removeSurrounding("\"").replace("\\\"", "\"").replace("\\\\", "\\")
                }
            } else {
                rawJson
            }

            var title: String? = null
            var byline: String? = null
            var url: String? = null
            var domain: String? = null
            var plainText: String? = null
            var contentHtml: String? = null
            var wordCount: Int? = null

            try {
                val json = JSONObject(unquoted)
                title = json.optString("title").takeIf { it.isNotBlank() }
                byline = json.optString("byline").takeIf { it.isNotBlank() }
                url = json.optString("url").takeIf { it.isNotBlank() }
                domain = json.optString("domain").takeIf { it.isNotBlank() }
                plainText = json.optString("plainText").takeIf { it.isNotBlank() }
                contentHtml = json.optString("contentHtml").takeIf { it.isNotBlank() }
                val wc = json.optInt("wordCount", -1)
                if (wc > 0) wordCount = wc
            } catch (_: Throwable) {
                // Fallback to regex parsing if JSONObject is stubbed or fails
            }

            if (title == null) title = extractStringField(unquoted, "title") ?: "Untitled Article"
            if (byline == null) byline = extractStringField(unquoted, "byline")
            if (url == null) url = extractStringField(unquoted, "url") ?: fallbackUrl
            if (domain == null) domain = extractStringField(unquoted, "domain") ?: fallbackUrl.removePrefix("https://").removePrefix("http://").substringBefore('/')
            if (plainText == null) plainText = extractStringField(unquoted, "plainText") ?: ""
            if (contentHtml == null) contentHtml = extractStringField(unquoted, "contentHtml") ?: ""
            if (wordCount == null) {
                val wcParsed = extractIntField(unquoted, "wordCount")
                wordCount = wcParsed ?: maxOf(1, plainText.split("\\s+".toRegex()).count { it.isNotBlank() })
            }

            val readingTime = maxOf(1, (wordCount / 200))

            ReaderArticle(
                url = url,
                title = title,
                byline = byline,
                domain = domain,
                contentHtml = contentHtml,
                plainText = plainText,
                estimatedReadingTimeMinutes = readingTime
            )
        } catch (e: Exception) {
            ReaderArticle(
                url = fallbackUrl,
                title = "Web Page",
                byline = null,
                domain = fallbackUrl,
                contentHtml = "",
                plainText = "",
                estimatedReadingTimeMinutes = 1
            )
        }
    }

    private fun extractStringField(jsonStr: String, key: String): String? {
        val pattern = "\"$key\"\\s*:\\s*\"((?:\\\\.|[^\"\\\\])*)\"".toRegex()
        val match = pattern.find(jsonStr)
        return match?.groupValues?.get(1)
            ?.replace("\\\"", "\"")
            ?.replace("\\n", "\n")
            ?.replace("\\r", "\r")
            ?.replace("\\t", "\t")
            ?.replace("\\\\", "\\")
    }

    private fun extractIntField(jsonStr: String, key: String): Int? {
        val pattern = "\"$key\"\\s*:\\s*(\\d+)".toRegex()
        return pattern.find(jsonStr)?.groupValues?.get(1)?.toIntOrNull()
    }
}
