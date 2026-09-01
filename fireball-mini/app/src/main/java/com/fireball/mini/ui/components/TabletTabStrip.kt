package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Folder
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.Public
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Work
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.Space
import com.fireball.mini.core.models.TabItem
import com.fireball.mini.core.models.TabSection
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

@Composable
fun TabletTabStrip(
    tabs: List<TabItem>,
    activeTabId: String?,
    activeSpace: Space?,
    isBurner: Boolean,
    onTabClick: (String) -> Unit,
    onTabClose: (String) -> Unit,
    onNewTabClick: () -> Unit,
    onSpaceClick: () -> Unit,
    engineType: com.fireball.mini.core.models.BrowserEngineType = com.fireball.mini.core.models.BrowserEngineType.NATIVE_WEBVIEW,
    onToggleEngine: () -> Unit = {},
    modifier: Modifier = Modifier
) {
    val scrollState = rememberScrollState()

    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = modifier
            .fillMaxWidth()
            .background(if (isBurner) Color(0xFF14121A) else FireballBackground)
            .statusBarsPadding()
            .padding(start = 8.dp, end = 8.dp, top = 4.dp, bottom = 0.dp)
    ) {
        // Space Selector Chip (e.g., [ 🪐 Main ] or [ 🕵️ Incognito ])
        activeSpace?.let { space ->
            Box(
                modifier = Modifier
                    .clip(RoundedCornerShape(8.dp))
                    .background(FireballDeepSurface)
                    .border(
                        1.dp,
                        try {
                            Color(android.graphics.Color.parseColor(space.accentColorHex)).copy(alpha = 0.6f)
                        } catch (e: Exception) {
                            FireballElectricLime.copy(alpha = 0.6f)
                        },
                        RoundedCornerShape(8.dp)
                    )
                    .clickable { onSpaceClick() }
                    .padding(horizontal = 10.dp, vertical = 6.dp)
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = getSpaceIcon(space.iconName),
                        contentDescription = space.name,
                        tint = try {
                            Color(android.graphics.Color.parseColor(space.accentColorHex))
                        } catch (e: Exception) {
                            FireballElectricLime
                        },
                        modifier = Modifier.size(14.dp)
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = space.name,
                        style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                        color = FireballPrimaryText,
                        fontSize = 11.sp
                    )
                }
            }
            Spacer(modifier = Modifier.width(8.dp))
        }

        // Horizontal Scrollable Tab Strip
        Row(
            verticalAlignment = Alignment.Bottom,
            modifier = Modifier
                .weight(1f)
                .horizontalScroll(scrollState)
        ) {
            for (tab in tabs) {
                val isActive = tab.id == activeTabId
                TabletTabItem(
                    tab = tab,
                    isActive = isActive,
                    isBurner = isBurner,
                    onClick = { onTabClick(tab.id) },
                    onClose = { onTabClose(tab.id) }
                )
                Spacer(modifier = Modifier.width(4.dp))
            }

            // New Tab "+" Button
            IconButton(
                onClick = onNewTabClick,
                modifier = Modifier
                    .size(32.dp)
                    .padding(bottom = 2.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Add,
                    contentDescription = "New Tab",
                    tint = FireballMutedText,
                    modifier = Modifier.size(18.dp)
                )
            }
        }

        // Engine Switcher Pill (e.g. [ ⚡ Stream ] vs [ 🌐 Native ])
        val isStream = engineType == com.fireball.mini.core.models.BrowserEngineType.FIREBALL_BEAM_STREAM
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(16.dp))
                .background(if (isStream) FireballElectricLime.copy(alpha = 0.15f) else FireballDeepSurface)
                .border(
                    1.dp,
                    if (isStream) FireballElectricLime else FireballBorder,
                    RoundedCornerShape(16.dp)
                )
                .clickable { onToggleEngine() }
                .padding(horizontal = 8.dp, vertical = 4.dp)
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(6.dp)
                        .clip(CircleShape)
                        .background(if (isStream) FireballElectricLime else FireballMeteorOrange)
                )
                Spacer(modifier = Modifier.width(4.dp))
                Text(
                    text = if (isStream) "⚡ STREAM" else "🌐 NATIVE",
                    color = if (isStream) FireballElectricLime else FireballMutedText,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.Bold
                )
            }
        }
    }
}


@Composable
private fun TabletTabItem(
    tab: TabItem,
    isActive: Boolean,
    isBurner: Boolean,
    onClick: () -> Unit,
    onClose: () -> Unit
) {
    val tabBackground = when {
        isActive && isBurner -> Color(0xFF1B1822)
        isActive -> FireballDeepSurface
        else -> FireballCardSurface.copy(alpha = 0.6f)
    }

    val tabBorderColor = when {
        isActive && isBurner -> FireballMeteorOrange.copy(alpha = 0.5f)
        isActive -> FireballBorder
        else -> Color.Transparent
    }

    Box(
        modifier = Modifier
            .widthIn(min = 120.dp, max = 220.dp)
            .height(36.dp)
            .clip(RoundedCornerShape(topStart = 10.dp, topEnd = 10.dp))
            .background(tabBackground)
            .border(1.dp, tabBorderColor, RoundedCornerShape(topStart = 10.dp, topEnd = 10.dp))
            .clickable { onClick() }
            .padding(horizontal = 10.dp),
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
                    imageVector = if (tab.section == TabSection.PINNED) Icons.Default.Language else Icons.Default.Public,
                    contentDescription = null,
                    tint = if (isActive) FireballElectricLime else FireballMutedText,
                    modifier = Modifier.size(14.dp)
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    text = tab.title.ifEmpty { "New Tab" },
                    style = MaterialTheme.typography.bodySmall.copy(
                        fontWeight = if (isActive) FontWeight.Bold else FontWeight.Normal
                    ),
                    color = if (isActive) FireballPrimaryText else FireballMutedText,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    fontSize = 12.sp
                )
            }

            // Tab Close (x) Button
            Box(
                modifier = Modifier
                    .size(24.dp)
                    .clip(CircleShape)
                    .clickable { onClose() },
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = Icons.Default.Close,
                    contentDescription = "Close Tab",
                    tint = if (isActive) FireballPrimaryText.copy(alpha = 0.85f) else FireballMutedText.copy(alpha = 0.6f),
                    modifier = Modifier.size(13.dp)
                )
            }
        }
    }
}

private fun getSpaceIcon(iconName: String): ImageVector {
    return when (iconName.lowercase()) {
        "work" -> Icons.Default.Work
        "folder" -> Icons.Default.Folder
        "security", "incognito" -> Icons.Default.Security
        else -> Icons.Default.Public
    }
}
