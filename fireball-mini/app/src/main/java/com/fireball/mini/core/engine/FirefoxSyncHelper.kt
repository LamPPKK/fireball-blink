package com.fireball.mini.core.engine

import android.util.Base64
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.HistoryItem
import com.fireball.mini.core.models.RemoteTab
import com.fireball.mini.core.models.TabItem
import org.json.JSONArray
import org.json.JSONObject
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.security.SecureRandom
import java.util.UUID
import javax.crypto.Cipher
import javax.crypto.spec.IvParameterSpec
import javax.crypto.spec.SecretKeySpec

object FirefoxSyncHelper {

    /**
     * Derives a 128-bit or 256-bit encryption key from a Firefox Sync Key string.
     */
    fun deriveKeyFromSyncKey(syncKey: String): SecretKeySpec {
        val sha256 = MessageDigest.getInstance("SHA-256")
        val keyBytes = sha256.digest(syncKey.toByteArray(StandardCharsets.UTF_8))
        return SecretKeySpec(keyBytes, "AES")
    }

    /**
     * Builds Firefox Sync 1.5 BSO (Basic Storage Object) collection for bookmarks.
     */
    fun buildBookmarksCollectionBso(bookmarks: List<BookmarkItem>, syncKey: String): String {
        val bsoArray = JSONArray()
        val secretKey = deriveKeyFromSyncKey(syncKey)

        for (b in bookmarks) {
            val record = JSONObject()
            record.put("id", b.id)
            record.put("type", "bookmark")
            record.put("title", b.title)
            record.put("bmkUri", b.url)
            record.put("parentid", "menu")
            record.put("dateAdded", b.createdAtTimestamp)

            val ciphertext = encryptBsoPayload(record.toString(), secretKey)
            val bsoObj = JSONObject()
            bsoObj.put("id", b.id)
            bsoObj.put("modified", b.createdAtTimestamp / 1000)
            bsoObj.put("payload", ciphertext)
            bsoArray.put(bsoObj)
        }

        return bsoArray.toString(2)
    }

    /**
     * Builds Firefox Sync 1.5 BSO collection for Open Tabs.
     */
    fun buildTabsCollectionBso(tabs: List<TabItem>, clientName: String, syncKey: String): String {
        val root = JSONObject()
        root.put("id", UUID.randomUUID().toString())
        root.put("clientName", clientName)

        val tabsArray = JSONArray()
        for (t in tabs) {
            val tabObj = JSONObject()
            tabObj.put("title", t.title)
            val urlHistory = JSONArray()
            urlHistory.put(t.url)
            tabObj.put("urlHistory", urlHistory)
            tabObj.put("lastUsed", t.lastAccessedTimestampMs / 1000)
            tabsArray.put(tabObj)
        }
        root.put("tabs", tabsArray)

        val secretKey = deriveKeyFromSyncKey(syncKey)
        val ciphertext = encryptBsoPayload(root.toString(), secretKey)

        val bso = JSONObject()
        bso.put("id", "tabs_client_" + clientName.replace(" ", "_"))
        bso.put("modified", System.currentTimeMillis() / 1000)
        bso.put("payload", ciphertext)

        return JSONArray().put(bso).toString(2)
    }

    /**
     * Parses incoming Firefox Sync BSO JSON array.
     */
    fun parseBookmarksFromBso(bsoArrayJson: String, syncKey: String): List<BookmarkItem> {
        val bookmarks = mutableListOf<BookmarkItem>()
        val secretKey = deriveKeyFromSyncKey(syncKey)

        try {
            val array = JSONArray(bsoArrayJson)
            for (i in 0 until array.length()) {
                val bso = array.getJSONObject(i)
                val encryptedPayload = bso.getString("payload")
                val decryptedJsonStr = decryptBsoPayload(encryptedPayload, secretKey) ?: continue
                val record = JSONObject(decryptedJsonStr)

                if (record.optString("type") == "bookmark" || record.has("bmkUri")) {
                    val url = record.optString("bmkUri", record.optString("url"))
                    val title = record.optString("title", url)
                    val dateAdded = record.optLong("dateAdded", System.currentTimeMillis())

                    if (url.startsWith("http://") || url.startsWith("https://")) {
                        bookmarks.add(
                            BookmarkItem(
                                id = record.optString("id", UUID.randomUUID().toString()),
                                url = url,
                                title = title,
                                createdAtTimestamp = dateAdded
                            )
                        )
                    }
                }
            }
        } catch (_: Exception) {
            // Ignored
        }
        return bookmarks
    }

