package com.fireball.mini.core.models

import java.util.UUID

enum class AiMessageSender {
    USER,
    ASSISTANT,
    SYSTEM
}

data class AiChatMessage(
    val id: String = UUID.randomUUID().toString(),
    val sender: AiMessageSender,
    val text: String,
    val timestampMs: Long = System.currentTimeMillis()
)

data class PageSummary(
    val title: String,
    val url: String,
    val domain: String,
    val keyTakeaways: List<String>,
    val briefOverview: String,
    val estimatedReadTimeMinutes: Int,
    val sentiment: String = "Neutral",
    val keywords: List<String> = emptyList(),
    val generatedTimestampMs: Long = System.currentTimeMillis()
)

data class ReaderArticle(
    val url: String,
    val title: String,
    val byline: String? = null,
    val domain: String,
    val contentHtml: String,
    val plainText: String,
    val estimatedReadingTimeMinutes: Int = 1,
    val publishedDate: String? = null
)

enum class TtsPlaybackStatus {
    IDLE,
    PLAYING,
    PAUSED,
    COMPLETED,
    ERROR
}

data class TtsState(
    val status: TtsPlaybackStatus = TtsPlaybackStatus.IDLE,
    val currentSentenceIndex: Int = 0,
    val totalSentencesCount: Int = 0,
    val speedRate: Float = 1.0f,
    val currentTextSnippet: String = ""
)

enum class ReaderTheme {
    DARK,
    SEPIA,
    CHARCOAL,
    OLED_BLACK
}
