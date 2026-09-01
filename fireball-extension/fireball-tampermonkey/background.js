/**
 * Fireball Tampermonkey - Background Service Worker
 */

chrome.runtime.onInstalled.addListener(() => {
  // Initialize default demo userscript
  chrome.storage.local.get(['userscripts'], (res) => {
    if (!res.userscripts || res.userscripts.length === 0) {
      const defaultScript = {
        id: 'script_dark_mode',
        name: 'Universal Dark Reader',
        author: 'Fireball',
        version: '1.0',
        matchPattern: '*://*/*',
        enabled: true,
        code: `
          // ==UserScript==
          // @name         Universal Smooth Scroll & Contrast
          // @match        *://*/*
          // ==/UserScript==
          console.log('⚡ [Tampermonkey] UserScript injected successfully!');
        `
      };
      chrome.storage.local.set({ userscripts: [defaultScript] });
    }
  });
});
