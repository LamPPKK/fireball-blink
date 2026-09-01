package com.fireball.mini.core.engine

import android.util.Base64
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.FireballBackupData
import com.fireball.mini.core.models.HistoryItem
import com.fireball.mini.core.models.Space
import org.json.JSONArray
import org.json.JSONObject
import java.nio.charset.StandardCharsets
import java.security.SecureRandom
import java.util.UUID
import javax.crypto.Cipher
import javax.crypto.SecretKeyFactory
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.PBEKeySpec
import javax.crypto.spec.SecretKeySpec

object EncryptedBackupManager {

    private const val GCM_TAG_LENGTH = 128
    private const val GCM_IV_LENGTH = 12
    private const val SALT_LENGTH = 16
    private const val ITERATION_COUNT = 10000
    private const val KEY_LENGTH = 256

    /**
     * Exports bookmarks into standard Netscape Bookmark HTML format.
     * Compatible with Chrome, Firefox, Safari, Edge, Brave.
     */
    fun exportBookmarksToNetscapeHtml(bookmarks: List<BookmarkItem>): String {
        val sb = StringBuilder()
        sb.append("<!DOCTYPE NETSCAPE-Bookmark-file-1>\n")
        sb.append("<!-- This is an automatically generated file. -->\n")
        sb.append("<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n")
        sb.append("<TITLE>Bookmarks</TITLE>\n")
        sb.append("<H1>Fireball Bookmarks</H1>\n")
        sb.append("<DL><p>\n")

        for (b in bookmarks) {
            val addDate = b.createdAtTimestamp / 1000
            val safeTitle = escapeHtml(b.title)
            val safeUrl = escapeHtml(b.url)
            sb.append("    <DT><A HREF=\"$safeUrl\" ADD_DATE=\"$addDate\">$safeTitle</A>\n")
        }

        sb.append("</DL><p>\n")
        return sb.toString()
    }

    /**
     * Imports bookmarks from a Netscape Bookmark HTML file.
     */
    fun importBookmarksFromNetscapeHtml(htmlContent: String): List<BookmarkItem> {
        val imported = mutableListOf<BookmarkItem>()
        val regex = "<A\\s+[^>]*HREF=[\"']([^\"']+)[\"'][^>]*>(.*?)</A>".toRegex(RegexOption.IGNORE_CASE)
        val matches = regex.findAll(htmlContent)

        for (match in matches) {
            val url = unescapeHtml(match.groupValues[1].trim())
            val rawTitle = unescapeHtml(match.groupValues[2].trim())
            val title = if (rawTitle.isBlank()) url else rawTitle

            if (url.startsWith("http://") || url.startsWith("https://")) {
                imported.add(
                    BookmarkItem(
                        id = UUID.randomUUID().toString(),
                        url = url,
                        title = title,
                        createdAtTimestamp = System.currentTimeMillis()
                    )
                )
            }
        }
        return imported
    }

    /**
     * Encrypts the backup JSON payload with AES-256-GCM and PBKDF2 key derivation.
     */
    fun encryptBackup(backup: FireballBackupData, passphrase: String): String {
        val plainJson = serializeBackup(backup)
        val salt = ByteArray(SALT_LENGTH).apply { SecureRandom().nextBytes(this) }
        val iv = ByteArray(GCM_IV_LENGTH).apply { SecureRandom().nextBytes(this) }

        val keySpec = PBEKeySpec(passphrase.toCharArray(), salt, ITERATION_COUNT, KEY_LENGTH)
        val secretKeyFactory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256")
        val keyBytes = secretKeyFactory.generateSecret(keySpec).encoded
        val secretKey = SecretKeySpec(keyBytes, "AES")

        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        val gcmSpec = GCMParameterSpec(GCM_TAG_LENGTH, iv)
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, gcmSpec)

        val cipherText = cipher.doFinal(plainJson.toByteArray(StandardCharsets.UTF_8))

        val bundleJson = JSONObject()
        bundleJson.put("version", 1)
        bundleJson.put("salt", safeBase64Encode(salt))
        bundleJson.put("iv", safeBase64Encode(iv))
        bundleJson.put("cipherText", safeBase64Encode(cipherText))

