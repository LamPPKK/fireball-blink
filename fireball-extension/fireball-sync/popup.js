import { generateBip39Words, validateBip39Words, createEncryptedBackup } from "../lib/crypto_helper.js";
import { FireballSyncClient, SyncStatus } from "../lib/sync_client.js";
import { exportBookmarksToNetscapeHtml } from "../lib/netscape_html.js";
import { StorageHelper } from "../lib/storage_helper.js";

const syncClient = new FireballSyncClient();
let generatedWords = null;
let currentTab = null;

document.addEventListener("DOMContentLoaded", async () => {
  setupTabNavigation();
  await initActiveTabInfo();
  await refreshSyncUi();
  await refreshSpacesAndTabs();
  await refreshMediaSniffer();
  setupBackupAndShields();
});

// 1. Navigation Tab Switching
function setupTabNavigation() {
  const tabs = document.querySelectorAll(".nav-tab");
  tabs.forEach((tab) => {
    tab.addEventListener("click", () => {
      tabs.forEach((t) => t.classList.remove("active"));
      document.querySelectorAll(".tab-panel").forEach((p) => p.classList.remove("active"));

      tab.classList.add("active");
      const panelId = tab.getAttribute("data-tab");
      const targetPanel = document.getElementById(panelId);
      if (targetPanel) {
        targetPanel.classList.add("active");
      }
    });
  });
}

// 2. Active Tab Info
async function initActiveTabInfo() {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  currentTab = tab;
  const titleEl = document.getElementById("currentTabTitle");
  if (titleEl && tab) {
    titleEl.textContent = tab.title || tab.url || "Untitled Tab";
  }

  document.getElementById("btnTeleportCurrent")?.addEventListener("click", async () => {
    if (currentTab && currentTab.url) {
      await teleportSingleTab(currentTab.url, currentTab.title);
    }
  });
}

// 3. Sync UI & Actions
async function refreshSyncUi() {
  const config = await syncClient.getSyncConfig();
  const disconnectedBox = document.getElementById("syncDisconnectedState");
  const connectedBox = document.getElementById("syncConnectedState");
  const statusPill = document.getElementById("syncStatusPill");
  const statusText = document.getElementById("syncStatusText");

  if (config.status === SyncStatus.SYNCED && config.words) {
    disconnectedBox?.classList.add("hidden");
    connectedBox?.classList.remove("hidden");
    statusPill?.classList.add("active");
    if (statusText) statusText.textContent = "Synced";

    const lastSyncedLabel = document.getElementById("lastSyncedLabel");
    if (lastSyncedLabel && config.lastSyncedTimestamp) {
      const date = new Date(config.lastSyncedTimestamp);
      lastSyncedLabel.textContent = `Lần đồng bộ gần nhất: ${date.toLocaleTimeString()}`;
    }

    await renderRemoteTabs();
  } else {
    disconnectedBox?.classList.remove("hidden");
    connectedBox?.classList.add("hidden");
    statusPill?.classList.remove("active");
    if (statusText) statusText.textContent = "Local";
  }

  // Setup Button Handlers
  document.getElementById("btnGenerateWords")?.addEventListener("click", () => {
    generatedWords = generateBip39Words(24);
    renderWordsGrid(generatedWords);
    document.getElementById("wordsDisplayBox")?.classList.remove("hidden");
    document.getElementById("wordsInputBox")?.classList.add("hidden");
  });

  document.getElementById("btnEnterWords")?.addEventListener("click", () => {
    document.getElementById("wordsInputBox")?.classList.remove("hidden");
    document.getElementById("wordsDisplayBox")?.classList.add("hidden");
  });

  document.getElementById("btnCopyWords")?.addEventListener("click", () => {
    if (generatedWords) {
      navigator.clipboard.writeText(generatedWords.join(" "));
      showToast("Đã sao chép 24 từ!");
    }
  });

  document.getElementById("btnConfirmConnect")?.addEventListener("click", async () => {
    if (generatedWords) {
      await syncClient.connectBraveSync(generatedWords);
      await syncClient.pushSyncData();
      showToast("Kết nối Brave Sync thành công!");
      await refreshSyncUi();
    }
  });

  document.getElementById("btnConnectExisting")?.addEventListener("click", async () => {
    const inputArea = document.getElementById("inputWordsArea");
    const val = inputArea ? inputArea.value : "";
    if (validateBip39Words(val)) {
      await syncClient.connectBraveSync(val);
      await syncClient.pushSyncData();
      showToast("Đã kết nối chuỗi Sync!");
      await refreshSyncUi();
    } else {
      alert("Chuỗi từ không hợp lệ. Vui lòng nhập đủ 24 từ cách nhau bởi dấu cách.");
    }
  });

  document.getElementById("btnPushSync")?.addEventListener("click", async () => {
    try {
      const result = await syncClient.pushSyncData();
      showToast(`Đã đồng bộ ${result.tabCount} tabs & ${result.bookmarkCount} bookmarks!`);
      await refreshSyncUi();
    } catch (e) {
      alert("Lỗi đồng bộ: " + e.message);
    }
  });

  document.getElementById("btnDisconnectSync")?.addEventListener("click", async () => {
    if (confirm("Bạn có chắc muốn ngắt kết nối đồng bộ trên thiết bị này?")) {
      await syncClient.disconnect();
      showToast("Đã ngắt kết nối.");
      await refreshSyncUi();
    }
  });
}

