package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.StarBorder
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue

@Composable
fun ChromiumTopToolbar(
    urlText: String,
    isLoading: Boolean,
    pageProgress: Int,
    isBurner: Boolean,
    tabCount: Int,
    adsBlockedCount: Long,
    isTabletLayout: Boolean = false,
    canGoBack: Boolean = true,
    canGoForward: Boolean = false,
    isBookmarked: Boolean = false,
    onBackClick: () -> Unit = {},
    onForwardClick: () -> Unit = {},
    onHomeClick: () -> Unit,
    onOmniboxClick: () -> Unit,
    onSwipeNextTab: () -> Unit = {},
    onSwipePrevTab: () -> Unit = {},
    onReload: () -> Unit,
    onShieldsClick: () -> Unit,
    onTabsClick: () -> Unit,
    onBookmarkClick: () -> Unit = {},
    onAiClick: () -> Unit = {},
    onMenuClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    val displayDomain = when {
        urlText.isEmpty() || urlText == "about:blank" -> "Search or type URL"
        urlText.startsWith("https://") -> urlText.removePrefix("https://").substringBefore('/')
        urlText.startsWith("http://") -> urlText.removePrefix("http://").substringBefore('/')
        else -> urlText
    }

    val isSecure = urlText.startsWith("https://")

    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(if (isBurner) Color(0xFF1B1822) else FireballDeepSurface)
            .then(if (!isTabletLayout) Modifier.statusBarsPadding() else Modifier)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = if (isTabletLayout) 12.dp else 4.dp, vertical = 6.dp)
        ) {
            // Navigation Cluster on Tablet / Desktop
            if (isTabletLayout) {
                IconButton(
                    onClick = onBackClick,
                    enabled = canGoBack,
                    modifier = Modifier.size(36.dp)
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                        contentDescription = "Back",
                        tint = if (canGoBack) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f),
                        modifier = Modifier.size(20.dp)
                    )
                }

                IconButton(
                    onClick = onForwardClick,
                    enabled = canGoForward,
                    modifier = Modifier.size(36.dp)
                ) {
                    Icon(
                        imageVector = Icons.AutoMirrored.Filled.ArrowForward,
                        contentDescription = "Forward",
                        tint = if (canGoForward) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f),
                        modifier = Modifier.size(20.dp)
                    )
                }

                IconButton(
                    onClick = onReload,
                    modifier = Modifier.size(36.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Refresh,
                        contentDescription = "Reload",
                        tint = FireballPrimaryText,
                        modifier = Modifier.size(20.dp)
                    )
                }

                IconButton(
                    onClick = onHomeClick,
                    modifier = Modifier.size(36.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Home,
                        contentDescription = "Home",
                        tint = if (isBurner) FireballMeteorOrange else FireballPrimaryText,
                        modifier = Modifier.size(20.dp)
                    )
                }

                Spacer(modifier = Modifier.width(6.dp))
            } else {
                // Home / Logo Icon on Phone
                IconButton(
                    onClick = onHomeClick,
                    modifier = Modifier.size(40.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Home,
                        contentDescription = "Home",
                        tint = if (isBurner) FireballMeteorOrange else FireballPrimaryText,
                        modifier = Modifier.size(22.dp)
                    )
                }
            }

            var totalDragX by remember { mutableFloatStateOf(0f) }

            // Omnibox Capsule (Pure Chromium Pill)
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(42.dp)
                    .clip(RoundedCornerShape(24.dp))
                    .background(if (isBurner) Color(0xFF2B2538) else FireballRaisedSurface)
                    .border(
                        1.dp,
                        if (isBurner) FireballMeteorOrange.copy(alpha = 0.5f) else FireballBorder,
                        RoundedCornerShape(24.dp)
                    )
                    .pointerInput(Unit) {
                        detectHorizontalDragGestures(
                            onDragStart = { totalDragX = 0f },
                            onDragEnd = {
                                if (totalDragX > 80f) {
                                    onSwipePrevTab()
                                } else if (totalDragX < -80f) {
                                    onSwipeNextTab()
                                }
                                totalDragX = 0f
                            },
                            onDragCancel = { totalDragX = 0f },
                            onHorizontalDrag = { _, dragAmount ->
                                totalDragX += dragAmount
                            }
                        )
                    }
                    .clickable { onOmniboxClick() }
                    .padding(horizontal = 12.dp),
                contentAlignment = Alignment.CenterStart
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
                        Icon(
                            imageVector = if (isSecure) Icons.Filled.Lock else Icons.Filled.Search,
                            contentDescription = "Security",
                            tint = if (isSecure) FireballElectricLime else FireballMutedText,
                            modifier = Modifier.size(15.dp)
                        )

                        Spacer(modifier = Modifier.width(8.dp))

                        Text(
                            text = displayDomain,
                            style = MaterialTheme.typography.bodyMedium.copy(
                                fontWeight = if (displayDomain.contains('.')) FontWeight.SemiBold else FontWeight.Normal
                            ),
                            color = if (urlText.isEmpty() || urlText == "about:blank") FireballMutedText else FireballPrimaryText,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis,
                            fontSize = 14.sp
                        )
                    }

                    // On Tablet: Bookmark star inside Omnibox
                    if (isTabletLayout) {
                        Icon(
                            imageVector = if (isBookmarked) Icons.Default.Star else Icons.Default.StarBorder,
                            contentDescription = "Bookmark",
                            tint = if (isBookmarked) Color(0xFFFFD700) else FireballMutedText,
                            modifier = Modifier
                                .size(18.dp)
                                .clickable { onBookmarkClick() }
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                    }

                    if (urlText.isNotEmpty() && urlText != "about:blank") {
                        Icon(
                            imageVector = Icons.Default.Refresh,
                            contentDescription = "Reload",
                            tint = FireballMutedText,
                            modifier = Modifier
                                .size(16.dp)
                                .clickable { onReload() }
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.width(4.dp))

            // AI Sparkle Assistant Button
            IconButton(
                onClick = onAiClick,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.AutoAwesome,
                    contentDescription = "Fireball AI",
                    tint = FireballElectricLime,
                    modifier = Modifier.size(20.dp)
                )
            }

            // Shields Icon
            IconButton(
                onClick = onShieldsClick,
                modifier = Modifier.size(38.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Shield,
                    contentDescription = "Shields",
                    tint = if (adsBlockedCount > 0) FireballMeteorOrange else FireballPrimaryText.copy(alpha = 0.85f),
                    modifier = Modifier.size(21.dp)
                )
            }

            // Tab Switcher [ 1 ]
            Box(
                modifier = Modifier
                    .size(32.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(if (isBurner) Color(0xFF2B2538) else FireballRaisedSurface)
                    .border(
                        1.5.dp,
                        if (isBurner) FireballMeteorOrange else FireballPrimaryText.copy(alpha = 0.7f),
                        RoundedCornerShape(8.dp)
                    )
                    .clickable { onTabsClick() },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = tabCount.toString(),
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.ExtraBold),
                    color = if (isBurner) FireballMeteorOrange else FireballPrimaryText,
                    fontSize = 12.sp
                )
            }

            Spacer(modifier = Modifier.width(2.dp))

            // Chromium 3-Dot Menu
            IconButton(
                onClick = onMenuClick,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.MoreVert,
                    contentDescription = "More options",
                    tint = FireballPrimaryText,
                    modifier = Modifier.size(22.dp)
                )
            }
        }

        // Slim Progress Bar attached underneath top toolbar
        if (isLoading) {
            LinearProgressIndicator(
                progress = { pageProgress / 100f },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(2.5.dp),
                color = if (isBurner) FireballMeteorOrange else Color(0xFF4285F4),
                trackColor = Color.Transparent
            )
        } else {
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(1.dp)
                    .background(FireballBorder)
            )
        }
    }
}
