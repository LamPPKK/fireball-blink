package com.fireball.mini.data

import android.content.Context
import android.content.SharedPreferences
import com.fireball.mini.FireballApp
import com.fireball.mini.core.engine.CryptoBase64Helper
import com.fireball.mini.core.models.DecryptedCredential
import com.fireball.mini.core.models.SavedCredential
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

class PasswordVaultRepository(context: Context? = null) {
    private val resolvedContext: Context? = context ?: FireballApp.instance
    private val prefs: SharedPreferences? = try {
        resolvedContext?.getSharedPreferences("fireball_passwords_vault", Context.MODE_PRIVATE)
    } catch (_: Throwable) {
        null
    }

    private val _credentials = MutableStateFlow<List<SavedCredential>>(emptyList())
    val credentials: StateFlow<List<SavedCredential>> = _credentials.asStateFlow()

    private val vaultSecretKey: SecretKeySpec by lazy {
        deriveVaultKey()
    }

    init {
        loadCredentials()
    }

    private fun deriveVaultKey(): SecretKeySpec {
        var seed = prefs?.getString("vault_salt_seed", null)
        if (seed == null) {
            val randomBytes = ByteArray(32)
            SecureRandom().nextBytes(randomBytes)
            seed = CryptoBase64Helper.encodeToString(randomBytes)
            prefs?.edit()?.putString("vault_salt_seed", seed)?.apply()
        }

        val pkgName = resolvedContext?.packageName ?: "com.fireball.mini"
        val digest = MessageDigest.getInstance("SHA-256")
        val rawKey = digest.digest("fireball_vault_master_key_${pkgName}_$seed".toByteArray(StandardCharsets.UTF_8))
        return SecretKeySpec(rawKey, "AES")
    }

    private fun loadCredentials() {
        val json = prefs?.getString("saved_credentials_json", "[]") ?: "[]"
        val list = mutableListOf<SavedCredential>()
        try {
            val arr = JSONArray(json)
            for (i in 0 until arr.length()) {
                val obj = arr.getJSONObject(i)
                list.add(
                    SavedCredential(
                        id = obj.getString("id"),
                        domain = obj.getString("domain"),
                        username = obj.getString("username"),
                        encryptedPasswordBase64 = obj.getString("encryptedPassword"),
                        ivBase64 = obj.getString("iv"),
                        createdTimestamp = obj.optLong("created", System.currentTimeMillis()),
                        lastUsedTimestamp = obj.optLong("lastUsed", System.currentTimeMillis())
                    )
                )
            }
        } catch (_: Exception) {}
        _credentials.value = list
    }

    fun saveCredential(domain: String, username: String, plainPassword: String): SavedCredential {
        val cleanDomain = SiteSettingsRepository.extractCleanDomain(domain)
        val iv = ByteArray(12)
        SecureRandom().nextBytes(iv)

        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, vaultSecretKey, GCMParameterSpec(128, iv))
        val encryptedBytes = cipher.doFinal(plainPassword.toByteArray(StandardCharsets.UTF_8))

        val encBase64 = CryptoBase64Helper.encodeToString(encryptedBytes)
        val ivBase64 = CryptoBase64Helper.encodeToString(iv)

        val existing = _credentials.value.firstOrNull { it.domain.equals(cleanDomain, ignoreCase = true) && it.username == username }
        val id = existing?.id ?: "cred_${System.currentTimeMillis()}_${(100..999).random()}"

        val cred = SavedCredential(
            id = id,
            domain = cleanDomain,
            username = username,
            encryptedPasswordBase64 = encBase64,
            ivBase64 = ivBase64,
            createdTimestamp = existing?.createdTimestamp ?: System.currentTimeMillis(),
            lastUsedTimestamp = System.currentTimeMillis()
        )

        val updated = _credentials.value.filterNot { it.id == id } + cred
        _credentials.value = updated
        persistCredentials()
        return cred
    }

    fun decryptCredential(cred: SavedCredential): DecryptedCredential? {
        return try {
            val iv = CryptoBase64Helper.decode(cred.ivBase64)
            val encBytes = CryptoBase64Helper.decode(cred.encryptedPasswordBase64)

            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, vaultSecretKey, GCMParameterSpec(128, iv))
            val plainBytes = cipher.doFinal(encBytes)
            val plain = String(plainBytes, StandardCharsets.UTF_8)

            DecryptedCredential(
                id = cred.id,
                domain = cred.domain,
                username = cred.username,
                plainPassword = plain,
                createdTimestamp = cred.createdTimestamp,
                lastUsedTimestamp = cred.lastUsedTimestamp
            )
        } catch (_: Exception) {
            null
        }
    }

    fun deleteCredential(id: String) {
        val updated = _credentials.value.filterNot { it.id == id }
        _credentials.value = updated
        persistCredentials()
    }

    fun clearAllCredentials() {
        _credentials.value = emptyList()
        persistCredentials()
    }

    fun getCredentialsForDomain(domain: String): List<SavedCredential> {
        val cleanDomain = SiteSettingsRepository.extractCleanDomain(domain)
        return _credentials.value.filter { it.domain.equals(cleanDomain, ignoreCase = true) }
    }

    private fun persistCredentials() {
        val arr = JSONArray()
        for (c in _credentials.value) {
            val obj = JSONObject().apply {
                put("id", c.id)
                put("domain", c.domain)
                put("username", c.username)
                put("encryptedPassword", c.encryptedPasswordBase64)
                put("iv", c.ivBase64)
                put("created", c.createdTimestamp)
                put("lastUsed", c.lastUsedTimestamp)
            }
            arr.put(obj)
        }
        prefs?.edit()?.putString("saved_credentials_json", arr.toString())?.apply()
    }
}
