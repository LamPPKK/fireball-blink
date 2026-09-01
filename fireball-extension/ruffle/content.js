/**
 * Fireball Ruffle Flash Emulator & Retro Game Interceptor
 * Automatically detects and polyfills Adobe Flash (.swf) content with Rust/WebAssembly Ruffle.
 */

(function () {
  'use strict';

  function isFlashElement(el) {
    if (!el || !el.tagName) return false;
    const tag = el.tagName.toLowerCase();
    if (tag === 'embed' || tag === 'object') {
      const type = (el.getAttribute('type') || '').toLowerCase();
      const src = (el.getAttribute('src') || el.getAttribute('data') || '').toLowerCase();
      if (type.includes('shockwave-flash') || src.endsWith('.swf') || src.includes('.swf?')) {
        return true;
      }
      // Check param tags inside object
      const params = el.querySelectorAll('param[name="movie"], param[name="src"]');
      for (const p of params) {
        if ((p.getAttribute('value') || '').toLowerCase().includes('.swf')) {
          return true;
        }
      }
    }
    return false;
  }

  function polyfillFlashElements() {
    const embeds = document.querySelectorAll('embed, object');
    let flashCount = 0;

    embeds.forEach((el) => {
      if (isFlashElement(el) && !el.dataset.rufflePolyfilled) {
        el.dataset.rufflePolyfilled = 'true';
        flashCount++;

        const src = el.getAttribute('src') || el.getAttribute('data') || '';
        const width = el.getAttribute('width') || el.clientWidth || 640;
        const height = el.getAttribute('height') || el.clientHeight || 480;

        // Create Ruffle Player Container
        const rufflePlayer = document.createElement('ruffle-embed');
        rufflePlayer.setAttribute('src', src);
        rufflePlayer.setAttribute('width', width);
        rufflePlayer.setAttribute('height', height);
        rufflePlayer.style.display = 'block';

        el.parentNode.replaceChild(rufflePlayer, el);
      }
    });

    // Check if the current page itself is a direct .swf URL
    if (window.location.pathname.toLowerCase().endsWith('.swf')) {
      if (!document.getElementById('fireball-ruffle-stage')) {
        document.body.innerHTML = `
          <div id="fireball-ruffle-stage" style="width:100vw;height:100vh;background:#0D0E12;display:flex;align-items:center;justify-content:center;margin:0;overflow:hidden;">
            <ruffle-embed src="${window.location.href}" style="width:100%;height:100%;"></ruffle-embed>
          </div>
        `;
      }
    }

    return flashCount;
  }

  // Run on page load and observe dynamic DOM changes
  if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', polyfillFlashElements);
    } else {
      polyfillFlashElements();
    }

    const observer = new MutationObserver(() => {
      polyfillFlashElements();
    });
    observer.observe(document.documentElement, { childList: true, subtree: true });
  }

  // Export for testing
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = { isFlashElement, polyfillFlashElements };
  }
})();
