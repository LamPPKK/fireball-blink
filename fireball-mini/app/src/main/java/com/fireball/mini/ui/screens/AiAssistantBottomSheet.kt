package com.fireball.mini.ui.screens

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.widget.Toast
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Chat
import androidx.compose.material.icons.automirrored.filled.Send
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Book
import androidx.compose.material.icons.filled.Chat
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Psychology
import androidx.compose.material.icons.filled.RecordVoiceOver
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.SmartToy
import androidx.compose.material.icons.filled.Timer
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SheetState
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.AiChatMessage
import com.fireball.mini.core.models.AiMessageSender
import com.fireball.mini.core.models.PageSummary
import com.fireball.mini.ui.components.AdaptiveDialogContainer
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.theme.FireballSecondaryText
import com.fireball.mini.ui.viewmodels.BrowserViewModel
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AiAssistantBottomSheet(
    viewModel: BrowserViewModel,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    onDismiss: () -> Unit,
    onOpenReaderMode: () -> Unit
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    val currentArticle by viewModel.currentArticle.collectAsState()
    val summary by viewModel.aiSummary.collectAsState()
    val isGeneratingSummary by viewModel.isGeneratingSummary.collectAsState()
    val chatMessages by viewModel.aiChatMessages.collectAsState()
    val isChatLoading by viewModel.isAiChatLoading.collectAsState()

    var selectedTab by remember { mutableIntStateOf(0) } // 0: Summary, 1: Chat
    var inputQuery by remember { mutableStateOf("") }
    val chatListState = rememberLazyListState()

    // Trigger initial summary if not present
    LaunchedEffect(currentArticle) {
        if (summary == null && currentArticle != null) {
            viewModel.generateAiSummary()
        }
    }

    LaunchedEffect(chatMessages.size) {
        if (chatMessages.isNotEmpty()) {
            chatListState.animateScrollToItem(chatMessages.size - 1)
        }
    }

    AdaptiveDialogContainer(
        onDismiss = onDismiss,
        sheetState = sheetState
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
                .padding(horizontal = 20.dp, vertical = 12.dp)
        ) {
            // Drag Handle / Dialog Header
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 12.dp),
                contentAlignment = Alignment.Center
            ) {
                Box(
                    modifier = Modifier
                        .size(width = 40.dp, height = 4.dp)
                        .clip(RoundedCornerShape(2.dp))
                        .background(FireballBorder)
                )
            }

            // Header with AI Sparkle Gradient
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.weight(1f, fill = false)
                ) {
                    Box(
                        modifier = Modifier
                            .size(36.dp)
                            .clip(RoundedCornerShape(10.dp))
                            .background(
                                Brush.linearGradient(
                                    listOf(FireballElectricLime.copy(alpha = 0.8f), FireballMeteorOrange.copy(alpha = 0.8f))
                                )
                            ),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.AutoAwesome,
                            contentDescription = "Fireball AI",
                            tint = FireballBackground,
                            modifier = Modifier.size(20.dp)
                        )
                    }

                    Spacer(modifier = Modifier.width(10.dp))

                    Column {
                        Text(
                            text = "Fireball AI",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.ExtraBold),
                            color = FireballPrimaryText,
                            fontSize = 15.5.sp,
                            maxLines = 1
                        )
                        Text(
                            text = currentArticle?.domain ?: "Page Context",
                            style = MaterialTheme.typography.bodySmall,
                            color = FireballElectricLime,
                            fontSize = 11.5.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }

                Spacer(modifier = Modifier.width(8.dp))

                // Tab Switcher Pill: [ ✨ Tóm tắt ] [ 💬 Chat ]
                Row(
                    modifier = Modifier
                        .clip(RoundedCornerShape(20.dp))
                        .background(FireballRaisedSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(20.dp))
                        .padding(3.dp)
                ) {
                    AiTabPill(
                        title = "Tóm tắt",
                        icon = Icons.Default.Psychology,
                        isSelected = selectedTab == 0,
                        onClick = { selectedTab = 0 }
                    )
                    AiTabPill(
                        title = "Chat",
                        icon = Icons.AutoMirrored.Filled.Chat,
                        isSelected = selectedTab == 1,
                        onClick = { selectedTab = 1 }
                    )
                }
            }

            Spacer(modifier = Modifier.height(14.dp))

            // Body Content based on active Tab
            if (selectedTab == 0) {
                // SUMMARY TAB
                if (isGeneratingSummary) {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(260.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Column(horizontalAlignment = Alignment.CenterHorizontally) {
                            CircularProgressIndicator(
                                color = FireballElectricLime,
                                modifier = Modifier.size(36.dp)
                            )
                            Spacer(modifier = Modifier.height(12.dp))
                            Text(
                                text = "Đang trích xuất DOM & phân tích nội dung...",
                                style = MaterialTheme.typography.bodySmall,
                                color = FireballSecondaryText
                            )
                        }
                    }
                } else if (summary != null) {
                    SummaryView(
                        summary = summary!!,
                        onCopy = {
                            val textToCopy = "📌 ${summary!!.title}\n\n${summary!!.briefOverview}\n\nÝ chính:\n" +
                                    summary!!.keyTakeaways.joinToString("\n") { "• $it" }
                            val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                            clipboard.setPrimaryClip(ClipData.newPlainText("Summary", textToCopy))
                            Toast.makeText(context, "Đã sao chép tóm tắt!", Toast.LENGTH_SHORT).show()
                        },
                        onReadAloud = {
                            viewModel.playArticleTts()
                            Toast.makeText(context, "Đang phát âm thanh giọng đọc...", Toast.LENGTH_SHORT).show()
                        },
                        onOpenReader = {
                            onDismiss()
                            onOpenReaderMode()
                        }
                    )
                } else {
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(200.dp),
                        contentAlignment = Alignment.Center
                    ) {
                        Text(
                            text = "Chưa có nội dung bài viết để tóm tắt.",
                            color = FireballMutedText,
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
            } else {
                // CHAT WITH PAGE TAB
                ChatWithPageView(
                    chatMessages = chatMessages,
                    isChatLoading = isChatLoading,
                    inputQuery = inputQuery,
                    chatListState = chatListState,
                    onInputChange = { inputQuery = it },
                    onSend = { query ->
                        if (query.isNotBlank()) {
                            viewModel.sendAiChatMessage(query)
                            inputQuery = ""
                        }
                    }
                )
            }
        }
    }
}

