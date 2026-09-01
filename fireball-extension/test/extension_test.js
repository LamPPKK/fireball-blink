import assert from "assert";
import { generateBip39Words, validateBip39Words, deriveKeyFromSyncWords, encryptSyncPayload, decryptSyncPayload, createEncryptedBackup, decryptBackup } from "../lib/crypto_helper.js";
import { exportBookmarksToNetscapeHtml, parseNetscapeHtml } from "../lib/netscape_html.js";

// Node.js Web Crypto polyfill / global check
if (typeof crypto === "undefined" || !crypto.subtle) {
  const { webcrypto } = await import("crypto");
  globalThis.crypto = webcrypto;
}

// polyfill btoa / atob for Node environment
if (typeof btoa === "undefined") {
  globalThis.btoa = (str) => Buffer.from(str, "binary").toString("base64");
  globalThis.atob = (b64) => Buffer.from(b64, "base64").toString("binary");
}

console.log("🚀 Starting Fireball Extension Unit Tests...\n");

// Test 1: BIP-39 Words Generation & Validation
console.log("🧪 Test 1: BIP-39 Words Generation & Validation");
const words24 = generateBip39Words(24);
assert.strictEqual(words24.length, 24, "Should generate exactly 24 words");
const wordsStr = words24.join(" ");
assert.ok(validateBip39Words(wordsStr), "Valid 24 words string should pass validation");
assert.strictEqual(validateBip39Words("hello world"), false, "Short word string should fail validation");
console.log("✅ Passed: BIP-39 words generation and validation");

// Test 2: AES-256-GCM Brave Sync Packet Round-Trip
console.log("\n🧪 Test 2: Brave Sync Packet AES-GCM Encrypt & Decrypt");
const samplePayload = {
  protocol: "brave_sync_v2",
  timestamp: Date.now(),
  deviceName: "MacBook Pro M3",
  bookmarks: [
    { id: "bm-1", title: "GitHub", url: "https://github.com" },
    { id: "bm-2", title: "DuckDuckGo", url: "https://duckduckgo.com" }
  ],
  tabs: [
    { id: "tab-1", title: "Fireball Blink Engine", url: "https://github.com/LamPPKK/fireball-blink", deviceName: "MacBook Pro M3" }
  ]
};

const cipherBase64 = await encryptSyncPayload(samplePayload, words24);
assert.ok(typeof cipherBase64 === "string" && cipherBase64.length > 20, "Ciphertext should be non-empty base64");

const decrypted = await decryptSyncPayload(cipherBase64, words24);
assert.strictEqual(decrypted.protocol, "brave_sync_v2");
assert.strictEqual(decrypted.deviceName, "MacBook Pro M3");
assert.strictEqual(decrypted.bookmarks.length, 2);
assert.strictEqual(decrypted.tabs.length, 1);
assert.strictEqual(decrypted.tabs[0].url, "https://github.com/LamPPKK/fireball-blink");
console.log("✅ Passed: Brave Sync AES-GCM packet encryption and decryption");

// Test 3: E2EE Backup (PBKDF2 + AES-256-GCM)
console.log("\n🧪 Test 3: E2EE Encrypted Backup (PBKDF2 + AES-256-GCM)");
const backupData = {
  version: 1,
  spaces: [{ id: "space-main", name: "Main Space" }, { id: "space-work", name: "Work Space" }],
  bookmarks: [{ title: "Hacker News", url: "https://news.ycombinator.com" }]
};
const password = "super_secure_fireball_password_2026";
const encryptedBackup = await createEncryptedBackup(backupData, password);

assert.strictEqual(encryptedBackup.cipher, "AES-256-GCM");
assert.strictEqual(encryptedBackup.kdf, "PBKDF2-HMAC-SHA256");
assert.ok(encryptedBackup.payloadBase64.length > 0);

const restored = await decryptBackup(encryptedBackup, password);
assert.strictEqual(restored.spaces.length, 2);
assert.strictEqual(restored.bookmarks[0].title, "Hacker News");
console.log("✅ Passed: E2EE Backup creation and restoration");

// Test 4: Netscape HTML Bookmark Export & Parsing
console.log("\n🧪 Test 4: Netscape HTML Bookmark Export & Parsing");
const bookmarksToExport = [
  { id: "1", title: "Google", url: "https://google.com" },
  { id: "2", title: "Fireball Browser", url: "https://fireball.dev" }
];
const htmlExport = exportBookmarksToNetscapeHtml(bookmarksToExport);
assert.ok(htmlExport.includes("<!DOCTYPE NETSCAPE-Bookmark-file-1>"));
assert.ok(htmlExport.includes("https://google.com"));
assert.ok(htmlExport.includes("https://fireball.dev"));

const parsed = parseNetscapeHtml(htmlExport);
assert.strictEqual(parsed.length, 2);
assert.strictEqual(parsed[0].url, "https://google.com");
assert.strictEqual(parsed[1].url, "https://fireball.dev");
// Test 5: Ruffle Flash Element Detection & Polyfill
console.log("\n🧪 Test 5: Ruffle Flash Element Detection & Polyfill");
const { isFlashElement } = await import("../content_scripts/ruffle_interceptor.js");

const mockFlashEmbed = {
  tagName: "EMBED",
  getAttribute: (name) => {
    if (name === "type") return "application/x-shockwave-flash";
    if (name === "src") return "https://example.com/games/strike-force-heroes.swf";
    return null;
  },
  querySelectorAll: () => []
};

const mockNonFlashEmbed = {
  tagName: "EMBED",
  getAttribute: (name) => {
    if (name === "type") return "video/mp4";
    if (name === "src") return "https://example.com/video.mp4";
    return null;
  },
  querySelectorAll: () => []
};

assert.strictEqual(isFlashElement(mockFlashEmbed), true, "Should identify .swf embed as Flash");
assert.strictEqual(isFlashElement(mockNonFlashEmbed), false, "Should not identify mp4 as Flash");
console.log("✅ Passed: Ruffle Flash detection and polyfill validation");

console.log("\n🎉 ALL 5 FIREBALL EXTENSION TEST SUITES PASSED SUCCESSFULLY!");

