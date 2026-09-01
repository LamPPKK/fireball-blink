import { StorageHelper } from "../lib/storage_helper.js";
import { FireballSyncClient } from "../lib/sync_client.js";

const syncClient = new FireballSyncClient();

// Media Sniffer In-Memory Cache (Tab ID -> Array of Media Items)
const tabMediaMap = new Map();

// Initialize Context Menus
chrome.runtime.onInstalled.addListener(() => {
  chrome.contextMenus.create({
    id: "fireball_teleport_tab",
    title: "🔥 Teleport Tab to Fireball Mini Android",
    contexts: ["page", "link"]
  });

  chrome.contextMenus.create({
    id: "fireball_clean_link",
    title: "🛡️ Copy Link (Strip Trackers & UTM)",
    contexts: ["link"]
  });
});

// Handle Context Menu Clicks
chrome.contextMenus.onClicked.addListener(async (info, tab) => {
  if (info.menuItemId === "fireball_teleport_tab") {
    const targetUrl = info.linkUrl || (tab && tab.url);
    const targetTitle = (tab && tab.title) || "Shared Tab";
    if (targetUrl) {
      await teleportTab(targetUrl, targetTitle);
    }
  } else if (info.menuItemId === "fireball_clean_link") {
    if (info.linkUrl) {
      const cleaned = cleanUrlTrackers(info.linkUrl);
      // Copy to clipboard
      if (tab && tab.id) {
        chrome.scripting.executeScript({
          target: { tabId: tab.id },
          func: (text) => navigator.clipboard.writeText(text),
          args: [cleaned]
        });
      }
    }
  }
});

// Handle Keyboard Shortcuts
chrome.commands.onCommand.addListener(async (command) => {
  if (command === "teleport_current_tab") {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    if (tab && tab.url) {
      await teleportTab(tab.url, tab.title || "Teleported Tab");
    }
  }
});

// Teleport Tab to Sync Storage
async function teleportTab(url, title) {
  const config = await syncClient.getSyncConfig();
  const remoteTabs = await StorageHelper.get("remote_tabs", []);

  const newTab = {
    id: "tab-" + Date.now(),
    title: title,
    url: cleanUrlTrackers(url),
    deviceName: syncClient.deviceName,
    timestamp: Date.now()
  };

  remoteTabs.unshift(newTab);
  await StorageHelper.set("remote_tabs", remoteTabs.slice(0, 100));

  // Trigger push sync if connected
  if (config.words) {
    try {
      await syncClient.pushSyncData();
    } catch (e) {
      console.warn("Could not push sync data immediately:", e);
    }
  }

  // Visual Notification
  chrome.action.setBadgeText({ text: "✓" });
  chrome.action.setBadgeBackgroundColor({ color: "#B8FF3D" });
  setTimeout(() => {
    chrome.action.setBadgeText({ text: "" });
  }, 2000);
}

// Media Sniffer: Listen for Messages from Content Scripts
chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message.type === "FIREBALL_MEDIA_DETECTED" && sender.tab) {
    const tabId = sender.tab.id;
    if (!tabMediaMap.has(tabId)) {
      tabMediaMap.set(tabId, []);
    }
    const list = tabMediaMap.get(tabId);
    if (!list.some(m => m.url === message.media.url)) {
      list.push(message.media);
      updateTabBadge(tabId, list.length);
    }
    sendResponse({ received: true, total: list.length });
  } else if (message.type === "GET_TAB_MEDIA") {
    const tabId = message.tabId;
    const media = tabMediaMap.get(tabId) || [];
    sendResponse({ media });
  } else if (message.type === "CLEAN_URL") {
    sendResponse({ cleanedUrl: cleanUrlTrackers(message.url) });
  }
  return true;
});

// Tab Clean up on Close
chrome.tabs.onRemoved.addListener((tabId) => {
  tabMediaMap.delete(tabId);
});

// Tab change badge update
chrome.tabs.onActivated.addListener(({ tabId }) => {
  const count = (tabMediaMap.get(tabId) || []).length;
  updateTabBadge(tabId, count);
});

function updateTabBadge(tabId, count) {
  if (count > 0) {
    chrome.action.setBadgeText({ tabId, text: count.toString() });
    chrome.action.setBadgeBackgroundColor({ tabId, color: "#FF4500" });
  } else {
    chrome.action.setBadgeText({ tabId, text: "" });
  }
}

// Strict URL Tracker Cleaner Helper
function cleanUrlTrackers(rawUrl) {
  if (!rawUrl || typeof rawUrl !== "string") return rawUrl;
  try {
    const url = new URL(rawUrl);
    const trackingParams = [
      "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
      "fbclid", "gclid", "gbraid", "wbraid", "mc_eid", "yclid", "_hsenc",
      "_hsmi", "mkt_tok", "igshid", "msclkid", "twclid", "si", "feature"
    ];
    trackingParams.forEach(param => url.searchParams.delete(param));
    return url.toString();
  } catch (e) {
    return rawUrl;
  }
}