    /**
     * Parses remote open tabs from Firefox Sync tabs BSO.
     */
    fun parseTabsFromBso(tabsBsoJson: String, syncKey: String): List<RemoteTab> {
        val remoteTabs = mutableListOf<RemoteTab>()
        val secretKey = deriveKeyFromSyncKey(syncKey)

        try {
            val array = JSONArray(tabsBsoJson)
            for (i in 0 until array.length()) {
                val bso = array.getJSONObject(i)
                val encryptedPayload = bso.getString("payload")
                val decryptedJsonStr = decryptBsoPayload(encryptedPayload, secretKey) ?: continue
                val clientObj = JSONObject(decryptedJsonStr)
                val deviceName = clientObj.optString("clientName", "Firefox Device")

                val tabsArray = clientObj.optJSONArray("tabs")
                if (tabsArray != null) {
                    for (j in 0 until tabsArray.length()) {
                        val t = tabsArray.getJSONObject(j)
                        val title = t.optString("title", "Firefox Tab")
                        val urlHistory = t.optJSONArray("urlHistory")
                        val url = urlHistory?.optString(0) ?: ""
                        val lastUsed = t.optLong("lastUsed", System.currentTimeMillis() / 1000) * 1000

                        if (url.isNotBlank()) {
                            remoteTabs.add(
                                RemoteTab(
                                    id = UUID.randomUUID().toString(),
                                    title = title,
                                    url = url,
                                    deviceName = deviceName,
                                    timestampMs = lastUsed
                                )
                            )
                        }
                    }
                }
            }
        } catch (_: Exception) {
            // Ignored
        }
        return remoteTabs
    }

    private fun encryptBsoPayload(plainJson: String, secretKey: SecretKeySpec): String {
        val iv = ByteArray(16).apply { SecureRandom().nextBytes(this) }
        val cipher = Cipher.getInstance("AES/CBC/PKCS5Padding")
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, IvParameterSpec(iv))
        val cipherBytes = cipher.doFinal(plainJson.toByteArray(StandardCharsets.UTF_8))

        val wrapper = JSONObject()
        wrapper.put("IV", safeBase64Encode(iv))
        wrapper.put("ciphertext", safeBase64Encode(cipherBytes))
        return wrapper.toString()
    }

    private fun decryptBsoPayload(wrapperJson: String, secretKey: SecretKeySpec): String? {
        return try {
            val wrapper = JSONObject(wrapperJson)
            val iv = safeBase64Decode(wrapper.getString("IV"))
            val cipherBytes = safeBase64Decode(wrapper.getString("ciphertext"))

            val cipher = Cipher.getInstance("AES/CBC/PKCS5Padding")
            cipher.init(Cipher.DECRYPT_MODE, secretKey, IvParameterSpec(iv))
            val plainBytes = cipher.doFinal(cipherBytes)
            String(plainBytes, StandardCharsets.UTF_8)
        } catch (_: Exception) {
            null
        }
    }

    private fun safeBase64Encode(data: ByteArray): String {
        return try {
            Base64.encodeToString(data, Base64.NO_WRAP)
        } catch (_: Throwable) {
            java.util.Base64.getEncoder().encodeToString(data)
        }
    }

    private fun safeBase64Decode(str: String): ByteArray {
        return try {
            Base64.decode(str, Base64.DEFAULT)
        } catch (_: Throwable) {
            java.util.Base64.getDecoder().decode(str)
        }
    }
}