@Composable
private fun SummaryView(
    summary: PageSummary,
    onCopy: () -> Unit,
    onReadAloud: () -> Unit,
    onOpenReader: () -> Unit
) {
    LazyColumn(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(max = 500.dp)
    ) {
        // Meta Badges
        item {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 10.dp)
            ) {
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(FireballElectricLime.copy(alpha = 0.15f))
                        .border(1.dp, FireballElectricLime.copy(alpha = 0.3f), RoundedCornerShape(6.dp))
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Default.Timer,
                            contentDescription = null,
                            tint = FireballElectricLime,
                            modifier = Modifier.size(12.dp)
                        )
                        Spacer(modifier = Modifier.width(4.dp))
                        Text(
                            text = "~${summary.estimatedReadTimeMinutes} phút đọc",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = FireballElectricLime,
                            fontSize = 11.sp
                        )
                    }
                }

                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(6.dp))
                        .background(FireballCardSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(6.dp))
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        text = summary.sentiment,
                        style = MaterialTheme.typography.labelSmall,
                        color = FireballSecondaryText,
                        fontSize = 11.sp
                    )
                }
            }
        }

        // Brief Overview
        item {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp))
                    .background(FireballCardSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(14.dp))
                    .padding(14.dp)
            ) {
                Text(
                    text = summary.briefOverview,
                    style = MaterialTheme.typography.bodyMedium,
                    color = FireballPrimaryText,
                    lineHeight = 20.sp,
                    fontSize = 13.sp
                )
            }
            Spacer(modifier = Modifier.height(12.dp))
        }

        // Key Takeaways Headline
        item {
            Text(
                text = "📌 ĐIỂM CỐT LÕI (KEY TAKEAWAYS)",
                style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.Bold),
                color = FireballElectricLime,
                fontSize = 11.sp,
                modifier = Modifier.padding(bottom = 6.dp)
            )
        }

        // Bullet points
        items(summary.keyTakeaways) { point ->
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 4.dp)
            ) {
                Text(
                    text = "•",
                    style = MaterialTheme.typography.bodyLarge.copy(fontWeight = FontWeight.Bold),
                    color = FireballMeteorOrange,
                    modifier = Modifier.padding(end = 8.dp)
                )
                Text(
                    text = point,
                    style = MaterialTheme.typography.bodyMedium,
                    color = FireballSecondaryText,
                    fontSize = 13.sp,
                    lineHeight = 19.sp
                )
            }
        }

        // Action Toolbar (Copy, TTS, Reader Mode)
        item {
            Spacer(modifier = Modifier.height(16.dp))
            Row(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                ActionChipButton(
                    title = "Sao chép",
                    icon = Icons.Default.ContentCopy,
                    onClick = onCopy,
                    modifier = Modifier.weight(1f)
                )
                ActionChipButton(
                    title = "Đọc bằng giọng nói",
                    icon = Icons.Default.RecordVoiceOver,
                    onClick = onReadAloud,
                    modifier = Modifier.weight(1.3f)
                )
                ActionChipButton(
                    title = "Reader Mode",
                    icon = Icons.Default.Book,
                    onClick = onOpenReader,
                    modifier = Modifier.weight(1.2f)
                )
            }
            Spacer(modifier = Modifier.height(10.dp))
        }
    }
}

