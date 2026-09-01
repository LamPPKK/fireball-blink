package com.fireball.mini.ui.screens

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.FormatSize
import androidx.compose.material.icons.filled.Headphones
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material.icons.filled.Timer
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.ReaderTheme
import com.fireball.mini.core.models.TtsPlaybackStatus
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.viewmodels.BrowserViewModel

@Composable
fun ReaderModeScreen(
    viewModel: BrowserViewModel,
    onClose: () -> Unit
) {
    val article by viewModel.currentArticle.collectAsState()
    val readerTheme by viewModel.readerTheme.collectAsState()
    val fontSizeSp by viewModel.readerFontSizeSp.collectAsState()
    val ttsState by viewModel.ttsState.collectAsState()

    var showFormattingMenu by remember { mutableStateOf(false) }

    // Theme Color Palettes
    val (backgroundColor, textColor, subtextColor, surfaceColor) = when (readerTheme) {
        ReaderTheme.DARK -> Quad(Color(0xFF16151D), Color(0xFFF3F4F6), Color(0xFF9CA3AF), Color(0xFF22202C))
        ReaderTheme.SEPIA -> Quad(Color(0xFFFBF0D9), Color(0xFF2C2214), Color(0xFF6B583E), Color(0xFFF2E3C6))
        ReaderTheme.CHARCOAL -> Quad(Color(0xFF2B2D42), Color(0xFFEDF2F4), Color(0xFF8D99AE), Color(0xFF383A56))
        ReaderTheme.OLED_BLACK -> Quad(Color(0xFF000000), Color(0xFFE5E7EB), Color(0xFF6B7280), Color(0xFF121212))
    }

    Scaffold(
        topBar = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(surfaceColor)
                    .statusBarsPadding()
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 6.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        IconButton(onClick = onClose) {
                            Icon(
                                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                                contentDescription = "Close Reader",
                                tint = textColor,
                                modifier = Modifier.size(20.dp)
                            )
                        }

                        Text(
                            text = article?.domain ?: "Reader Mode",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                            color = textColor,
                            fontSize = 15.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        // TTS Audio Play / Pause
                        IconButton(
                            onClick = {
                                if (ttsState.status == TtsPlaybackStatus.PLAYING) {
                                    viewModel.pauseArticleTts()
                                } else {
                                    viewModel.playArticleTts()
                                }
                            }
                        ) {
                            Icon(
                                imageVector = if (ttsState.status == TtsPlaybackStatus.PLAYING) Icons.Default.Pause else Icons.Default.Headphones,
                                contentDescription = "Text to Speech",
                                tint = if (ttsState.status == TtsPlaybackStatus.PLAYING) FireballElectricLime else textColor,
                                modifier = Modifier.size(22.dp)
                            )
                        }

                        // Font & Theme Options
                        IconButton(onClick = { showFormattingMenu = !showFormattingMenu }) {
                            Icon(
                                imageVector = Icons.Default.FormatSize,
                                contentDescription = "Font & Theme",
                                tint = textColor,
                                modifier = Modifier.size(20.dp)
                            )
                        }
                    }
                }

                // Formatting Settings Expansion
                AnimatedVisibility(visible = showFormattingMenu) {
                    Column(
                        modifier = Modifier
                            .fillMaxWidth()
                            .background(backgroundColor.copy(alpha = 0.95f))
                            .border(1.dp, FireballBorder)
                            .padding(14.dp)
                    ) {
                        // Font Size Controls
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.SpaceBetween,
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text(
                                text = "Kích thước chữ: ${fontSizeSp}sp",
                                style = MaterialTheme.typography.bodyMedium,
                                color = textColor,
                                fontSize = 13.sp
                            )

                            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(8.dp))
                                        .background(surfaceColor)
                                        .clickable { viewModel.setReaderFontSize(fontSizeSp - 2) }
                                        .padding(horizontal = 12.dp, vertical = 6.dp)
                                ) {
                                    Text(text = "A-", color = textColor, fontWeight = FontWeight.Bold)
                                }

                                Box(
                                    modifier = Modifier
                                        .clip(RoundedCornerShape(8.dp))
                                        .background(surfaceColor)
                                        .clickable { viewModel.setReaderFontSize(fontSizeSp + 2) }
                                        .padding(horizontal = 12.dp, vertical = 6.dp)
                                ) {
                                    Text(text = "A+", color = textColor, fontWeight = FontWeight.Bold)
                                }
                            }
                        }

                        Spacer(modifier = Modifier.height(12.dp))

                        // Theme Selector
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            ThemePill("Tối", ReaderTheme.DARK, readerTheme == ReaderTheme.DARK, surfaceColor, textColor) {
                                viewModel.setReaderTheme(ReaderTheme.DARK)
                            }
                            ThemePill("Sepia", ReaderTheme.SEPIA, readerTheme == ReaderTheme.SEPIA, surfaceColor, textColor) {
                                viewModel.setReaderTheme(ReaderTheme.SEPIA)
                            }
                            ThemePill("Than", ReaderTheme.CHARCOAL, readerTheme == ReaderTheme.CHARCOAL, surfaceColor, textColor) {
                                viewModel.setReaderTheme(ReaderTheme.CHARCOAL)
                            }
                            ThemePill("OLED", ReaderTheme.OLED_BLACK, readerTheme == ReaderTheme.OLED_BLACK, surfaceColor, textColor) {
                                viewModel.setReaderTheme(ReaderTheme.OLED_BLACK)
                            }
                        }
                    }
                }
            }
        }
    ) { paddingValues ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .background(backgroundColor)
        ) {
            val articleContent = article
            if (articleContent != null) {
                LazyColumn(
                    contentPadding = PaddingValues(horizontal = 24.dp, vertical = 20.dp),
                    modifier = Modifier.fillMaxSize()
                ) {
                    // Article Title
                    item {
                        Text(
                            text = articleContent.title,
                            style = MaterialTheme.typography.headlineMedium.copy(
                                fontWeight = FontWeight.ExtraBold,
                                fontFamily = FontFamily.Serif
                            ),
                            color = textColor,
                            fontSize = (fontSizeSp + 6).sp,
                            lineHeight = (fontSizeSp + 14).sp
                        )
                        Spacer(modifier = Modifier.height(10.dp))
                    }

                    // Byline & Reading Time
                    item {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(10.dp),
                            modifier = Modifier.padding(bottom = 20.dp)
                        ) {
                            if (!articleContent.byline.isNullOrBlank()) {
                                Text(
                                    text = articleContent.byline,
                                    style = MaterialTheme.typography.bodySmall.copy(fontWeight = FontWeight.SemiBold),
                                    color = FireballElectricLime,
                                    fontSize = 12.sp
                                )
                                Text(text = "•", color = subtextColor)
                            }

                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    imageVector = Icons.Default.Timer,
                                    contentDescription = null,
                                    tint = subtextColor,
                                    modifier = Modifier.size(13.dp)
                                )
                                Spacer(modifier = Modifier.width(4.dp))
                                Text(
                                    text = "~${articleContent.estimatedReadingTimeMinutes} phút đọc",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = subtextColor,
                                    fontSize = 12.sp
                                )
                            }
                        }

                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .height(1.dp)
                                .background(subtextColor.copy(alpha = 0.2f))
                        )
                        Spacer(modifier = Modifier.height(20.dp))
                    }

                    // Article Paragraphs
                    val paragraphs = articleContent.plainText.split("\n\n").filter { it.isNotBlank() }
                    items(paragraphs.size) { index ->
                        val para = paragraphs[index]
                        val isCurrentlySpeaking = ttsState.status == TtsPlaybackStatus.PLAYING &&
                                ttsState.currentTextSnippet.isNotBlank() &&
                                para.contains(ttsState.currentTextSnippet.take(20))

                        Text(
                            text = para,
                            style = MaterialTheme.typography.bodyLarge.copy(
                                fontFamily = FontFamily.Serif,
                                lineHeight = (fontSizeSp * 1.6f).sp
                            ),
                            color = if (isCurrentlySpeaking) FireballElectricLime else textColor,
                            fontSize = fontSizeSp.sp,
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(bottom = 16.dp)
                        )
                    }

                    item {
                        Spacer(modifier = Modifier.height(80.dp))
                    }
                }
            } else {
                Box(
                    modifier = Modifier.fillMaxSize(),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "Không tìm thấy nội dung bài viết phù hợp cho Reader Mode.",
                        color = subtextColor,
                        style = MaterialTheme.typography.bodyMedium
                    )
                }
            }

            // Floating TTS Audio Playback Bar
            AnimatedVisibility(
                visible = ttsState.status == TtsPlaybackStatus.PLAYING || ttsState.status == TtsPlaybackStatus.PAUSED,
                enter = fadeIn(),
                exit = fadeOut(),
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .padding(16.dp)
            ) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(28.dp))
                        .background(surfaceColor)
                        .border(1.dp, FireballElectricLime.copy(alpha = 0.5f), RoundedCornerShape(28.dp))
                        .padding(horizontal = 16.dp, vertical = 10.dp)
                ) {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.SpaceBetween,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier.weight(1f)
                        ) {
                            Box(
                                modifier = Modifier
                                    .size(36.dp)
                                    .clip(CircleShape)
                                    .background(FireballActiveSurface)
                                    .clickable {
                                        if (ttsState.status == TtsPlaybackStatus.PLAYING) {
                                            viewModel.pauseArticleTts()
                                        } else {
                                            viewModel.resumeArticleTts()
                                        }
                                    },
                                contentAlignment = Alignment.Center
                            ) {
                                Icon(
                                    imageVector = if (ttsState.status == TtsPlaybackStatus.PLAYING) Icons.Default.Pause else Icons.Default.PlayArrow,
                                    contentDescription = "Play/Pause",
                                    tint = FireballElectricLime,
                                    modifier = Modifier.size(20.dp)
                                )
                            }

                            Spacer(modifier = Modifier.width(10.dp))

                            Column {
                                Text(
                                    text = if (ttsState.status == TtsPlaybackStatus.PLAYING) "Đang phát giọng đọc AI..." else "Tạm dừng giọng đọc",
                                    style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.Bold),
                                    color = textColor,
                                    fontSize = 12.sp
                                )
                                Text(
                                    text = "Câu ${ttsState.currentSentenceIndex + 1} / ${ttsState.totalSentencesCount}",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = subtextColor,
                                    fontSize = 10.sp
                                )
                            }
                        }

                        // Speed Pill & Stop
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Box(
                                modifier = Modifier
                                    .clip(RoundedCornerShape(12.dp))
                                    .background(FireballActiveSurface)
                                    .clickable {
                                        val nextRate = when (ttsState.speedRate) {
                                            1.0f -> 1.25f
                                            1.25f -> 1.5f
                                            1.5f -> 2.0f
                                            else -> 1.0f
                                        }
                                        viewModel.setTtsSpeedRate(nextRate)
                                    }
                                    .padding(horizontal = 8.dp, vertical = 4.dp)
                            ) {
                                Text(
                                    text = "${ttsState.speedRate}x",
                                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                                    color = FireballElectricLime,
                                    fontSize = 11.sp
                                )
                            }

                            Spacer(modifier = Modifier.width(8.dp))

                            IconButton(
                                onClick = { viewModel.stopArticleTts() },
                                modifier = Modifier.size(32.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Stop,
                                    contentDescription = "Stop",
                                    tint = subtextColor,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun ThemePill(
    label: String,
    theme: ReaderTheme,
    isSelected: Boolean,
    surfaceColor: Color,
    textColor: Color,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .background(if (isSelected) FireballActiveSurface else surfaceColor)
            .border(
                1.dp,
                if (isSelected) FireballElectricLime else FireballBorder,
                RoundedCornerShape(8.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 12.dp, vertical = 6.dp)
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall.copy(
                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium
            ),
            color = if (isSelected) FireballElectricLime else textColor,
            fontSize = 11.sp
        )
    }
}

private data class Quad<A, B, C, D>(val first: A, val second: B, val third: C, val fourth: D)
