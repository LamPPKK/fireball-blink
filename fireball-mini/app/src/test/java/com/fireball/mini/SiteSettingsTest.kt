package com.fireball.mini

import com.fireball.mini.core.models.PermissionStatus
import com.fireball.mini.core.models.SitePermissionType
import com.fireball.mini.data.SiteSettingsRepository
import org.junit.Assert.assertEquals
import org.junit.Test

class SiteSettingsTest {

    @Test
    fun testExtractCleanDomain() {
        assertEquals("duckduckgo.com", SiteSettingsRepository.extractCleanDomain("https://duckduckgo.com/settings"))
        assertEquals("github.com", SiteSettingsRepository.extractCleanDomain("http://github.com/LamPPKK/fireball-blink"))
        assertEquals("example.org", SiteSettingsRepository.extractCleanDomain("example.org/path/to/page"))
        assertEquals("sub.domain.co.uk", SiteSettingsRepository.extractCleanDomain("https://sub.domain.co.uk:443/test"))
    }

    @Test
    fun testPermissionStatusDisplayNames() {
        assertEquals("Cho phép (Allow)", PermissionStatus.ALLOW.displayName)
        assertEquals("Chặn (Block)", PermissionStatus.BLOCK.displayName)
        assertEquals("Hỏi trước (Ask)", PermissionStatus.ASK.displayName)
    }

    @Test
    fun testPermissionTypesEnum() {
        val types = SitePermissionType.values()
        assertEquals(8, types.size)
        assertEquals("Vị trí (Location)", SitePermissionType.LOCATION.displayName)
        assertEquals("Máy ảnh (Camera)", SitePermissionType.CAMERA.displayName)
        assertEquals("Microphone", SitePermissionType.MICROPHONE.displayName)
    }
}
