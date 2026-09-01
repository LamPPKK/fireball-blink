package com.fireball.mini.ui.screens

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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Book
import androidx.compose.material.icons.filled.BookmarkBorder
import androidx.compose.material.icons.filled.CleaningServices
import androidx.compose.material.icons.filled.DesktopWindows
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.FindInPage
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Share
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SheetState
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MenuBottomSheet(
    currentUrl: String,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    onDismiss: () -> Unit,
    onReload: () -> Unit,
    onOpenDownloads: () -> Unit,
    onOpenHistory: () -> Unit = {},
    onOpenBookmarks: () -> Unit = {},
    onOpenFindInPage: () -> Unit = {},
    onOpenAiAssistant: () -> Unit = {},
    onOpenReaderMode: () -> Unit = {},
    onOpenSettings: () -> Unit,
    onOpenShields: () -> Unit
) {
    var isDesktopMode by remember { mutableStateOf(false) }

    AdaptiveDialogContainer(
        onDismiss = onDismiss,
        sheetState = sheetState
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(horizontal = 20.dp, vertical = 14.dp)
        ) {
            // Drag handle pill
            Box(
                modifier = Modifier
                    .size(width = 36.dp, height = 4.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(FireballBorder)
                    .align(Alignment.CenterHorizontally)
            )

            Spacer(modifier = Modifier.height(14.dp))

            // Quick Top Action Icons Row
            Row(
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp))
                    .background(FireballCardSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(14.dp))
                    .padding(horizontal = 8.dp, vertical = 10.dp)
            ) {
                QuickMenuActionButton(
                    icon = Icons.Default.Refresh,
                    label = "Reload",
                    onClick = {
                        onReload()
                        onDismiss()
                    }
                )
                QuickMenuActionButton(
                    icon = Icons.Default.BookmarkBorder,
                    label = "Bookmarks",
                    onClick = {
                        onOpenBookmarks()
                        onDismiss()
                    }
                )
                QuickMenuActionButton(
                    icon = Icons.Default.History,
                    label = "History",
                    onClick = {
                        onOpenHistory()
                        onDismiss()
                    }
                )
                QuickMenuActionButton(
                    icon = Icons.Default.Download,
                    label = "Downloads",
                    onClick = {
                        onOpenDownloads()
                        onDismiss()
                    }
                )
                QuickMenuActionButton(
                    icon = Icons.Default.Share,
                    label = "Share",
                    onClick = { onDismiss() }
                )
            }

            Spacer(modifier = Modifier.height(14.dp))

            // Detailed Menu List
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp))
                    .background(FireballCardSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(14.dp))
            ) {
                // Fireball AI
                MenuItemRow(
                    icon = Icons.Default.AutoAwesome,
                    label = "Fireball AI (Summarize & Chat)",
                    accentColor = FireballElectricLime,
                    onClick = {
                        onOpenAiAssistant()
                        onDismiss()
                    }
                )

                // Reader Mode
                MenuItemRow(
                    icon = Icons.Default.Book,
                    label = "Distraction-Free Reader Mode",
                    accentColor = Color(0xFF00F0FF),
                    onClick = {
                        onOpenReaderMode()
                        onDismiss()
                    }
                )

                // Find in page
                MenuItemRow(
                    icon = Icons.Default.FindInPage,
                    label = "Find in Page",
                    accentColor = FireballPrimaryText,
                    onClick = {
                        onOpenFindInPage()
                        onDismiss()
                    }
                )

                // Desktop site toggle row
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 12.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Default.DesktopWindows,
                            contentDescription = "Desktop Site",
                            tint = FireballPrimaryText,
                            modifier = Modifier.size(20.dp)
                        )
                        Spacer(modifier = Modifier.width(12.dp))
                        Text(
                            text = "Desktop Site",
                            style = MaterialTheme.typography.bodyMedium,
                            color = FireballPrimaryText
                        )
                    }

                    Switch(
                        checked = isDesktopMode,
                        onCheckedChange = { isDesktopMode = it },
                        colors = SwitchDefaults.colors(
                            checkedThumbColor = FireballBackground,
                            checkedTrackColor = FireballElectricLime,
                            uncheckedThumbColor = FireballMutedText,
                            uncheckedTrackColor = FireballRaisedSurface
                        )
                    )
                }

                // Shields
                MenuItemRow(
                    icon = Icons.Default.Shield,
                    label = "Fireball Shields & Egress",
                    accentColor = FireballMeteorOrange,
                    onClick = {
                        onOpenShields()
                        onDismiss()
                    }
                )

                // Clear browsing data
                MenuItemRow(
                    icon = Icons.Default.CleaningServices,
                    label = "Clear Browsing Data",
                    accentColor = FireballElectricLime,
                    onClick = { onDismiss() }
                )

                // Settings
                MenuItemRow(
                    icon = Icons.Default.Settings,
                    label = "Settings",
                    accentColor = FireballPrimaryText,
                    onClick = {
                        onOpenSettings()
                        onDismiss()
                    }
                )
            }

            Spacer(modifier = Modifier.height(14.dp))
        }
    }
}

@Composable
private fun QuickMenuActionButton(
    icon: ImageVector,
    label: String,
    onClick: () -> Unit
) {
    Column(
        horizontalAlignment = Alignment.CenterHorizontally,
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .clickable { onClick() }
            .padding(horizontal = 8.dp, vertical = 4.dp)
    ) {
        Box(
            modifier = Modifier
                .size(38.dp)
                .clip(CircleShape)
                .background(FireballRaisedSurface),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = icon,
                contentDescription = label,
                tint = FireballPrimaryText,
                modifier = Modifier.size(18.dp)
            )
        }
        Spacer(modifier = Modifier.height(4.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = FireballSecondaryText,
            fontSize = 11.sp
        )
    }
}

@Composable
private fun MenuItemRow(
    icon: ImageVector,
    label: String,
    accentColor: Color,
    onClick: () -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(horizontal = 16.dp, vertical = 13.dp)
    ) {
        Icon(
            imageVector = icon,
            contentDescription = label,
            tint = accentColor,
            modifier = Modifier.size(20.dp)
        )
        Spacer(modifier = Modifier.width(12.dp))
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            color = FireballPrimaryText,
            fontSize = 14.sp
        )
    }
}