@Composable
private fun ChatWithPageView(
    chatMessages: List<AiChatMessage>,
    isChatLoading: Boolean,
    inputQuery: String,
    chatListState: androidx.compose.foundation.lazy.LazyListState,
    onInputChange: (String) -> Unit,
    onSend: (String) -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .height(420.dp)
    ) {
        // Quick Prompt Chips
        val promptChips = listOf(
            "📌 Tóm tắt 3 ý quan trọng",
            "🎯 Bài viết nói về gì?",
            "✍️ Ai là tác giả?",
            "🌐 Dịch sang Tiếng Việt"
        )
        val chipScrollState = rememberScrollState()

        Row(
            modifier = Modifier
                .fillMaxWidth()
                .horizontalScroll(chipScrollState)
                .padding(bottom = 8.dp),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            for (prompt in promptChips) {
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(16.dp))
                        .background(FireballCardSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(16.dp))
                        .clickable { onSend(prompt) }
                        .padding(horizontal = 10.dp, vertical = 6.dp)
                ) {
                    Text(
                        text = prompt,
                        style = MaterialTheme.typography.labelSmall,
                        color = FireballSecondaryText,
                        fontSize = 11.sp
                    )
                }
            }
        }

        // Chat Message Stream
        LazyColumn(
            state = chatListState,
            modifier = Modifier
                .weight(1f)
                .fillMaxWidth(),
            contentPadding = PaddingValues(vertical = 4.dp)
        ) {
            items(chatMessages) { message ->
                val isUser = message.sender == AiMessageSender.USER
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 4.dp),
                    horizontalArrangement = if (isUser) Arrangement.End else Arrangement.Start
                ) {
                    if (!isUser) {
                        Icon(
                            imageVector = Icons.Default.SmartToy,
                            contentDescription = "AI",
                            tint = FireballElectricLime,
                            modifier = Modifier
                                .size(24.dp)
                                .padding(end = 4.dp, top = 4.dp)
                        )
                    }

                    Box(
                        modifier = Modifier
                            .fillMaxWidth(0.85f)
                            .clip(
                                RoundedCornerShape(
                                    topStart = 14.dp,
                                    topEnd = 14.dp,
                                    bottomStart = if (isUser) 14.dp else 2.dp,
                                    bottomEnd = if (isUser) 2.dp else 14.dp
                                )
                            )
                            .background(if (isUser) FireballActiveSurface else FireballCardSurface)
                            .border(
                                1.dp,
                                if (isUser) FireballElectricLime.copy(alpha = 0.6f) else FireballBorder,
                                RoundedCornerShape(14.dp)
                            )
                            .padding(12.dp)
                    ) {
                        Text(
                            text = message.text,
                            style = MaterialTheme.typography.bodyMedium,
                            color = FireballPrimaryText,
                            fontSize = 13.sp,
                            lineHeight = 19.sp
                        )
                    }
                }
            }

            if (isChatLoading) {
                item {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        modifier = Modifier.padding(start = 8.dp, top = 6.dp)
                    ) {
                        CircularProgressIndicator(
                            color = FireballElectricLime,
                            modifier = Modifier.size(16.dp),
                            strokeWidth = 2.dp
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "AI đang suy nghĩ...",
                            style = MaterialTheme.typography.bodySmall,
                            color = FireballMutedText,
                            fontSize = 11.sp
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(8.dp))

        // Chat Input Box
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .height(46.dp)
                .clip(RoundedCornerShape(24.dp))
                .background(FireballRaisedSurface)
                .border(1.dp, FireballBorder, RoundedCornerShape(24.dp))
                .padding(start = 14.dp, end = 4.dp)
        ) {
            BasicTextField(
                value = inputQuery,
                onValueChange = onInputChange,
                modifier = Modifier.weight(1f),
                textStyle = MaterialTheme.typography.bodyMedium.copy(color = FireballPrimaryText, fontSize = 13.sp),
                cursorBrush = SolidColor(FireballElectricLime),
                singleLine = true,
                keyboardOptions = KeyboardOptions(imeAction = ImeAction.Send),
                keyboardActions = KeyboardActions(onSend = { onSend(inputQuery) }),
                decorationBox = { innerTextField ->
                    if (inputQuery.isEmpty()) {
                        Text(
                            text = "Hỏi bất kỳ điều gì về trang web này...",
                            color = FireballMutedText,
                            fontSize = 13.sp
                        )
                    }
                    innerTextField()
                }
            )

            IconButton(
                onClick = { onSend(inputQuery) },
                enabled = inputQuery.isNotBlank() && !isChatLoading,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.Send,
                    contentDescription = "Send",
                    tint = if (inputQuery.isNotBlank()) FireballElectricLime else FireballMutedText,
                    modifier = Modifier.size(18.dp)
                )
            }
        }
    }
}