function renderWordsGrid(words) {
  const grid = document.getElementById("wordsGrid");
  if (!grid) return;
  grid.innerHTML = "";
  words.forEach((w, i) => {
    const chip = document.createElement("div");
    chip.className = "word-chip";
    chip.innerHTML = `<span class="word-idx">${i + 1}.</span>${w}`;
    grid.appendChild(chip);
  });
}

async function renderRemoteTabs() {
  const remoteTabs = await syncClient.getRemoteTabs();
  const container = document.getElementById("remoteTabsList");
  const countBadge = document.getElementById("remoteTabCount");

  if (countBadge) countBadge.textContent = `${remoteTabs.length} tabs`;
  if (!container) return;

  if (remoteTabs.length === 0) {
    container.innerHTML = `<div class="empty-state">Chưa có tab nào từ điện thoại Fireball Mini.</div>`;
    return;
  }

  container.innerHTML = "";
  remoteTabs.forEach((tab) => {
    const item = document.createElement("div");
    item.className = "list-item";
    item.innerHTML = `
      <div class="item-info">
        <div class="item-title">${escapeHtml(tab.title || "Remote Tab")}</div>
        <div class="item-sub">${escapeHtml(tab.url || "")} · <span class="text-lime">${escapeHtml(tab.deviceName || "Mobile")}</span></div>
      </div>
      <button class="btn btn-small btn-outline">Mở</button>
    `;
    item.querySelector("button")?.addEventListener("click", () => {
      chrome.tabs.create({ url: tab.url });
    });
    container.appendChild(item);
  });
}

// 4. Spaces & Tabs Panel
async function refreshSpacesAndTabs() {
  const allTabs = await chrome.tabs.query({ currentWindow: true });
  const validTabs = allTabs.filter(t => t.url && !t.url.startsWith("chrome://"));
  const container = document.getElementById("localTabsList");
  const mainCount = document.getElementById("mainSpaceTabCount");

  if (mainCount) mainCount.textContent = `${validTabs.length} tabs`;
  if (!container) return;

  container.innerHTML = "";
  validTabs.forEach((tab) => {
    const item = document.createElement("div");
    item.className = "list-item";
    item.innerHTML = `
      <div class="item-info">
        <div class="item-title">${escapeHtml(tab.title || "Tab")}</div>
        <div class="item-sub">${escapeHtml(tab.url || "")}</div>
      </div>
      <button class="btn btn-small btn-primary" title="Teleport to Mobile">🚀 Gửi</button>
    `;
    item.querySelector("button")?.addEventListener("click", async () => {
      await teleportSingleTab(tab.url, tab.title);
    });
    container.appendChild(item);
  });
}

async function teleportSingleTab(url, title) {
  const remoteTabs = await StorageHelper.get("remote_tabs", []);
  remoteTabs.unshift({
    id: "tab-" + Date.now(),
    title: title || "Shared Tab",
    url: url,
    deviceName: syncClient.deviceName,
    timestamp: Date.now()
  });
  await StorageHelper.set("remote_tabs", remoteTabs.slice(0, 100));

  const config = await syncClient.getSyncConfig();
  if (config.words) {
    try {
      await syncClient.pushSyncData();
    } catch (e) {
      console.warn(e);
    }
  }

  showToast("🚀 Đã teleport tab tới Fireball Mini!");
}

