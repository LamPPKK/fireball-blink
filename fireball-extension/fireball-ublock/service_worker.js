/**
 * Fireball uBlock Origin - Background Service Worker
 * Manages rule sets, cosmetic CSS injection, and blocked counters.
 */

let blockedCount = 0;

chrome.declarativeNetRequest?.onRuleMatchedDebug?.addListener((info) => {
  blockedCount++;
  chrome.action.setBadgeText({ text: blockedCount.toString() });
  chrome.action.setBadgeBackgroundColor({ color: '#FF5500' });
});

// Cosmetic filtering injection
chrome.webNavigation?.onCommitted?.addListener((details) => {
  if (details.frameId === 0) {
    chrome.scripting?.insertCSS({
      target: { tabId: details.tabId },
      css: '.ad, .ads, .advertisement, [id^="google_ads_"], [class*="sponsored"] { display: none !important; }'
    }).catch(() => {});
  }
});
