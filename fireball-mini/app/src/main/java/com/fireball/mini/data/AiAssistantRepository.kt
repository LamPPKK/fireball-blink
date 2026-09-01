package com.fireball.mini.data

import com.fireball.mini.core.ai.FireballAiService
import com.fireball.mini.core.models.AiChatMessage
import com.fireball.mini.core.models.AiMessageSender
import com.fireball.mini.core.models.PageSummary
import com.fireball.mini.core.models.ReaderArticle
import com.fireball.mini.core.models.ReaderTheme
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class AiAssistantRepository(
    private val aiService: FireballAiService = FireballAiService()
) {
    private val _currentArticle = MutableStateFlow<ReaderArticle?>(null)
    val currentArticle: StateFlow<ReaderArticle?> = _currentArticle.asStateFlow()

    private val _summary = MutableStateFlow<PageSummary?>(null)
    val summary: StateFlow<PageSummary?> = _summary.asStateFlow()

    private val _isGeneratingSummary = MutableStateFlow(false)
    val isGeneratingSummary: StateFlow<Boolean> = _isGeneratingSummary.asStateFlow()

    private val _chatMessages = MutableStateFlow<List<AiChatMessage>>(emptyList())
    val chatMessages: StateFlow<List<AiChatMessage>> = _chatMessages.asStateFlow()

    private val _isChatLoading = MutableStateFlow(false)
    val isChatLoading: StateFlow<Boolean> = _isChatLoading.asStateFlow()

    private val _isReaderModeActive = MutableStateFlow(false)
    val isReaderModeActive: StateFlow<Boolean> = _isReaderModeActive.asStateFlow()

    private val _readerTheme = MutableStateFlow(ReaderTheme.DARK)
    val readerTheme: StateFlow<ReaderTheme> = _readerTheme.asStateFlow()

    private val _readerFontSizeSp = MutableStateFlow(16)
    val readerFontSizeSp: StateFlow<Int> = _readerFontSizeSp.asStateFlow()

    fun setExtractedArticle(article: ReaderArticle) {
        _currentArticle.value = article
        // Reset summary if new article is set
        if (_summary.value?.url != article.url) {
            _summary.value = null
            _chatMessages.value = listOf(
                AiChatMessage(
                    sender = AiMessageSender.ASSISTANT,
                    text = "Xin chào! Tôi là trợ lý Fireball AI. Bạn có thể hỏi bất kỳ câu hỏi nào về trang web **${article.title}** hoặc yêu cầu tôi tóm tắt, dịch thuật, giải thích."
                )
            )
        }
    }

    suspend fun generateSummaryForCurrentArticle(): PageSummary? {
        val article = _currentArticle.value ?: return null
        _isGeneratingSummary.value = true
        return try {
            val sum = aiService.generateSummary(article)
            _summary.value = sum
            sum
        } finally {
            _isGeneratingSummary.value = false
        }
    }

    suspend fun sendMessage(userQuestion: String) {
        val article = _currentArticle.value ?: return
        val userMsg = AiChatMessage(sender = AiMessageSender.USER, text = userQuestion)
        _chatMessages.value = _chatMessages.value + userMsg
        _isChatLoading.value = true

        try {
            val answer = aiService.answerQuestion(article, userQuestion, _chatMessages.value)
            val assistantMsg = AiChatMessage(sender = AiMessageSender.ASSISTANT, text = answer)
            _chatMessages.value = _chatMessages.value + assistantMsg
        } finally {
            _isChatLoading.value = false
        }
    }

    fun setReaderModeActive(active: Boolean) {
        _isReaderModeActive.value = active
    }

    fun setReaderTheme(theme: ReaderTheme) {
        _readerTheme.value = theme
    }

    fun setReaderFontSize(sizeSp: Int) {
        _readerFontSizeSp.value = sizeSp.coerceIn(12, 28)
    }

    fun clearChat() {
        val title = _currentArticle.value?.title ?: "trang web"
        _chatMessages.value = listOf(
            AiChatMessage(
                sender = AiMessageSender.ASSISTANT,
                text = "Xin chào! Bạn muốn tìm hiểu hoặc đặt câu hỏi gì về **$title**?"
            )
        )
    }
}
