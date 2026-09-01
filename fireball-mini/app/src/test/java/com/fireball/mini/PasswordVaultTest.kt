package com.fireball.mini

import com.fireball.mini.core.engine.CryptoBase64Helper
import com.fireball.mini.core.models.DecryptedCredential
import com.fireball.mini.core.models.SavedCredential
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.nio.charset.StandardCharsets
import java.security.MessageDigest
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.spec.GCMParameterSpec
import javax.crypto.spec.SecretKeySpec

class PasswordVaultTest {

    @Test
    fun testBase64EncodingAndDecoding() {
        val testString = "Fireball Secure Password 123!@#"
        val encoded = CryptoBase64Helper.encodeToString(testString.toByteArray(StandardCharsets.UTF_8))
        assertNotNull(encoded)

        val decodedBytes = CryptoBase64Helper.decode(encoded)
        val decodedString = String(decodedBytes, StandardCharsets.UTF_8)
        assertEquals(testString, decodedString)
    }

    @Test
    fun testAesGcmEncryptionDecryptionFlow() {
        val plainPassword = "SuperSecretPassword#2026"
        val domain = "github.com"
        val username = "lamndt"

        val seed = "test_salt_seed_12345"
        val digest = MessageDigest.getInstance("SHA-256")
        val rawKey = digest.digest("fireball_vault_master_key_com.fireball.mini_$seed".toByteArray(StandardCharsets.UTF_8))
        val secretKey = SecretKeySpec(rawKey, "AES")

        val iv = ByteArray(12)
        SecureRandom().nextBytes(iv)

        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, GCMParameterSpec(128, iv))
        val encryptedBytes = cipher.doFinal(plainPassword.toByteArray(StandardCharsets.UTF_8))

        val encBase64 = CryptoBase64Helper.encodeToString(encryptedBytes)
        val ivBase64 = CryptoBase64Helper.encodeToString(iv)

        val savedCred = SavedCredential(
            id = "cred_1",
            domain = domain,
            username = username,
            encryptedPasswordBase64 = encBase64,
            ivBase64 = ivBase64
        )

        assertEquals("github.com", savedCred.domain)
        assertEquals("lamndt", savedCred.username)

        // Decrypt
        val decCipher = Cipher.getInstance("AES/GCM/NoPadding")
        val decIv = CryptoBase64Helper.decode(savedCred.ivBase64)
        val decEnc = CryptoBase64Helper.decode(savedCred.encryptedPasswordBase64)
        decCipher.init(Cipher.DECRYPT_MODE, secretKey, GCMParameterSpec(128, decIv))
        val decPlainBytes = decCipher.doFinal(decEnc)
        val decryptedText = String(decPlainBytes, StandardCharsets.UTF_8)

        assertEquals(plainPassword, decryptedText)
    }

    @Test
    fun testDecryptedCredentialDataModel() {
        val cred = DecryptedCredential(
            id = "cred_2",
            domain = "duckduckgo.com",
            username = "user@duck.com",
            plainPassword = "myPassword123",
            createdTimestamp = 1000L,
            lastUsedTimestamp = 2000L
        )
        assertEquals("cred_2", cred.id)
        assertEquals("duckduckgo.com", cred.domain)
        assertEquals("user@duck.com", cred.username)
        assertEquals("myPassword123", cred.plainPassword)
    }
}
