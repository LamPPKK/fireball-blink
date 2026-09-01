package com.fireball.mini.core.ruffle

/**
 * Ruffle WebAssembly Flash Player Helper for Fireball Android Mini.
 * Injects Ruffle WASM polyfill into web pages to play retro Flash games & animations.
 */
object RuffleHelper {

    val ruffleInjectionScript = """
        (function() {
            if (window.fireballRuffleInjected) return;
            window.fireballRuffleInjected = true;
            
            // Inject Ruffle WebAssembly loader script
            const script = document.createElement('script');
            script.src = 'https://unpkg.com/@ruffle-rs/ruffle';
            script.async = true;
            script.onload = function() {
                console.log('🎮 [Fireball] Ruffle Flash Player Emulation Engine Loaded');
                if (window.RufflePlayer) {
                    const ruffle = window.RufflePlayer.newest();
                    const polyfill = ruffle.createPolyfill();
                    polyfill.polyfill();
                }
            };
            document.head.appendChild(script);
        })();
    """.trimIndent()
}
