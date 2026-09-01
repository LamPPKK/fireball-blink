package com.fireball.mini.core.vault

import java.nio.ByteBuffer
import javax.crypto.Mac
import javax.crypto.spec.SecretKeySpec
import kotlin.math.pow

/**
 * Local RFC-6238 TOTP (Time-Based One-Time Password) Authenticator.
 * Generates standard 6-digit 2FA verification codes every 30 seconds.
 */
object TotpAuthenticator {

    private const val TIME_STEP_SECONDS = 30L
    private const val DIGITS = 6

    /**
     * Generates a 6-digit TOTP code for a given Base32 secret key.
     */
    fun generateCurrentCode(base32Secret: String, timestampSeconds: Long = System.currentTimeMillis() / 1000L): String {
        val counter = timestampSeconds / TIME_STEP_SECONDS
        val keyBytes = decodeBase32(base32Secret.replace(" ", "").uppercase())
        if (keyBytes.isEmpty()) return "000000"

        val data = ByteBuffer.allocate(8).putLong(counter).array()
        val mac = Mac.getInstance("HmacSHA1").apply {
            init(SecretKeySpec(keyBytes, "HmacSHA1"))
        }
        val hash = mac.doFinal(data)

        val offset = hash[hash.size - 1].toInt() and 0x0F
        val binary = ((hash[offset].toInt() and 0x7F) shl 24) or
                ((hash[offset + 1].toInt() and 0xFF) shl 16) or
                ((hash[offset + 2].toInt() and 0xFF) shl 8) or
                (hash[offset + 3].toInt() and 0xFF)

        val otp = binary % (10.0.pow(DIGITS.toDouble()).toInt())
        return otp.toString().padStart(DIGITS, '0')
    }

    /**
     * Calculates remaining seconds in the current 30-second window.
     */
    fun getRemainingSeconds(timestampSeconds: Long = System.currentTimeMillis() / 1000L): Int {
        return (TIME_STEP_SECONDS - (timestampSeconds % TIME_STEP_SECONDS)).toInt()
    }

    /**
     * Decodes a standard Base32 string (RFC 4648) into raw byte array.
     */
    fun decodeBase32(input: String): ByteArray {
        val base32Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"
        val cleanInput = input.trim().replace("=", "")
        val bytes = ArrayList<Byte>()

        var buffer = 0
        var bitsLeft = 0

        for (c in cleanInput) {
            val valIndex = base32Chars.indexOf(c)
            if (valIndex < 0) continue

            buffer = (buffer shl 5) or valIndex
            bitsLeft += 5

            if (bitsLeft >= 8) {
                bytes.add(((buffer shr (bitsLeft - 8)) and 0xFF).toByte())
                bitsLeft -= 8
            }
        }
        return bytes.toByteArray()
    }
}
