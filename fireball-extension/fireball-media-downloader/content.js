/**
 * Fireball Media Sniffer Content Script
 * Detects HTML5 video/audio, HLS (.m3u8), DASH (.mpd), and blob streams
 */

(function () {
  const discoveredUrls = new Set();

  function scanMediaElements() {
    // 1. Scan <video> and <audio> elements
    const mediaElements = document.querySelectorAll("video, audio, source");
    mediaElements.forEach((el) => {
      const src = el.src || el.currentSrc || el.getAttribute("src");
      if (src && isValidMediaUrl(src) && !discoveredUrls.has(src)) {
        discoveredUrls.add(src);
        reportMedia(src, el.tagName.toLowerCase(), el);
      }
    });

    // 2. Scan links and embedded players
    const anchors = document.querySelectorAll("a[href*='.m3u8'], a[href*='.mp4'], a[href*='.mpd'], a[href*='.webm']");
    anchors.forEach((a) => {
      const href = a.href;
      if (href && !discoveredUrls.has(href)) {
        discoveredUrls.add(href);
        reportMedia(href, "video");
      }
    });
  }

  function isValidMediaUrl(url) {
    if (!url || typeof url !== "string") return false;
    if (url.startsWith("data:") || url.startsWith("javascript:")) return false;
    const lower = url.toLowerCase();
    return (
      lower.includes(".m3u8") ||
      lower.includes(".mpd") ||
      lower.includes(".mp4") ||
      lower.includes(".webm") ||
      lower.includes(".mp3") ||
      lower.includes(".m4a") ||
      lower.includes(".aac") ||
      lower.includes("blob:")
    );
  }

  function reportMedia(url, tagType, el = null) {
    let title = document.title || "Media Stream";
    const isHls = url.includes(".m3u8");
    const isDash = url.includes(".mpd");
    const isAudio = tagType === "audio" || url.includes(".mp3") || url.includes(".m4a");

    let resolution = null;
    if (el && el.videoWidth && el.videoHeight) {
      resolution = `${el.videoWidth}x${el.videoHeight}`;
    }

    const payload = {
      url: url,
      title: title,
      pageUrl: window.location.href,
      pageTitle: document.title,
      type: isHls ? "HLS (.m3u8)" : isDash ? "DASH (.mpd)" : isAudio ? "AUDIO" : "VIDEO",
      resolution: resolution,
      timestamp: Date.now()
    };

    try {
      chrome.runtime.sendMessage({
        type: "FIREBALL_MEDIA_DETECTED",
        media: payload
      });
    } catch (e) {
      // Ignore if background worker context is inactive
    }
  }

  // Initial Scan
  scanMediaElements();

  // MutationObserver for dynamically injected players (React, YouTube, Vue, etc.)
  const observer = new MutationObserver(() => {
    scanMediaElements();
  });

  observer.observe(document.documentElement, {
    childList: true,
    subtree: true,
    attributes: true,
    attributeFilter: ["src"]
  });

  // Periodic fallback check
  setInterval(scanMediaElements, 3000);
})();
