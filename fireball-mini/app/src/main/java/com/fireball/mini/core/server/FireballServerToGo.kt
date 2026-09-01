package com.fireball.mini.core.server

import android.graphics.Bitmap
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.io.OutputStream
import java.net.InetAddress
import java.net.NetworkInterface
import java.net.ServerSocket
import java.net.Socket
import java.util.Collections

/**
 * Fireball Server To Go:
 * Lightweight, battery-optimized embedded streaming server running directly on Android.
 * Enables an Android device to serve as a pocket host for Fireball Clients on Wi-Fi hotspot or LAN.
 */
class FireballServerToGo(
    val port: Int = 9090
) {
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var serverSocket: ServerSocket? = null
    private var serverJob: Job? = null

    private val _isRunning = MutableStateFlow(false)
    val isRunning: StateFlow<Boolean> = _isRunning.asStateFlow()

    private val _connectedClientsCount = MutableStateFlow(0)
    val connectedClientsCount: StateFlow<Int> = _connectedClientsCount.asStateFlow()

    private val _serverIpAddress = MutableStateFlow("127.0.0.1")
    val serverIpAddress: StateFlow<String> = _serverIpAddress.asStateFlow()

    private var currentFrameBytes: ByteArray? = null

    init {
        _serverIpAddress.value = getLocalIpAddress()
    }

    fun start() {
        if (serverJob?.isActive == true) return
        _isRunning.value = true
        _serverIpAddress.value = getLocalIpAddress()

        serverJob = scope.launch {
            try {
                serverSocket = ServerSocket(port)
                while (isActive && _isRunning.value) {
                    val socket = serverSocket?.accept() ?: break
                    launch {
                        handleClient(socket)
                    }
                }
            } catch (_: Exception) {
            } finally {
                _isRunning.value = false
            }
        }
    }

    fun updateFrame(bitmap: Bitmap) {
        val stream = ByteArrayOutputStream()
        bitmap.compress(Bitmap.CompressFormat.JPEG, 75, stream)
        currentFrameBytes = stream.toByteArray()
    }

    private fun handleClient(socket: Socket) {
        try {
            _connectedClientsCount.value++
            val input: InputStream = socket.getInputStream()
            val output: OutputStream = socket.getOutputStream()

            val buffer = ByteArray(2048)
            val bytesRead = input.read(buffer)
            if (bytesRead <= 0) return

            val requestStr = String(buffer, 0, bytesRead)
            val firstLine = requestStr.lines().firstOrNull() ?: ""

            when {
                firstLine.startsWith("GET /health") -> {
                    val body = "{\"status\":\"ok\",\"service\":\"fireball-server-togo\",\"version\":\"1.0.0\"}\n"
                    val response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n$body"
                    output.write(response.toByteArray())
                }
                firstLine.startsWith("GET /stream/frame") -> {
                    val frame = currentFrameBytes ?: createFallbackTile()
                    val headers = "HTTP/1.1 200 OK\r\nContent-Type: image/jpeg\r\nContent-Length: ${frame.size}\r\nConnection: close\r\n\r\n"
                    output.write(headers.toByteArray())
                    output.write(frame)
                }
                firstLine.startsWith("GET /pair/init") -> {
                    val ip = _serverIpAddress.value
                    val qrUrl = "fireball-beam://pair?host=$ip&port=$port&phrase=pocket-server-fireball-mobile-stream"
                    val body = "{\"status\":\"ok\",\"host\":\"$ip\",\"port\":$port,\"qr\":\"$qrUrl\"}\n"
                    val response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n$body"
                    output.write(response.toByteArray())
                }
                else -> {
                    val body = "Fireball Server To Go Online\n"
                    val response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ${body.length}\r\nConnection: close\r\n\r\n$body"
                    output.write(response.toByteArray())
                }
            }
            output.flush()
        } catch (_: Exception) {
        } finally {
            _connectedClientsCount.value = maxOf(0, _connectedClientsCount.value - 1)
            try { socket.close() } catch (_: Exception) {}
        }
    }

    private fun createFallbackTile(): ByteArray {
        // Minimal 1x1 dark PNG pixel
        return byteArrayOf(
            0x89.toByte(), 0x50.toByte(), 0x4E.toByte(), 0x47.toByte(), 0x0D.toByte(), 0x0A.toByte(), 0x1A.toByte(), 0x0A.toByte(),
            0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x0D.toByte(), 0x49.toByte(), 0x48.toByte(), 0x44.toByte(), 0x52.toByte(),
            0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x01.toByte(), 0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x01.toByte(),
            0x08.toByte(), 0x06.toByte(), 0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x1F.toByte(), 0x15.toByte(), 0xC4.toByte(),
            0x89.toByte(), 0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x0D.toByte(), 0x49.toByte(), 0x44.toByte(), 0x41.toByte(),
            0x54.toByte(), 0x78.toByte(), 0x9C.toByte(), 0x63.toByte(), 0x60.toByte(), 0x60.toByte(), 0x60.toByte(), 0x60.toByte(),
            0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x05.toByte(), 0x00.toByte(), 0x01.toByte(), 0xA7.toByte(), 0x35.toByte(),
            0x5B.toByte(), 0x65.toByte(), 0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x00.toByte(), 0x49.toByte(), 0x45.toByte(),
            0x4E.toByte(), 0x44.toByte(), 0xAE.toByte(), 0x42.toByte(), 0x60.toByte(), 0x82.toByte()
        )
    }

    private fun getLocalIpAddress(): String {
        try {
            val interfaces = Collections.list(NetworkInterface.getNetworkInterfaces())
            for (intf in interfaces) {
                val addrs = Collections.list(intf.inetAddresses)
                for (addr in addrs) {
                    if (!addr.isLoopbackAddress && addr.hostAddress != null) {
                        val sAddr = addr.hostAddress ?: ""
                        val isIPv4 = sAddr.indexOf(':') < 0
                        if (isIPv4) return sAddr
                    }
                }
            }
        } catch (_: Exception) {}
        return "127.0.0.1"
    }

    fun stop() {
        _isRunning.value = false
        try {
            serverSocket?.close()
        } catch (_: Exception) {}
        serverJob?.cancel()
        serverJob = null
    }

    fun destroy() {
        stop()
        scope.cancel()
    }
}