        return bundleJson.toString(2)
    }

    /**
     * Decrypts the backup JSON payload.
     */
    fun decryptBackup(encryptedBundleJson: String, passphrase: String): FireballBackupData {
        val json = JSONObject(encryptedBundleJson)
        val salt = safeBase64Decode(json.getString("salt"))
        val iv = safeBase64Decode(json.getString("iv"))
        val cipherText = safeBase64Decode(json.getString("cipherText"))

        val keySpec = PBEKeySpec(passphrase.toCharArray(), salt, ITERATION_COUNT, KEY_LENGTH)
        val secretKeyFactory = SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256")
        val keyBytes = secretKeyFactory.generateSecret(keySpec).encoded
        val secretKey = SecretKeySpec(keyBytes, "AES")

        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        val gcmSpec = GCMParameterSpec(GCM_TAG_LENGTH, iv)
        cipher.init(Cipher.DECRYPT_MODE, secretKey, gcmSpec)

        val decryptedBytes = cipher.doFinal(cipherText)
        val plainJson = String(decryptedBytes, StandardCharsets.UTF_8)

        return deserializeBackup(plainJson)
    }

    fun serializeBackup(backup: FireballBackupData): String {
        val root = JSONObject()
        root.put("version", backup.version)
        root.put("exportedTimestampMs", backup.exportedTimestampMs)

        val spacesArray = JSONArray()
        for (s in backup.spaces) {
            val spaceObj = JSONObject()
            spaceObj.put("id", s.id)
            spaceObj.put("name", s.name)
            spaceObj.put("profileId", s.profileId)
            spaceObj.put("isBurner", s.isBurner)
            spaceObj.put("accentColorHex", s.accentColorHex)
            spaceObj.put("iconName", s.iconName)
            spacesArray.put(spaceObj)
        }
        root.put("spaces", spacesArray)

        val bookmarksArray = JSONArray()
        for (b in backup.bookmarks) {
            val bObj = JSONObject()
            bObj.put("id", b.id)
            bObj.put("url", b.url)
            bObj.put("title", b.title)
            bObj.put("createdAtTimestamp", b.createdAtTimestamp)
            bObj.put("folder", b.folder)
            bookmarksArray.put(bObj)
        }
        root.put("bookmarks", bookmarksArray)

        val historyArray = JSONArray()
        for (h in backup.history) {
            val hObj = JSONObject()
            hObj.put("id", h.id)
            hObj.put("url", h.url)
            hObj.put("title", h.title)
            hObj.put("visitedAtTimestamp", h.visitedAtTimestamp)
            hObj.put("visitCount", h.visitCount)
            historyArray.put(hObj)
        }
        root.put("history", historyArray)

        return root.toString(2)
    }

    fun deserializeBackup(jsonStr: String): FireballBackupData {
        val root = JSONObject(jsonStr)
        val version = root.optInt("version", 1)
        val timestamp = root.optLong("exportedTimestampMs", System.currentTimeMillis())

        val spaces = mutableListOf<Space>()
        val spacesArray = root.optJSONArray("spaces")
        if (spacesArray != null) {
            for (i in 0 until spacesArray.length()) {
                val obj = spacesArray.getJSONObject(i)
                spaces.add(
                    Space(
                        id = obj.optString("id", "space-${UUID.randomUUID()}"),
                        name = obj.optString("name", "Space"),
                        profileId = obj.optString("profileId", "default-profile"),
                        isBurner = obj.optBoolean("isBurner", false),
                        accentColorHex = obj.optString("accentColorHex", "#B8FF3D"),
                        iconName = obj.optString("iconName", "globe")
                    )
                )
            }
        }

        val bookmarks = mutableListOf<BookmarkItem>()
        val bookmarksArray = root.optJSONArray("bookmarks")
        if (bookmarksArray != null) {
            for (i in 0 until bookmarksArray.length()) {
                val obj = bookmarksArray.getJSONObject(i)
                bookmarks.add(
                    BookmarkItem(
                        id = obj.optString("id", UUID.randomUUID().toString()),
                        url = obj.getString("url"),
                        title = obj.optString("title", "Bookmark"),
                        createdAtTimestamp = obj.optLong("createdAtTimestamp", System.currentTimeMillis()),
                        folder = obj.optString("folder", "Mobile Bookmarks")
                    )
                )
            }
        }

        val history = mutableListOf<HistoryItem>()
        val historyArray = root.optJSONArray("history")
        if (historyArray != null) {
            for (i in 0 until historyArray.length()) {
                val obj = historyArray.getJSONObject(i)
                history.add(
                    HistoryItem(
                        id = obj.optString("id", UUID.randomUUID().toString()),
                        url = obj.getString("url"),
                        title = obj.optString("title", "Page"),
                        visitedAtTimestamp = obj.optLong("visitedAtTimestamp", System.currentTimeMillis()),
                        visitCount = obj.optInt("visitCount", 1)
                    )
                )
            }
        }

        return FireballBackupData(
            version = version,
            exportedTimestampMs = timestamp,
            spaces = spaces,
            bookmarks = bookmarks,
            history = history
        )
    }

    private fun escapeHtml(text: String): String {
        return text
            .replace("&", "&amp;")
            .replace("<", "&lt;")
            .replace(">", "&gt;")
            .replace("\"", "&quot;")
            .replace("'", "&#39;")
    }

    private fun unescapeHtml(text: String): String {
        return text
            .replace("&lt;", "<")
            .replace("&gt;", ">")
            .replace("&quot;", "\"")
            .replace("&#39;", "'")
            .replace("&amp;", "&")
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
