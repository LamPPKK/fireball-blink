package com.fireball.mini

import com.fireball.mini.core.ai.ArticleExtractorHelper
import com.fireball.mini.core.ai.FireballAiService
import com.fireball.mini.core.models.AiMessageSender
import com.fireball.mini.core.models.ReaderArticle
import com.fireball.mini.data.AiAssistantRepository
import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

class AiAndReaderModeTest {

    @Test
    fun testArticleExtractorParsing() {
        val sampleJson = """
            {
                "title": "Quantum Computing Breakthrough in 2026",
                "byline": "Dr. Sarah Connor",
                "url": "https://example.com/tech/quantum-2026",
                "domain": "example.com",
                "plainText": "Quantum computing has achieved a massive milestone with fault-tolerant topological qubits. Researchers have demonstrated quantum error correction that scales exponentially. This advancement opens the door for simulating complex chemical reactions and molecular structures.",
                "wordCount": 42
            }
        """.trimIndent()

        val article = ArticleExtractorHelper.parseExtractedJson(sampleJson, "https://example.com/tech/quantum-2026")
        assertEquals("Quantum Computing Breakthrough in 2026", article.title)
        assertEquals("Dr. Sarah Connor", article.byline)
        assertEquals("example.com", article.domain)
        assertTrue(article.plainText.contains("fault-tolerant topological qubits"))
        assertEquals(1, article.estimatedReadingTimeMinutes)
    }

    @Test
    fun testAiSummaryGeneration() = runBlocking {
        val aiService = FireballAiService()
        val article = ReaderArticle(
            url = "https://news.ycombinator.com/article1",
            title = "Modern Browser Architecture and Blink Engine",
            byline = "Linus Tech",
            domain = "news.ycombinator.com",
            contentHtml = "<p>Blink engine powers modern browsers with isolated rendering processes. Tab discarding saves significant system memory. Security sandboxes isolate untrusted web code.</p>",
            plainText = "Blink engine powers modern browsers with isolated rendering processes. Tab discarding saves significant system memory. Security sandboxes isolate untrusted web code. High-performance native Boyer-Moore matching blocks tracking domains rapidly.",
            estimatedReadingTimeMinutes = 2
        )

        val summary = aiService.generateSummary(article)
        assertNotNull(summary)
        assertEquals("Modern Browser Architecture and Blink Engine", summary.title)
        assertEquals("news.ycombinator.com", summary.domain)
        assertTrue(summary.keyTakeaways.isNotEmpty())
        assertTrue(summary.briefOverview.isNotBlank())
    }

    @Test
    fun testAiQuestionAnswering() = runBlocking {
        val aiService = FireballAiService()
        val article = ReaderArticle(
            url = "https://fireball.dev/docs",
            title = "Fireball Mini High Performance Security Guide",
            byline = "Fireball Security Team",
            domain = "fireball.dev",
            contentHtml = "",
            plainText = "Fireball Mini incorporates hardware-accelerated Boyer-Moore pattern matching. It blocks 187,000 tracker rules natively in C++. Tab spaces ensure complete cookie partition isolation.",
            estimatedReadingTimeMinutes = 1
        )

        // Question about author
        val authorAnswer = aiService.answerQuestion(article, "Ai là tác giả của bài viết?", emptyList())
        assertTrue(authorAnswer.contains("Fireball Security Team"))

        // Question about summary
        val summaryAnswer = aiService.answerQuestion(article, "Tóm tắt bài viết này cho tôi", emptyList())
        assertTrue(summaryAnswer.contains("Ý chính"))

        // Context question
        val contentAnswer = aiService.answerQuestion(article, "Trình duyệt sử dụng thuật toán gì?", emptyList())
        assertTrue(contentAnswer.contains("Boyer-Moore") || contentAnswer.contains("Fireball"))
    }

    @Test
    fun testAiRepositoryLifecycle() = runBlocking {
        val repo = AiAssistantRepository()
        val article = ReaderArticle(
            url = "https://techcrunch.com/sample",
            title = "Autonomous AI Agents in Mobile Devices",
            byline = "TechCrunch",
            domain = "techcrunch.com",
            contentHtml = "",
            plainText = "Autonomous mobile agents provide on-device summarization and intelligent assistance. Real-time NLP simplifies web browsing.",
            estimatedReadingTimeMinutes = 1
        )

        repo.setExtractedArticle(article)
        assertEquals(article, repo.currentArticle.value)

        // Generate summary
        val summary = repo.generateSummaryForCurrentArticle()
        assertNotNull(summary)
        assertEquals(summary, repo.summary.value)

        // Send chat message
        repo.sendMessage("Tác giả là ai?")
        assertEquals(3, repo.chatMessages.value.size) // 1 initial + 1 user + 1 assistant
        assertEquals(AiMessageSender.USER, repo.chatMessages.value[1].sender)
        assertEquals(AiMessageSender.ASSISTANT, repo.chatMessages.value[2].sender)
    }
}
