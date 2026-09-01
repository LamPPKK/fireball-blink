package com.fireball.mini.core.engine

import android.webkit.WebView

object ReaderModeHelper {

    private val TOGGLE_READER_SCRIPT = """
        (function() {
            var readerContainer = document.getElementById('fireball-reader-container');
            if (readerContainer) {
                readerContainer.remove();
                document.body.style.display = '';
                return 'disabled';
            }

            // Extract main article or content
            var article = document.querySelector('article') || document.querySelector('main') || document.body;
            var title = document.title || 'Reader View';
            var contentHtml = article.innerHTML;

            // Hide normal body
            document.body.style.display = 'none';

            // Create reader container
            var container = document.createElement('div');
            container.id = 'fireball-reader-container';
            container.style.position = 'fixed';
            container.style.top = '0';
            container.style.left = '0';
            container.style.width = '100%';
            container.style.height = '100%';
            container.style.backgroundColor = '#121212';
            container.style.color = '#E0E0E0';
            container.style.fontFamily = 'Georgia, serif';
            container.style.fontSize = '18px';
            container.style.lineHeight = '1.6';
            container.style.padding = '24px 20px';
            container.style.boxSizing = 'border-box';
            container.style.overflowY = 'auto';
            container.style.zIndex = '999999';

            container.innerHTML = '<h1 style="font-size: 24px; margin-bottom: 16px; color: #FFFFFF; font-family: sans-serif;">' + title + '</h1>' +
                                  '<div style="max-width: 680px; margin: 0 auto;">' + contentHtml + '</div>';

            document.documentElement.appendChild(container);
            return 'enabled';
        })();
    """.trimIndent()

    fun toggleReaderMode(webView: WebView, onResult: ((Boolean) -> Unit)? = null) {
        webView.post {
            webView.evaluateJavascript(TOGGLE_READER_SCRIPT) { result ->
                val isEnabled = result?.contains("enabled") == true
                onResult?.invoke(isEnabled)
            }
        }
    }
}
