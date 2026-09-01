/**
 * Fireball WebStore Interceptor
 * Injects 1-Click "Add to Fireball" button on Chrome Web Store & Microsoft Edge Add-ons.
 */

(function() {
  const isChromeStore = window.location.hostname.includes('chromewebstore.google.com') || window.location.hostname.includes('chrome.google.com');
  const isEdgeStore = window.location.hostname.includes('microsoftedge.microsoft.com');

  if (!isChromeStore && !isEdgeStore) return;

  console.log('⚡ [Fireball] WebStore Interceptor active.');

  function injectInstallButton() {
    if (document.getElementById('fireball-install-btn')) return;

    const btn = document.createElement('button');
    btn.id = 'fireball-install-btn';
    btn.textContent = '🔥 Add to Fireball';
    btn.style.cssText = `
      position: fixed;
      bottom: 24px;
      right: 24px;
      z-index: 999999;
      background: #00FF88;
      color: #000;
      font-weight: bold;
      font-family: -apple-system, BlinkMacSystemFont, sans-serif;
      font-size: 14px;
      padding: 12px 20px;
      border-radius: 24px;
      border: none;
      cursor: pointer;
      box-shadow: 0 8px 24px rgba(0, 255, 136, 0.4);
      transition: transform 0.2s ease;
    `;

    btn.onmouseover = () => { btn.style.transform = 'scale(1.05)'; };
    btn.onmouseout = () => { btn.style.transform = 'scale(1)'; };

    btn.onclick = () => {
      const url = window.location.href;
      btn.textContent = '⏳ Fetching CRX...';
      
      chrome.runtime.sendMessage({
        type: 'FIREBALL_INSTALL_EXTENSION',
        storeUrl: url
      }, (res) => {
        if (res && res.success) {
          btn.textContent = '✅ Installed to Fireball!';
          btn.style.background = '#00E676';
        } else {
          btn.textContent = '📥 Download Started';
        }
      });
    };

    document.body.appendChild(btn);
  }

  // Poll for SPA navigation
  setInterval(injectInstallButton, 1000);
})();
