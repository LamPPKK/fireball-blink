package com.fireball.mini

import com.fireball.mini.core.server.FireballServerToGo
import org.junit.Assert.*
import org.junit.Test

class ServerToGoAndClientTest {

    @Test
    fun testServerToGoConfiguration() {
        val server = FireballServerToGo(port = 9090)
        assertEquals(9090, server.port)
        assertFalse(server.isRunning.value)
        assertEquals(0, server.connectedClientsCount.value)
        assertNotNull(server.serverIpAddress.value)
        assertTrue(server.serverIpAddress.value.isNotEmpty())
    }

    @Test
    fun testServerCustomPort() {
        val server = FireballServerToGo(port = 9095)
        assertEquals(9095, server.port)
        assertFalse(server.isRunning.value)
    }
}
