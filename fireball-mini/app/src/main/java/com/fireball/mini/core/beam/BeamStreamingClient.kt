package com.fireball.mini.core.beam

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * Android Fireball Beam Client
 * Connects to Fireball Server for remote browser execution, streaming, and touch mapping.
 */
class BeamStreamingClient(
    private var serverUrl: String = "http://10.0.2.2:9090"
) {
    private val scope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var streamJob: Job? = null

    private val _isStreaming = MutableStateFlow(false)
    val isStreaming: StateFlow<Boolean> = _isStreaming.asStateFlow()

    private val _currentFrame = MutableStateFlow<Bitmap?>(null)
    val currentFrame: StateFlow<Bitmap?> = _currentFrame.asStateFlow()

    private val _statusMessage = MutableStateFlow("Disconnected")
    val statusMessage: StateFlow<String> = _statusMessage.asStateFlow()

    fun setServerUrl(url: String) {
        this.serverUrl = url.trimEnd('/')
    }

    fun startStream() {
        if (streamJob?.isActive == true) return
        _isStreaming.value = true
        _statusMessage.value = "Connecting to Fireball Server..."

        streamJob = scope.launch {
            while (isActive && _isStreaming.value) {
                try {
                    val url = URL("$serverUrl/stream/frame?format=jpeg&q=80")
                    val conn = (url.openConnection() as HttpURLConnection).apply {
                        requestMethod = "GET"
                        connectTimeout = 3000
                        readTimeout = 3000
                    }

                    if (conn.responseCode == 200) {
                        conn.inputStream.use { input ->
                            val bitmap = BitmapFactory.decodeStream(input)
                            if (bitmap != null) {
                                _currentFrame.value = bitmap
                                _statusMessage.value = "⚡ Live Streaming (Fireball Server)"
                            }
                        }
                    } else {
                        _statusMessage.value = "Server response: ${conn.responseCode}"
                    }
                } catch (e: Exception) {
                    _statusMessage.value = "Stream error: ${e.localizedMessage ?: "offline"}"
                }

                // 30 FPS stream interval (~33ms)
                delay(33)
            }
        }
    }

    fun stopStream() {
        _isStreaming.value = false
        streamJob?.cancel()
        streamJob = null
        _statusMessage.value = "Stream Stopped"
    }

    fun sendClick(normX: Float, normY: Float) {
        postEvent("/input/click?x=$normX&y=$normY")
    }

    fun sendMove(normX: Float, normY: Float) {
        postEvent("/input/move?x=$normX&y=$normY")
    }

    fun sendScroll(dx: Int, dy: Int) {
        postEvent("/input/scroll?dx=$dx&dy=$dy")
    }

    fun sendNewTab() {
        postEvent("/tabs/new")
    }

    fun sendReload() {
        postEvent("/navigation/reload")
    }

    private fun postEvent(path: String) {
        scope.launch {
            try {
                val url = URL("$serverUrl$path")
                val conn = (url.openConnection() as HttpURLConnection).apply {
                    requestMethod = "POST"
                    connectTimeout = 1500
                    readTimeout = 1500
                }
                conn.responseCode // execute
                conn.disconnect()
            } catch (_: Exception) {}
        }
    }

    fun destroy() {
        stopStream()
        scope.cancel()
    }
}
