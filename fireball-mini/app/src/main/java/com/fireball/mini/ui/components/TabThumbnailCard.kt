package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.DriveFileMove
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.PushPin
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.VolumeUp
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.DiscardState
import com.fireball.mini.core.models.Space
import com.fireball.mini.core.models.TabItem
import com.fireball.mini.core.models.TabSection
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

@Composable
fun TabThumbnailCard(
    tab: TabItem,
    isActive: Boolean,
    allSpaces: List<Space> = emptyList(),
    onTabClick: () -> Unit,
    onCloseClick: () -> Unit,
    onTogglePin: () -> Unit = {},
    onToggleFavorite: () -> Unit = {},
    onDuplicateTab: () -> Unit = {},
    onMoveToSpace: (String) -> Unit = {},
    modifier: Modifier = Modifier
) {
    var showMenu by remember { mutableStateOf(false) }

    val borderColor = when {
        isActive -> FireballElectricLime
        tab.section == TabSection.FAVORITE -> FireballMeteorOrange
        tab.section == TabSection.PINNED -> FireballElectricLime.copy(alpha = 0.6f)
        else -> FireballBorder.copy(alpha = 0.7f)
    }

    val displayDomain = tab.url.removePrefix("https://").removePrefix("http://").substringBefore('/')
    val titleText = when {
        tab.title.isNotBlank() && tab.title != "about:blank" -> tab.title
        displayDomain.isNotBlank() && displayDomain != "about:blank" -> displayDomain
        else -> "New Tab"
    }

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(16.dp))
            .background(FireballCardSurface)
            .border(
                width = if (isActive) 1.8.dp else 1.dp,
                color = borderColor,
                shape = RoundedCornerShape(16.dp)
            )
            .clickable { onTabClick() }
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            // Header Bar
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier
                    .fillMaxWidth()
                    .background(if (isActive) FireballActiveSurface else FireballRaisedSurface)
                    .padding(horizontal = 8.dp, vertical = 6.dp)
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.weight(1f)
                ) {
                    Box(
                        modifier = Modifier
                            .size(22.dp)
                            .clip(CircleShape)
                            .background(FireballDeepSurface),
                        contentAlignment = Alignment.Center
                    ) {
                        if (tab.section == TabSection.FAVORITE) {
                            Icon(
                                imageVector = Icons.Default.Star,
                                contentDescription = "Favorite",
                                tint = FireballMeteorOrange,
                                modifier = Modifier.size(13.dp)
                            )
                        } else if (tab.section == TabSection.PINNED) {
                            Icon(
                                imageVector = Icons.Default.PushPin,
                                contentDescription = "Pinned",
                                tint = FireballElectricLime,
                                modifier = Modifier.size(13.dp)
                            )
                        } else {
                            Icon(
                                imageVector = Icons.Default.Language,
                                contentDescription = "Web",
                                tint = if (isActive) FireballElectricLime else FireballMutedText,
                                modifier = Modifier.size(13.dp)
                            )
                        }
                    }

                    Spacer(modifier = Modifier.width(8.dp))

                    Text(
                        text = titleText,
                        style = MaterialTheme.typography.bodyMedium.copy(
                            fontWeight = if (isActive) FontWeight.Bold else FontWeight.Medium
                        ),
                        color = if (isActive) FireballPrimaryText else FireballPrimaryText.copy(alpha = 0.85f),
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis,
                        fontSize = 12.sp
                    )
                }

                Row(verticalAlignment = Alignment.CenterVertically) {
                    // 3-Dot Quick Menu
                    Box {
                        IconButton(
                            onClick = { showMenu = true },
                            modifier = Modifier.size(24.dp)
                        ) {
                            Icon(
                                imageVector = Icons.Default.MoreVert,
                                contentDescription = "Options",
                                tint = FireballMutedText,
                                modifier = Modifier.size(15.dp)
                            )
                        }

                        DropdownMenu(
                            expanded = showMenu,
                            onDismissRequest = { showMenu = false },
                            modifier = Modifier.background(FireballDeepSurface)
                        ) {
                            DropdownMenuItem(
                                text = {
                                    Text(
                                        text = if (tab.section == TabSection.PINNED) "Unpin Tab" else "Pin Tab",
                                        color = FireballPrimaryText,
                                        fontSize = 13.sp
                                    )
                                },
                                leadingIcon = {
                                    Icon(
                                        imageVector = Icons.Default.PushPin,
                                        contentDescription = null,
                                        tint = FireballElectricLime,
                                        modifier = Modifier.size(16.dp)
                                    )
                                },
                                onClick = { onTogglePin(); showMenu = false }
                            )

                            DropdownMenuItem(
                                text = {
                                    Text(
                                        text = if (tab.section == TabSection.FAVORITE) "Remove Favorite" else "Add to Favorites",
                                        color = FireballPrimaryText,
                                        fontSize = 13.sp
                                    )
                                },
                                leadingIcon = {
                                    Icon(
                                        imageVector = Icons.Default.Star,
                                        contentDescription = null,
                                        tint = FireballMeteorOrange,
                                        modifier = Modifier.size(16.dp)
                                    )
                                },
                                onClick = { onToggleFavorite(); showMenu = false }
                            )

                            DropdownMenuItem(
                                text = {
                                    Text(
                                        text = "Duplicate Tab",
                                        color = FireballPrimaryText,
                                        fontSize = 13.sp
                                    )
                                },
                                leadingIcon = {
                                    Icon(
                                        imageVector = Icons.Default.ContentCopy,
                                        contentDescription = null,
                                        tint = FireballPrimaryText,
                                        modifier = Modifier.size(16.dp)
                                    )
                                },
                                onClick = { onDuplicateTab(); showMenu = false }
                            )

                            if (allSpaces.size > 1) {
                                HorizontalDivider(color = FireballBorder)
                                allSpaces.filter { it.id != tab.spaceId }.forEach { space ->
                                    DropdownMenuItem(
                                        text = {
                                            Text(
                                                text = "Move to ${space.name}",
                                                color = FireballPrimaryText,
                                                fontSize = 13.sp
                                            )
                                        },
                                        leadingIcon = {
                                            Icon(
                                                imageVector = Icons.Default.DriveFileMove,
                                                contentDescription = null,
                                                tint = try {
                                                    Color(android.graphics.Color.parseColor(space.accentColorHex))
                                                } catch (_: Exception) {
                                                    FireballElectricLime
                                                },
                                                modifier = Modifier.size(16.dp)
                                            )
                                        },
                                        onClick = { onMoveToSpace(space.id); showMenu = false }
                                    )
                                }
                            }
                        }
                    }

                    // Close Tab Button (Clear Tap Target)
                    IconButton(
                        onClick = onCloseClick,
                        modifier = Modifier.size(24.dp)
                    ) {
                        Icon(
                            imageVector = Icons.Default.Close,
                            contentDescription = "Close Tab",
                            tint = FireballMutedText,
                            modifier = Modifier.size(16.dp)
                        )
                    }
                }
            }

            // Preview Stage (Center Canvas with Domain Emblem)
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .aspectRatio(1.25f)
                    .background(
                        Brush.verticalGradient(
                            colors = listOf(
                                FireballDeepSurface,
                                FireballCardSurface
                            )
                        )
                    )
                    .padding(horizontal = 8.dp, vertical = 10.dp),
                contentAlignment = Alignment.Center
            ) {
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(10.dp))
                        .background(FireballRaisedSurface)
                        .border(1.dp, FireballBorder.copy(alpha = 0.6f), RoundedCornerShape(10.dp))
                        .padding(horizontal = 12.dp, vertical = 6.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = if (displayDomain.isNotBlank() && displayDomain != "about:blank") displayDomain else "duckduckgo.com",
                        style = MaterialTheme.typography.labelMedium.copy(fontWeight = FontWeight.SemiBold),
                        color = if (isActive) FireballElectricLime else FireballPrimaryText,
                        fontSize = 12.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                }
            }

            // Bottom Status Bar (Footer: Active / Inactive / RAM Discarded indicator)
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier
                    .fillMaxWidth()
                    .background(FireballRaisedSurface)
                    .border(0.5.dp, FireballBorder.copy(alpha = 0.5f))
                    .padding(horizontal = 8.dp, vertical = 6.dp)
            ) {
                // Left: Audio indicator or Section name
                Row(verticalAlignment = Alignment.CenterVertically) {
                    if (tab.isAudible) {
                        Icon(
                            imageVector = Icons.Default.VolumeUp,
                            contentDescription = "Audio playing",
                            tint = FireballElectricLime,
                            modifier = Modifier.size(13.dp)
                        )
                        Spacer(modifier = Modifier.width(4.dp))
                    }

                    Text(
                        text = when (tab.section) {
                            TabSection.FAVORITE -> "Favorite"
                            TabSection.PINNED -> "Pinned"
                            TabSection.TODAY -> "Tab"
                        },
                        style = MaterialTheme.typography.labelSmall,
                        color = FireballMutedText,
                        fontSize = 10.sp
                    )
                }

                // Right: Active / Inactive / RAM Discarded indicator
                if (tab.discardState == DiscardState.DISCARDED) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(FireballBorder)
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "💤 RAM SAVED",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = FireballElectricLime,
                            fontSize = 9.sp
                        )
                    }
                } else if (isActive) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(FireballElectricLime.copy(alpha = 0.2f))
                            .border(0.8.dp, FireballElectricLime.copy(alpha = 0.8f), RoundedCornerShape(4.dp))
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "● ACTIVE",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = FireballElectricLime,
                            fontSize = 9.sp
                        )
                    }
                } else {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(FireballDeepSurface)
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "○ INACTIVE",
                            style = MaterialTheme.typography.labelSmall,
                            color = FireballMutedText,
                            fontSize = 9.sp
                        )
                    }
                }
            }
        }
    }
}
