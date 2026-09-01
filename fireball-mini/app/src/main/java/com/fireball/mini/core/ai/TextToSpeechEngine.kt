package com.fireball.mini.core.ai

import android.content.Context
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import com.fireball.mini.core.models.TtsPlaybackStatus
import com.fireball.mini.core.models.TtsState
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.util.Locale

class TextToSpeechEngine(private val context: Context) {

    private var tts: TextToSpeech? = null
    private var isInitialized = false

    private val _ttsState = MutableStateFlow(TtsState())
    val ttsState: StateFlow<TtsState> = _ttsState.asStateFlow()

    private var sentences: List<String> = emptyList()
    private var currentIndex: Int = 0

    init {
        tts = TextToSpeech(context.applicationContext) { status ->
            if (status == TextToSpeech.SUCCESS) {
                tts?.language = Locale.getDefault()
                isInitialized = true
                tts?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
                    override fun onStart(utteranceId: String?) {
                        _ttsState.value = _ttsState.value.copy(status = TtsPlaybackStatus.PLAYING)
                    }

                    override fun onDone(utteranceId: String?) {
                        playNextSentence()
                    }

                    override fun onError(utteranceId: String?) {
                        _ttsState.value = _ttsState.value.copy(status = TtsPlaybackStatus.ERROR)
                    }
                })
            }
        }
    }

    fun startReading(plainText: String) {
        if (!isInitialized || plainText.isBlank()) return

        sentences = plainText
            .split(Regex("(?<=[.!?\\n])\\s+"))
            .map { it.trim() }
            .filter { it.isNotBlank() }

        if (sentences.isEmpty()) return

        currentIndex = 0
        _ttsState.value = TtsState(
            status = TtsPlaybackStatus.PLAYING,
            currentSentenceIndex = 0,
            totalSentencesCount = sentences.size,
            speedRate = _ttsState.value.speedRate,
            currentTextSnippet = sentences.first()
        )
        speakSentence(0)
    }

    fun pause() {
        tts?.stop()
        _ttsState.value = _ttsState.value.copy(status = TtsPlaybackStatus.PAUSED)
    }

    fun resume() {
        if (currentIndex < sentences.size) {
            _ttsState.value = _ttsState.value.copy(status = TtsPlaybackStatus.PLAYING)
            speakSentence(currentIndex)
        }
    }

    fun stop() {
        tts?.stop()
        currentIndex = 0
        _ttsState.value = _ttsState.value.copy(
            status = TtsPlaybackStatus.IDLE,
            currentSentenceIndex = 0,
            currentTextSnippet = ""
        )
    }

    fun setSpeedRate(rate: Float) {
        tts?.setSpeechRate(rate)
        _ttsState.value = _ttsState.value.copy(speedRate = rate)
    }

    private fun playNextSentence() {
        currentIndex++
        if (currentIndex < sentences.size) {
            _ttsState.value = _ttsState.value.copy(
                currentSentenceIndex = currentIndex,
                currentTextSnippet = sentences[currentIndex]
            )
            speakSentence(currentIndex)
        } else {
            _ttsState.value = _ttsState.value.copy(
                status = TtsPlaybackStatus.COMPLETED,
                currentSentenceIndex = sentences.size
            )
        }
    }

    private fun speakSentence(index: Int) {
        if (index in sentences.indices) {
            val textToSpeak = sentences[index]
            tts?.speak(textToSpeak, TextToSpeech.QUEUE_FLUSH, null, "UTTERANCE_$index")
        }
    }

    fun release() {
        tts?.stop()
        tts?.shutdown()
        tts = null
    }
}