@Composable
private fun AiTabPill(
    title: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Box(
        modifier = Modifier
            .clip(RoundedCornerShape(16.dp))
            .background(if (isSelected) FireballActiveSurface else Color.Transparent)
            .border(
                1.dp,
                if (isSelected) FireballElectricLime else Color.Transparent,
                RoundedCornerShape(16.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 12.dp, vertical = 6.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = if (isSelected) FireballElectricLime else FireballMutedText,
                modifier = Modifier.size(13.dp)
            )
            Spacer(modifier = Modifier.width(4.dp))
            Text(
                text = title,
                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                color = if (isSelected) FireballElectricLime else FireballMutedText,
                fontSize = 11.sp,
                maxLines = 1,
                softWrap = false
            )
        }
    }
}

@Composable
private fun ActionChipButton(
    title: String,
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(10.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(10.dp))
            .clickable { onClick() }
            .padding(horizontal = 8.dp, vertical = 8.dp),
        contentAlignment = Alignment.Center
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = FireballPrimaryText,
                modifier = Modifier.size(14.dp)
            )
            Spacer(modifier = Modifier.width(4.dp))
            Text(
                text = title,
                style = MaterialTheme.typography.labelSmall,
                color = FireballPrimaryText,
                fontSize = 11.sp,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }
    }
}
