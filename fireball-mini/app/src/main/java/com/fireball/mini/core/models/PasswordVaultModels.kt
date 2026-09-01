package com.fireball.mini.core.models

/**
 * Represents a saved web login credential stored in the on-device encrypted vault.
 */
data class SavedCredential(
    val id: String,
    val domain: String,
    val username: String,
    val encryptedPasswordBase64: String,
    val ivBase64: String,
    val createdTimestamp: Long = System.currentTimeMillis(),
    val lastUsedTimestamp: Long = System.currentTimeMillis()
)

/**
 * Decrypted plain credential for UI presentation / copy action.
 */
data class DecryptedCredential(
    val id: String,
    val domain: String,
    val username: String,
    val plainPassword: String,
    val createdTimestamp: Long,
    val lastUsedTimestamp: Long
)
