import { encryptSyncPayload, decryptSyncPayload } from "./crypto_helper.js";
import { StorageHelper } from "./storage_helper.js";

export const SyncStatus = {
  DISCONNECTED: "DISCONNECTED",
  CONNECTING: "CONNECTING",
  SYNCED: "SYNCED",
  AUTH_ERROR: "AUTH_ERROR"
};

export const SyncProvider = {
  BRAVE_SYNC_CHAIN: "BRAVE_SYNC_CHAIN",
  FIREFOX_SYNC: "FIREFOX_SYNC"
};

export class FireballSyncClient {
  constructor() {
    this.deviceName = "Fireball Desktop Companion (" + (navigator.userAgent.includes("Mac") ? "macOS" : "Windows/Linux") + ")";
  }

  async getSyncConfig() {
    return await StorageHelper.get("sync_config", {
      status: SyncStatus.DISCONNECTED,
      provider: SyncProvider.BRAVE_SYNC_CHAIN,
      words: null,
      fxaEmail: null,
      lastSyncedTimestamp: null,
      syncBookmarks: true,
      syncHistory: true,
      syncTabs: true
    });
  }

  async setSyncConfig(config) {
    await StorageHelper.set("sync_config", config);
  }

  async connectBraveSync(words) {
    const config = await this.getSyncConfig();
    config.provider = SyncProvider.BRAVE_SYNC_CHAIN;
    config.words = Array.isArray(words) ? words : words.trim().split(/\s+/);
    config.status = SyncStatus.SYNCED;
    config.lastSyncedTimestamp = Date.now();
    await this.setSyncConfig(config);
    return config;
  }

  async disconnect() {
    await StorageHelper.remove("sync_config");
    await StorageHelper.remove("remote_tabs");
  }

  async buildLocalSyncPayload() {
    // Collect local bookmarks
    let bookmarks = [];
    try {
      const tree = await new Promise(r => chrome.bookmarks.getTree(r));
      bookmarks = flattenBookmarks(tree);
    } catch (e) {
      console.warn("Could not read bookmarks:", e);
    }

    // Collect local open tabs
    let tabs = [];
    try {
      const chromeTabs = await new Promise(r => chrome.tabs.query({}, r));
      tabs = chromeTabs
        .filter(t => t.url && !t.url.startsWith("chrome://") && !t.url.startsWith("about:"))
        .map(t => ({
          id: "tab-" + t.id,
          title: t.title || "Untitled",
          url: t.url,
          deviceName: this.deviceName,
          timestamp: Date.now()
        }));
    } catch (e) {
      console.warn("Could not read tabs:", e);
    }

    return {
      protocol: "brave_sync_v2",
      timestamp: Date.now(),
      deviceName: this.deviceName,
      bookmarks: bookmarks.slice(0, 500),
      history: [],
      tabs: tabs
    };
  }

  async pushSyncData() {
    const config = await this.getSyncConfig();
    if (config.status !== SyncStatus.SYNCED || !config.words) {
      throw new Error("Sync not connected");
    }

    const payload = await this.buildLocalSyncPayload();
    const encryptedPacket = await encryptSyncPayload(payload, config.words);

    // Save encrypted sync snapshot
    await StorageHelper.set("last_encrypted_packet", encryptedPacket);
    config.lastSyncedTimestamp = Date.now();
    await this.setSyncConfig(config);

    return {
      success: true,
      timestamp: config.lastSyncedTimestamp,
      tabCount: payload.tabs.length,
      bookmarkCount: payload.bookmarks.length
    };
  }

  async mergeRemoteSyncPacket(encryptedBase64) {
    const config = await this.getSyncConfig();
    if (!config.words) throw new Error("Missing sync words");

    const payload = await decryptSyncPayload(encryptedBase64, config.words);

    // Store remote tabs
    if (payload.tabs && Array.isArray(payload.tabs)) {
      await StorageHelper.set("remote_tabs", payload.tabs);
    }

    return payload;
  }

  async getRemoteTabs() {
    return await StorageHelper.get("remote_tabs", []);
  }
}

function flattenBookmarks(nodes) {
  const result = [];
  function traverse(list) {
    for (const node of list) {
      if (node.url) {
        result.push({
          id: node.id,
          title: node.title,
          url: node.url,
          dateAdded: node.dateAdded
        });
      }
      if (node.children) {
        traverse(node.children);
      }
    }
  }
  traverse(nodes);
  return result;
}
