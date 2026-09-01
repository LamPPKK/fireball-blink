package com.fireball.mini.core.userscripts

/**
 * Userscript Engine for Fireball Android Mini.
 * Injects Tampermonkey & Greasemonkey compatible user scripts into Android WebViews.
 */
object UserscriptEngine {

    data class UserScript(
        val id: String,
        val name: String,
        val matchPattern: String = "*://*/*",
        val enabled: Boolean = true,
        val code: String
    )

    private val installedScripts = mutableListOf<UserScript>(
        UserScript(
            id = "default_enhancer",
            name = "Page Performance & Dark Mode Enhancer",
            matchPattern = "*://*/*",
            enabled = true,
            code = "console.log('⚡ [Fireball Userscript Engine] Mobile user script active.');"
        )
    )

    fun getScriptsForUrl(url: String): List<UserScript> {
        return installedScripts.filter { it.enabled && matchesUrl(url, it.matchPattern) }
    }

    fun generateInjectionPayload(url: String): String {
        val scripts = getScriptsForUrl(url)
        if (scripts.isEmpty()) return ""

        val combinedCode = scripts.joinToString("\n") { it.code }
        return """
            (function() {
                try {
                    $combinedCode
                } catch(e) {
                    console.error('[Fireball Userscript Error]', e);
                }
            })();
        """.trimIndent()
    }

    private fun matchesUrl(url: String, pattern: String): Boolean {
        if (pattern == "*://*/*" || pattern == "<all_urls>") return true
        val regex = pattern.replace(".", "\\.").replace("*", ".*").toRegex()
        return regex.containsMatchIn(url)
    }
}
