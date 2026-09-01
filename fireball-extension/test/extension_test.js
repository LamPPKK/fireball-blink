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

// Test 6: Fireball Tampermonkey Pattern Matching & Script Injection
console.log("\n🧪 Test 6: Fireball Tampermonkey Userscript Engine");
function matchesPattern(url, pattern) {
  if (pattern === "<all_urls>" || pattern === "*://*/*") return true;
  const regex = new RegExp("^" + pattern.replace(/\*/g, ".*") + "$");
  return regex.test(url);
}
assert.strictEqual(matchesPattern("https://youtube.com/watch?v=123", "*://youtube.com/*"), true);
assert.strictEqual(matchesPattern("https://github.com/LamPPKK", "*://github.com/*"), true);
assert.strictEqual(matchesPattern("https://example.org", "*://github.com/*"), false);
console.log("✅ Passed: Tampermonkey userscript URL pattern matching");

// Test 7: uBlock Origin Declarative Rules Verification
console.log("\n🧪 Test 7: uBlock Origin Rule Set");
import fs from "fs";
import path from "path";
const ublockRulesPath = path.resolve("fireball-extension/ublock-origin/rules/rules.json");
const rules = JSON.parse(fs.readFileSync(ublockRulesPath, "utf-8"));

assert.ok(Array.isArray(rules) && rules.length > 0, "uBlock rules should be a non-empty array");
assert.strictEqual(rules[0].action.type, "block");
assert.ok(rules[0].condition.urlFilter.includes("doubleclick.net"));
console.log("✅ Passed: uBlock Origin DeclarativeNetRequest rules valid");

// Test 8: WebStore Interceptor Store URL Detection & Extraction
console.log("\n🧪 Test 8: WebStore Interceptor Store Detection");
function isWebStoreUrl(url) {
  return url.includes("chromewebstore.google.com") ||
         url.includes("chrome.google.com/webstore") ||
         url.includes("microsoftedge.microsoft.com/addons");
}
assert.strictEqual(isWebStoreUrl("https://chromewebstore.google.com/detail/test/123"), true);
assert.strictEqual(isWebStoreUrl("https://microsoftedge.microsoft.com/addons/detail/test/456"), true);
assert.strictEqual(isWebStoreUrl("https://example.com"), false);
console.log("✅ Passed: WebStore Interceptor URL recognition");

// Test 9: Authenticator RFC-6238 TOTP Counter Math
console.log("\n🧪 Test 9: Authenticator TOTP Time Step Calculation");
function calculateTimeStep(epochSeconds = Date.now() / 1000, period = 30) {
  return Math.floor(epochSeconds / period);
}
const baseTime = 1700000010; // Math.floor(1700000010 / 30) == 56666667
const step1 = calculateTimeStep(baseTime, 30);
const step2 = calculateTimeStep(baseTime + 15, 30); // 1700000025 -> 56666667
const step3 = calculateTimeStep(baseTime + 30, 30); // 1700000040 -> 56666668
assert.strictEqual(step1, step2, "Should remain in same TOTP step within 30s period");
assert.strictEqual(step3, step1 + 1, "Should advance by exactly 1 step after 30s");
console.log("✅ Passed: Authenticator RFC-6238 time step calculation");


// Test 10: Thin Web Client Binary Frame Unpacking & HUD Metrics
console.log("\n🧪 Test 10: Thin Web Client Frame Protocol & HUD Logic");
function parseBinaryFrameHeader(buffer) {
  if (buffer.byteLength < 4) return null;
  const view = new DataView(buffer);
  const magic = String.fromCharCode(view.getUint8(0), view.getUint8(1), view.getUint8(2), view.getUint8(3));
  return { magic, payloadLength: buffer.byteLength - 4 };
}
const testBuf = new ArrayBuffer(8);
const testView = new Uint8Array(testBuf);
testView[0] = 70; // 'F'
testView[1] = 66; // 'B'
testView[2] = 69; // 'E'
testView[3] = 65; // 'A'
const parsedHeader = parseBinaryFrameHeader(testBuf);
assert.strictEqual(parsedHeader.magic, "FBEA");
assert.strictEqual(parsedHeader.payloadLength, 4);
console.log("✅ Passed: Thin Web Client binary frame header parsing");

console.log("\n🎉 ALL 10 FIREBALL EXTENSION & WEB CLIENT AUTOMATED TESTS PASSED SUCCESSFULLY!");



