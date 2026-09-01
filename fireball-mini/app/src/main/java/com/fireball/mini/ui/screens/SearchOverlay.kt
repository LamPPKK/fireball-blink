package com.fireball.mini.ui.screens

import androidx.compose.animation.AnimatedVisibility
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
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ContentPaste
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Public
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
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
import com.fireball.mini.ui.theme.FireballSearchSurface
import com.fireball.mini.ui.theme.FireballSecondaryText

data class QuickSiteItem(
    val title: String,
    val url: String,
    val iconEmoji: String,
    val accentColor: Color
)

@Composable
fun SearchOverlay(
    initialText: String,
    isBurner: Boolean,
    onClose: () -> Unit,
    onSubmit: (String) -> Unit
) {
    var searchQuery by remember {
        mutableStateOf(if (initialText == "about:blank" || initialText.startsWith("https://duckduckgo.com")) "" else initialText)
    }
    val focusRequester = remember { FocusRequester() }
    val clipboardManager = LocalClipboardManager.current

    val quickSites = listOf(
        QuickSiteItem("DuckDuckGo", "https://duckduckgo.com", "🦆", FireballMeteorOrange),
        QuickSiteItem("Google", "https://www.google.com", "🔍", Color(0xFF4285F4)),
        QuickSiteItem("GitHub", "https://github.com", "🐙", FireballPrimaryText),
        QuickSiteItem("YouTube", "https://www.youtube.com", "▶️", Color(0xFFFF0000)),
        QuickSiteItem("Reddit", "https://www.reddit.com", "🤖", Color(0xFFFF4500)),
        QuickSiteItem("Wikipedia", "https://www.wikipedia.org", "📚", Color(0xFF6B7A6C)),
        QuickSiteItem("Hacker News", "https://news.ycombinator.com", "📰", FireballMeteorOrange),
        QuickSiteItem("X / Twitter", "https://x.com", "✖️", FireballPrimaryText)
    )

    LaunchedEffect(Unit) {
        focusRequester.requestFocus()
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .background(FireballBackground)
            .statusBarsPadding()
            .navigationBarsPadding()
    ) {
        // Search Input Header
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .background(FireballDeepSurface)
                .border(1.dp, FireballBorder)
                .padding(horizontal = 8.dp, vertical = 10.dp)
        ) {
            IconButton(onClick = onClose, modifier = Modifier.size(36.dp)) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                    contentDescription = "Cancel",
                    tint = FireballPrimaryText,
                    modifier = Modifier.size(20.dp)
                )
            }

            Spacer(modifier = Modifier.width(4.dp))

            // Search Bar Field
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(44.dp)
                    .clip(RoundedCornerShape(12.dp))
                    .background(FireballSearchSurface)
                    .border(
                        1.dp,
                        if (isBurner) FireballMeteorOrange else FireballElectricLime,
                        RoundedCornerShape(12.dp)
                    )
                    .padding(horizontal = 12.dp),
                contentAlignment = Alignment.CenterStart
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Icon(
                        imageVector = Icons.Default.Search,
                        contentDescription = "Search",
                        tint = if (isBurner) FireballMeteorOrange else FireballElectricLime,
                        modifier = Modifier.size(18.dp)
                    )

                    Spacer(modifier = Modifier.width(8.dp))

                    BasicTextField(
                        value = searchQuery,
                        onValueChange = { searchQuery = it },
                        modifier = Modifier
                            .weight(1f)
                            .focusRequester(focusRequester),
                        singleLine = true,
                        textStyle = MaterialTheme.typography.bodyLarge.copy(
                            color = FireballPrimaryText,
                            fontSize = 15.sp
                        ),
                        cursorBrush = SolidColor(if (isBurner) FireballMeteorOrange else FireballElectricLime),
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Uri,
                            imeAction = ImeAction.Go
                        ),
                        keyboardActions = KeyboardActions(onGo = {
                            if (searchQuery.isNotBlank()) onSubmit(searchQuery)
                        }),
                        decorationBox = { innerTextField ->
                            if (searchQuery.isEmpty()) {
                                Text(
                                    text = "Search or type URL...",
                                    style = MaterialTheme.typography.bodyMedium,
                                    color = FireballMutedText,
                                    fontSize = 14.sp
                                )
                            }
                            innerTextField()
                        }
                    )

                    if (searchQuery.isNotEmpty()) {
                        IconButton(
                            onClick = { searchQuery = "" },
                            modifier = Modifier.size(28.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.Close,
                                contentDescription = "Clear",
                                tint = FireballMutedText,
                                modifier = Modifier.size(16.dp)
                            )
                        }
                    } else {
                        // Paste from clipboard button
                        IconButton(
                            onClick = {
                                val clip = clipboardManager.getText()?.text
                                if (!clip.isNullOrBlank()) {
                                    searchQuery = clip
                                }
                            },
                            modifier = Modifier.size(28.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.ContentPaste,
                                contentDescription = "Paste",
                                tint = FireballMutedText,
                                modifier = Modifier.size(16.dp)
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.width(8.dp))

            // Action "GO" button
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(10.dp))
                    .background(if (searchQuery.isNotBlank()) (if (isBurner) FireballMeteorOrange else FireballElectricLime) else FireballRaisedSurface)
                    .clickable(enabled = searchQuery.isNotBlank()) { onSubmit(searchQuery) }
                    .padding(horizontal = 14.dp, vertical = 10.dp)
            ) {
                Text(
                    text = "Go",
                    style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.Bold),
                    color = if (searchQuery.isNotBlank()) FireballBackground else FireballMutedText,
                    fontSize = 13.sp
                )
            }
        }

        Spacer(modifier = Modifier.height(16.dp))

        // Quick Sites Grid
        Column(modifier = Modifier.fillMaxWidth().padding(horizontal = 16.dp)) {
            Text(
                text = "TOP SITES & SHORTCUTS",
                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.SemiBold),
                color = FireballSecondaryText,
                fontSize = 11.sp,
                modifier = Modifier.padding(bottom = 12.dp)
            )

            LazyVerticalGrid(
                columns = GridCells.Adaptive(minSize = 90.dp),
                horizontalArrangement = Arrangement.spacedBy(10.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp)
            ) {
                items(quickSites) { site ->
                    Column(
                        horizontalAlignment = Alignment.CenterHorizontally,
                        modifier = Modifier
                            .clip(RoundedCornerShape(12.dp))
                            .background(FireballCardSurface)
                            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
                            .clickable { onSubmit(site.url) }
                            .padding(vertical = 12.dp, horizontal = 4.dp)
                    ) {
                        Box(
                            modifier = Modifier
                                .size(42.dp)
                                .clip(CircleShape)
                                .background(FireballRaisedSurface)
                                .border(1.dp, site.accentColor.copy(alpha = 0.4f), CircleShape),
                            contentAlignment = Alignment.Center
                        ) {
                            Text(text = site.iconEmoji, fontSize = 20.sp)
                        }

                        Spacer(modifier = Modifier.height(6.dp))

                        Text(
                            text = site.title,
                            style = MaterialTheme.typography.bodySmall,
                            color = FireballPrimaryText,
                            fontSize = 11.sp,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(24.dp))

        // Search engine indicator
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.Center,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 16.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Lock,
                contentDescription = "Shields Protected",
                tint = FireballElectricLime,
                modifier = Modifier.size(14.dp)
            )
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = "Fireball Shields Active • Tracking Parameters Stripped",
                style = MaterialTheme.typography.bodySmall,
                color = FireballMutedText,
                fontSize = 11.sp
            )
        }
    }
}
