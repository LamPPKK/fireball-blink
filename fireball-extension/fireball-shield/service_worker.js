/**
 * Fireball Shield - Background Service Worker
 * Strips tracking parameters & enforces WebRTC privacy.
 */

const TRACKING_PARAMS = [
  'utm_source', 'utm_medium', 'utm_campaign', 'utm_term', 'utm_content',
  'fbclid', 'gclid', 'msclkid', 'mc_eid', '_ga', '_hsenc', 'yclid'
];

chrome.webNavigation?.onBeforeNavigate?.addListener((details) => {
  if (details.frameId !== 0) return;
  try {
    const url = new URL(details.url);
    let modified = false;
    for (const param of TRACKING_PARAMS) {
      if (url.searchParams.has(param)) {
        url.searchParams.delete(param);
        modified = true;
      }
    }
    if (modified) {
      chrome.tabs.update(details.tabId, { url: url.toString() });
    }
  } catch (_e) {}
});

// Enforce WebRTC IP leak protection
if (chrome.privacy?.network?.webRTCIPHandlingPolicy) {
  chrome.privacy.network.webRTCIPHandlingPolicy.set({
    value: 'disable_non_proxied_udp'
  });
}
