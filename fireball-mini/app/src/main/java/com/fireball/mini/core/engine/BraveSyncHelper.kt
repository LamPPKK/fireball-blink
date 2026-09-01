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
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

object BraveSyncHelper {

    private val BIP39_WORDLIST = listOf(
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract", "absurd", "abuse",
        "access", "accident", "account", "accuse", "achieve", "acid", "acoustic", "acquire", "across", "act",
        "action", "actor", "actress", "actual", "adapt", "add", "addict", "address", "adjust", "admit",
        "adult", "advance", "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
        "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album", "alcohol", "alert",
        "alien", "all", "alley", "allow", "almost", "alone", "alpha", "already", "also", "alter",
        "always", "amateur", "amazing", "among", "amount", "amused", "analyst", "anchor", "ancient", "anger",
        "angle", "angry", "animal", "ankle", "announce", "annual", "another", "answer", "antenna", "antique",
        "anxiety", "any", "apart", "apology", "appear", "apple", "approve", "april", "arch", "arctic",
        "area", "arena", "argue", "arm", "armed", "armor", "army", "around", "arrange", "arrest",
        "arrive", "arrow", "art", "artefact", "artist", "artwork", "ask", "aspect", "assault", "asset",
        "assist", "assume", "asthma", "athlete", "atom", "attack", "attend", "attitude", "attract", "auction",
        "audit", "august", "aunt", "author", "auto", "autumn", "average", "avocado", "avoid", "awake",
        "aware", "away", "awesome", "awful", "awkward", "axis", "baby", "bachelor", "bacon", "badge",
        "bag", "balance", "balcony", "ball", "bamboo", "banana", "banner", "bar", "barely", "bargain",
        "barrel", "base", "basic", "basket", "battle", "beach", "bean", "beauty", "because", "become",
        "beef", "before", "begin", "behave", "behind", "believe", "below", "belt", "bench", "benefit",
        "best", "betray", "better", "between", "beyond", "bicycle", "bid", "bike", "bind", "biology",
        "bird", "birth", "bitter", "black", "blade", "blame", "blanket", "blast", "bleak", "bless",
        "blind", "blood", "blossom", "blouse", "blue", "blur", "blush", "board", "boat", "body",
        "boil", "bomb", "bone", "bonus", "book", "boost", "border", "boring", "borrow", "boss",
        "bottom", "bounce", "box", "boy", "bracket", "brain", "brand", "brass", "brave", "bread",
        "breeze", "brick", "bridge", "brief", "bright", "bring", "brisk", "broccoli", "broken", "bronze",
        "broom", "brother", "brown", "brush", "bubble", "buddy", "budget", "buffalo", "build", "bulb",
        "bulk", "bullet", "bundle", "bunker", "burden", "burger", "burst", "bus", "business", "busy",
        "butter", "buyer", "buzz", "cabbage", "cabin", "cable", "cactus", "cage", "cake", "call",
        "calm", "camera", "camp", "can", "canal", "cancel", "candy", "cannon", "canoe", "canvas",
        "canyon", "capable", "capital", "captain", "car", "carbon", "card", "cargo", "carpet", "carry",
        "cart", "case", "cash", "casino", "castle", "casual", "cat", "catalog", "catch", "category",
        "cattle", "caught", "cause", "caution", "cave", "ceiling", "celery", "cement", "census", "century",
        "cereal", "certain", "chair", "chalk", "champion", "change", "chaos", "chapter", "charge", "chase",
        "chat", "cheap", "check", "cheese", "chef", "cherry", "chest", "chicken", "chief", "child",
        "chimney", "choice", "choose", "chronic", "chuckle", "chunk", "churn", "cigar", "cinnamon", "circle",
        "citizen", "city", "civil", "claim", "clap", "clarify", "claw", "clay", "clean", "clerk",
        "clever", "click", "client", "cliff", "climb", "clinic", "clip", "clock", "clog", "close",
        "cloth", "cloud", "clown", "club", "clump", "cluster", "clutch", "coach", "coast", "coconut",
        "code", "coffee", "coil", "coin", "collect", "color", "column", "combine", "come", "comfort",
        "comic", "common", "company", "concert", "conduct", "confirm", "congress", "connect", "consider", "control"
    )

    /**
     * Generates a 24-word Brave Sync Chain seed phrase.
     */
    fun generateSyncWords(): List<String> {
        val random = SecureRandom()
        val words = mutableListOf<String>()
        val size = BIP39_WORDLIST.size
        for (i in 0 until 24) {
            words.add(BIP39_WORDLIST[random.nextInt(size)])
        }
        return words
    }

    /**
     * Validates if a sync code phrase is valid (at least 24 words).
     */
    fun isValidSyncWords(wordsStr: String): Boolean {
        val words = wordsStr.trim().split("\\s+".toRegex()).filter { it.isNotBlank() }
        return words.size in 24..25
    }