// 5. Media Sniffer Panel
async function refreshMediaSniffer() {
  if (!currentTab || !currentTab.id) return;

  chrome.runtime.sendMessage({ type: "GET_TAB_MEDIA", tabId: currentTab.id }, (response) => {
    const mediaList = (response && response.media) || [];
    const badgeEl = document.getElementById("mediaCountBadge");
    const countBadge = document.getElementById("snifferCountBadge");
    const emptyState = document.getElementById("snifferEmptyState");
    const container = document.getElementById("mediaListContainer");

    if (badgeEl) badgeEl.textContent = mediaList.length.toString();
    if (countBadge) countBadge.textContent = mediaList.length.toString();

    if (mediaList.length === 0) {
      emptyState?.classList.remove("hidden");
      if (container) container.innerHTML = "";
      return;
    }

    emptyState?.classList.add("hidden");
    if (!container) return;
    container.innerHTML = "";

    mediaList.forEach((media, idx) => {
      const item = document.createElement("div");
      item.className = "list-item";
      item.innerHTML = `
        <div class="item-info">
          <div class="item-title">${escapeHtml(media.title || "Media Stream " + (idx + 1))}</div>
          <div class="item-sub"><span class="text-lime">${escapeHtml(media.type)}</span> ${media.resolution ? "· " + media.resolution : ""} · ${escapeHtml(media.url.substring(0, 45))}...</div>
        </div>
        <div style="display: flex; gap: 4px;">
          <button class="btn btn-small btn-outline btn-copy" title="Copy Link">📋</button>
          <button class="btn btn-small btn-primary btn-dl" title="Download">⬇️</button>
        </div>
      `;

      item.querySelector(".btn-copy")?.addEventListener("click", () => {
        navigator.clipboard.writeText(media.url);
        showToast("Đã sao chép URL luồng!");
      });

      item.querySelector(".btn-dl")?.addEventListener("click", () => {
        chrome.downloads.download({ url: media.url, filename: `fireball_media_${Date.now()}.${media.type.includes("HLS") ? "m3u8" : "mp4"}` });
        showToast("Đang bắt đầu tải...");
      });

      container.appendChild(item);
    });
  });
}

// 6. Backup & Shields Panel
function setupBackupAndShields() {
  document.getElementById("btnExportHtmlBookmarks")?.addEventListener("click", async () => {
    try {
      const tree = await new Promise(r => chrome.bookmarks.getTree(r));
      const bookmarks = [];
      function traverse(nodes) {
        for (const n of nodes) {
          if (n.url) bookmarks.push(n);
          if (n.children) traverse(n.children);
        }
      }
      traverse(tree);

      const html = exportBookmarksToNetscapeHtml(bookmarks);
      const blob = new Blob([html], { type: "text/html" });
      const url = URL.createObjectURL(blob);
      chrome.downloads.download({ url: url, filename: `fireball_bookmarks_${Date.now()}.html` });
      showToast("Đã xuất file HTML Bookmarks!");
    } catch (e) {
      alert("Lỗi xuất dấu trang: " + e.message);
    }
  });

  document.getElementById("btnCreateEncryptedBackup")?.addEventListener("click", async () => {
    const pwInput = document.getElementById("backupPasswordInput");
    const password = pwInput ? pwInput.value : "";
    if (!password || password.length < 4) {
      alert("Vui lòng nhập mật khẩu tối thiểu 4 ký tự.");
      return;
    }

    try {
      const tree = await new Promise(r => chrome.bookmarks.getTree(r));
      const chromeTabs = await chrome.tabs.query({ currentWindow: true });
      const backupData = {
        app: "Fireball Companion",
        timestamp: Date.now(),
        bookmarks: tree,
        tabs: chromeTabs.map(t => ({ title: t.title, url: t.url }))
      };

      const encrypted = await createEncryptedBackup(backupData, password);
      const blob = new Blob([JSON.stringify(encrypted, null, 2)], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      chrome.downloads.download({ url: url, filename: `fireball_backup_${Date.now()}.fireball` });
      showToast("Đã tạo file sao lưu AES-256!");
      if (pwInput) pwInput.value = "";
    } catch (e) {
      alert("Lỗi tạo sao lưu: " + e.message);
    }
  });
}

function showToast(msg) {
  const toast = document.getElementById("toastMessage");
  if (toast) {
    toast.textContent = msg;
    setTimeout(() => {
      toast.textContent = "";
    }, 2500);
  }
}

function escapeHtml(str) {
  if (!str) return "";
  return str
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#039;");
}
