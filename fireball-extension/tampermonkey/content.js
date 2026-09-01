/**
 * Fireball Tampermonkey - Userscript Injection Engine
 * Executes matching .user.js scripts with standard GM_* polyfills.
 */

(function() {
  const GM_POLYFILL = `
    const GM_info = { script: { name: 'Fireball UserScript', version: '1.0' } };
    function GM_log(msg) { console.log('[Tampermonkey]', msg); }
    function GM_addStyle(css) {
      const style = document.createElement('style');
      style.textContent = css;
      (document.head || document.documentElement).appendChild(style);
    }
    function GM_setValue(key, val) { localStorage.setItem('GM_' + key, JSON.stringify(val)); }
    function GM_getValue(key, def) {
      const val = localStorage.getItem('GM_' + key);
      return val ? JSON.parse(val) : def;
    }
  `;

  // Fetch installed scripts from extension storage
  chrome.storage?.local?.get(['userscripts'], (res) => {
    const scripts = res.userscripts || [];
    const currentUrl = window.location.href;

    for (const script of scripts) {
      if (!script.enabled) continue;
      if (matchesPattern(currentUrl, script.matchPattern || '*://*/*')) {
        injectUserScript(script.code);
      }
    }
  });

  function matchesPattern(url, pattern) {
    if (pattern === '<all_urls>' || pattern === '*://*/*') return true;
    const regex = new RegExp('^' + pattern.replace(/\*/g, '.*') + '$');
    return regex.test(url);
  }

  function injectUserScript(code) {
    const scriptEl = document.createElement('script');
    scriptEl.textContent = `(function() { ${GM_POLYFILL} \n ${code} \n})();`;
    (document.head || document.documentElement).appendChild(scriptEl);
    scriptEl.remove();
  }
})();