    /**
     * Derives a 256-bit AES key from the 24-word Brave Sync Chain code using SHA-256.
     */
    fun deriveKeyFromSyncWords(words: List<String>): SecretKeySpec {
        val rawMnemonic = words.joinToString(" ")
        val sha256 = MessageDigest.getInstance("SHA-256")
        val keyBytes = sha256.digest(rawMnemonic.toByteArray(StandardCharsets.UTF_8))
        return SecretKeySpec(keyBytes, "AES")
    }

    /**
     * Encrypts outgoing Brave Sync entities (Bookmarks, History, Open Tabs).
     */
    fun buildEncryptedSyncPacket(
        words: List<String>,
        bookmarks: List<BookmarkItem>,
        history: List<HistoryItem>,
        openTabs: List<TabItem>,
        deviceName: String = "Fireball Android"
    ): String {
        val root = JSONObject()
        root.put("protocol", "brave_sync_v2")
        root.put("timestamp", System.currentTimeMillis())
        root.put("deviceName", deviceName)

        val bmArray = JSONArray()
        for (b in bookmarks) {
            val item = JSONObject()
            item.put("id", b.id)
            item.put("url", b.url)
            item.put("title", b.title)
            item.put("createdAtTimestamp", b.createdAtTimestamp)
            bmArray.put(item)
        }
        root.put("bookmarks", bmArray)

        val histArray = JSONArray()
        for (h in history) {
            val item = JSONObject()
            item.put("id", h.id)
            item.put("url", h.url)
            item.put("title", h.title)
            item.put("visitedAtTimestamp", h.visitedAtTimestamp)
            item.put("visitCount", h.visitCount)
            histArray.put(item)
        }
        root.put("history", histArray)

        val tabArray = JSONArray()
        for (t in openTabs) {
            val item = JSONObject()
            item.put("id", t.id)
            item.put("url", t.url)
            item.put("title", t.title)
            item.put("deviceName", deviceName)
            item.put("timestamp", t.lastAccessedTimestampMs)
            tabArray.put(item)
        }
        root.put("tabs", tabArray)

        val secretKey = deriveKeyFromSyncWords(words)
        val iv = ByteArray(12).apply { SecureRandom().nextBytes(this) }
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, GCMParameterSpec(128, iv))
        val cipherBytes = cipher.doFinal(root.toString().toByteArray(StandardCharsets.UTF_8))

        val bundle = JSONObject()
        bundle.put("iv", safeBase64Encode(iv))
        bundle.put("payload", safeBase64Encode(cipherBytes))
        return bundle.toString()
    }

    /**
     * Decrypts incoming Brave Sync packet.
     */
    fun parseEncryptedSyncPacket(
        words: List<String>,
        encryptedBundleJson: String
    ): Triple<List<BookmarkItem>, List<HistoryItem>, List<RemoteTab>> {
        val bundle = JSONObject(encryptedBundleJson)
        val iv = safeBase64Decode(bundle.getString("iv"))
        val cipherBytes = safeBase64Decode(bundle.getString("payload"))

        val secretKey = deriveKeyFromSyncWords(words)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, secretKey, GCMParameterSpec(128, iv))
        val plainBytes = cipher.doFinal(cipherBytes)
        val jsonStr = String(plainBytes, StandardCharsets.UTF_8)

        val root = JSONObject(jsonStr)
        val newBookmarks = mutableListOf<BookmarkItem>()
        val bmArray = root.optJSONArray("bookmarks")
        if (bmArray != null) {
            for (i in 0 until bmArray.length()) {
                val obj = bmArray.getJSONObject(i)
                newBookmarks.add(
                    BookmarkItem(
                        id = obj.optString("id", UUID.randomUUID().toString()),
                        url = obj.getString("url"),
                        title = obj.optString("title", "Bookmark"),
                        createdAtTimestamp = obj.optLong("createdAtTimestamp", System.currentTimeMillis())
                    )
                )
            }
        }

        val newHistory = mutableListOf<HistoryItem>()
        val histArray = root.optJSONArray("history")
        if (histArray != null) {
            for (i in 0 until histArray.length()) {
                val obj = histArray.getJSONObject(i)
                newHistory.add(
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

        val remoteTabs = mutableListOf<RemoteTab>()
        val tabArray = root.optJSONArray("tabs")
        if (tabArray != null) {
            for (i in 0 until tabArray.length()) {
                val obj = tabArray.getJSONObject(i)
                remoteTabs.add(
                    RemoteTab(
                        id = obj.optString("id", UUID.randomUUID().toString()),
                        title = obj.optString("title", "Tab"),
                        url = obj.getString("url"),
                        deviceName = obj.optString("deviceName", "Brave Device"),
                        timestampMs = obj.optLong("timestamp", System.currentTimeMillis())
                    )
                )
            }
        }

        return Triple(newBookmarks, newHistory, remoteTabs)
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
