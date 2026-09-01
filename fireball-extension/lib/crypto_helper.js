import { BIP39_WORDLIST } from "./bip39_wordlist.js";

/**
 * Fireball Web Crypto Helper (AES-256-GCM, SHA-256, PBKDF2)
 * Fully compatible with Fireball Mini Android (BraveSyncHelper.kt & EncryptedBackupManager.kt)
 */

export function generateBip39Words(count = 24) {
  const words = [];
  const randomBytes = new Uint32Array(count);
  crypto.getRandomValues(randomBytes);
  const wordlistLength = BIP39_WORDLIST.length;

  for (let i = 0; i < count; i++) {
    const index = randomBytes[i] % wordlistLength;
    words.push(BIP39_WORDLIST[index]);
  }
  return words;
}

export function validateBip39Words(wordsStr) {
  if (!wordsStr || typeof wordsStr !== "string") return false;
  const words = wordsStr.trim().split(/\s+/).filter(Boolean);
  return words.length >= 24 && words.length <= 25;
}

export async function deriveKeyFromSyncWords(words) {
  const mnemonicStr = Array.isArray(words) ? words.join(" ") : words.trim();
  const encoder = new TextEncoder();
  const keyMaterial = encoder.encode(mnemonicStr);

  const hashBuffer = await crypto.subtle.digest("SHA-256", keyMaterial);
  return await crypto.subtle.importKey(
    "raw",
    hashBuffer,
    { name: "AES-GCM" },
    false,
    ["encrypt", "decrypt"]
  );
}

export async function encryptSyncPayload(payloadObj, words) {
  const key = await deriveKeyFromSyncWords(words);
  const encoder = new TextEncoder();
  const rawBytes = encoder.encode(JSON.stringify(payloadObj));

  const iv = new Uint8Array(12);
  crypto.getRandomValues(iv);

  const cipherBuffer = await crypto.subtle.encrypt(
    { name: "AES-GCM", iv: iv },
    key,
    rawBytes
  );

  const combined = new Uint8Array(iv.byteLength + cipherBuffer.byteLength);
  combined.set(iv, 0);
  combined.set(new Uint8Array(cipherBuffer), iv.byteLength);

  return uint8ArrayToBase64(combined);
}

export async function decryptSyncPayload(cipherBase64, words) {
  const key = await deriveKeyFromSyncWords(words);
  const combined = base64ToUint8Array(cipherBase64);

  if (combined.byteLength < 13) {
    throw new Error("Ciphertext too short (missing IV)");
  }

  const iv = combined.slice(0, 12);
  const ciphertext = combined.slice(12);

  const decryptedBuffer = await crypto.subtle.decrypt(
    { name: "AES-GCM", iv: iv },
    key,
    ciphertext
  );

  const decoder = new TextDecoder();
  const jsonStr = decoder.decode(decryptedBuffer);
  return JSON.parse(jsonStr);
}

export async function createEncryptedBackup(backupData, password) {
  const encoder = new TextEncoder();
  const salt = new Uint8Array(16);
  crypto.getRandomValues(salt);

  const iv = new Uint8Array(12);
  crypto.getRandomValues(iv);

  const pwKeyMaterial = await crypto.subtle.importKey(
    "raw",
    encoder.encode(password),
    { name: "PBKDF2" },
    false,
    ["deriveKey"]
  );

  const derivedKey = await crypto.subtle.deriveKey(
    {
      name: "PBKDF2",
      salt: salt,
      iterations: 100000,
      hash: "SHA-256"
    },
    pwKeyMaterial,
    { name: "AES-GCM", length: 256 },
    false,
    ["encrypt"]
  );

  const cipherBuffer = await crypto.subtle.encrypt(
    { name: "AES-GCM", iv: iv },
    derivedKey,
    encoder.encode(JSON.stringify(backupData))
  );

  return {
    version: 1,
    cipher: "AES-256-GCM",
    kdf: "PBKDF2-HMAC-SHA256",
    iterations: 100000,
    saltBase64: uint8ArrayToBase64(salt),
    ivBase64: uint8ArrayToBase64(iv),
    payloadBase64: uint8ArrayToBase64(new Uint8Array(cipherBuffer)),
    timestamp: Date.now()
  };
}

export async function decryptBackup(backupObj, password) {
  const encoder = new TextEncoder();
  const salt = base64ToUint8Array(backupObj.saltBase64);
  const iv = base64ToUint8Array(backupObj.ivBase64);
  const ciphertext = base64ToUint8Array(backupObj.payloadBase64);

  const pwKeyMaterial = await crypto.subtle.importKey(
    "raw",
    encoder.encode(password),
    { name: "PBKDF2" },
    false,
    ["deriveKey"]
  );

  const derivedKey = await crypto.subtle.deriveKey(
    {
      name: "PBKDF2",
      salt: salt,
      iterations: backupObj.iterations || 100000,
      hash: "SHA-256"
    },
    pwKeyMaterial,
    { name: "AES-GCM", length: 256 },
    false,
    ["decrypt"]
  );

  const decryptedBuffer = await crypto.subtle.decrypt(
    { name: "AES-GCM", iv: iv },
    derivedKey,
    ciphertext
  );

  const decoder = new TextDecoder();
  return JSON.parse(decoder.decode(decryptedBuffer));
}

// Binary Utilities
function uint8ArrayToBase64(bytes) {
  let binary = "";
  const len = bytes.byteLength;
  for (let i = 0; i < len; i++) {
    binary += String.fromCharCode(bytes[i]);
  }
  return btoa(binary);
}

function base64ToUint8Array(base64) {
  const binaryString = atob(base64);
  const len = binaryString.length;
  const bytes = new Uint8Array(len);
  for (let i = 0; i < len; i++) {
    bytes[i] = binaryString.charCodeAt(i);
  }
  return bytes;
}
