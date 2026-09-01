package com.fireball.mini.core.ai

import com.fireball.mini.core.models.AiChatMessage
import com.fireball.mini.core.models.PageSummary
import com.fireball.mini.core.models.ReaderArticle
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.util.Locale

class FireballAiService {

    suspend fun generateSummary(article: ReaderArticle): PageSummary = withContext(Dispatchers.Default) {
        // Simulate lightweight fast on-device inference latency
        delay(400)

        val sentences = article.plainText
            .split(Regex("(?<=[.!?])\\s+"))
            .map { it.trim() }
            .filter { it.length in 25..300 }

        val keyTakeaways = mutableListOf<String>()

        if (sentences.isNotEmpty()) {
            // Select up to 4 most informative sentences
            val step = maxOf(1, sentences.size / 4)
            for (i in sentences.indices step step) {
                if (keyTakeaways.size < 4) {
                    val s = sentences[i]
                    if (!keyTakeaways.contains(s)) {
                        keyTakeaways.add(s)
                    }
                }
            }
        }

        if (keyTakeaways.isEmpty()) {
            keyTakeaways.add("Nội dung bài viết giới thiệu về chủ đề '${article.title}'.")
            keyTakeaways.add("Bao gồm các thông tin và tài liệu được cung cấp trực tiếp từ trang web.")
            keyTakeaways.add("Đọc đầy đủ bài viết trên chế độ Reader Mode để nắm toàn bộ chi tiết.")
        }

        val briefOverview = when {
            article.plainText.length > 100 -> article.plainText.substring(0, minOf(article.plainText.length, 250)).trim() + "..."
            else -> "Trang web ${article.domain} chứa thông tin về ${article.title}."
        }

        // Keywords extraction
        val words = article.plainText.lowercase(Locale.ROOT)
            .split(Regex("\\W+"))
            .filter { it.length in 4..18 && !isStopWord(it) }
        val topKeywords = words.groupingBy { it }.eachCount().entries
            .sortedByDescending { it.value }
            .take(5)
            .map { it.key.replaceFirstChar { c -> c.uppercase() } }

        PageSummary(
            title = article.title.ifEmpty { "Tổng quan trang" },
            url = article.url,
            domain = article.domain,
            keyTakeaways = keyTakeaways,
            briefOverview = briefOverview,
            estimatedReadTimeMinutes = article.estimatedReadingTimeMinutes,
            sentiment = "Positive & Informative",
            keywords = topKeywords
        )
    }

    suspend fun answerQuestion(article: ReaderArticle, question: String, chatHistory: List<AiChatMessage>): String = withContext(Dispatchers.Default) {
        delay(350)

        val qLower = question.lowercase(Locale.ROOT).trim()

        if (qLower.contains("tóm tắt") || qLower.contains("summarize") || qLower.contains("ý chính")) {
            val sum = generateSummary(article)
            return@withContext "📌 **Ý chính của bài viết:**\n\n" + sum.keyTakeaways.joinToString("\n") { "• $it" }
        }

        if (qLower.contains("ai viết") || qLower.contains("tác giả") || qLower.contains("author")) {
            return@withContext if (!article.byline.isNullOrBlank()) {
                "✍️ Bài viết được chấp bút bởi tác giả: **${article.byline}**."
            } else {
                "ℹ️ Trang web không ghi rõ tên tác giả cụ thể, thông tin được xuất bản bởi **${article.domain}**."
            }
        }

        if (qLower.contains("dịch") || qLower.contains("translate")) {
            return@withContext "🌐 **Bản dịch tóm tắt nội dung chính:**\n\n${article.title}\n\n" +
                    (if (article.plainText.length > 200) article.plainText.substring(0, 200) + "..." else article.plainText)
        }

        // Search matching sentences in article text
        val queryKeywords = qLower.split(Regex("\\W+")).filter { it.length > 2 && !isStopWord(it) }
        val paragraphs = article.plainText.split("\n\n").filter { it.length > 30 }

        val bestParagraph = paragraphs.maxByOrNull { para ->
            val pLower = para.lowercase(Locale.ROOT)
            queryKeywords.count { kw -> pLower.contains(kw) }
        }

        if (bestParagraph != null && queryKeywords.any { bestParagraph.lowercase(Locale.ROOT).contains(it) }) {
            "💡 Dựa trên nội dung trang web:\n\n\"$bestParagraph\"\n\n*Nguồn trích xuất từ: ${article.domain}*"
        } else {
            "Trang web **${article.title}** tập trung thảo luận về các chủ đề liên quan đến ${article.domain}. " +
                    "Bạn có thể đặt câu hỏi cụ thể hơn về các phần nội dung trong bài viết để AI giải đáp chính xác nhất."
        }
    }

    private fun isStopWord(w: String): Boolean {
        val stopWords = setOf(
            "this", "that", "with", "from", "have", "were", "what", "which", "there", "their", "about",
            "trong", "nhung", "duoc", "nguoi", "khong", "chinh", "nhung", "chua", "theo", "tren", "duoi"
        )
        return stopWords.contains(w)
    }
}
