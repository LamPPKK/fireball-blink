package com.fireball.mini.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.Article
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.BookmarkBorder
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.Computer
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.FindInPage
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Key
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.QrCode2
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.StarBorder
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SheetState
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.components.AdaptiveDialogContainer
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ChromiumMenuSheet(
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    canGoBack: Boolean,
    canGoForward: Boolean,
    isBookmarked: Boolean,
    isDesktopMode: Boolean,
    onDismiss: () -> Unit,
    onBackClick: () -> Unit,
    onForwardClick: () -> Unit,
    onBookmarkClick: () -> Unit,
    onDownloadClick: () -> Unit,
    onInfoClick: () -> Unit,
    onNewTabClick: () -> Unit,
    onNewIncognitoClick: () -> Unit,
    onHistoryClick: () -> Unit,
    onBookmarksListClick: () -> Unit,
    onFindInPageClick: () -> Unit,
    onAiAssistantClick: () -> Unit = {},
    onReaderModeClick: () -> Unit,
    onShieldsClick: () -> Unit,
    onSyncClick: () -> Unit = {},
    onQrShareClick: () -> Unit = {},
    onPasswordsClick: () -> Unit = {},
    onSiteInfoClick: () -> Unit = {},
    onToggleDesktopMode: () -> Unit,
    onSettingsClick: () -> Unit
) {
    AdaptiveDialogContainer(
        onDismiss = onDismiss,
        sheetState = sheetState
    ) {
        val scrollState = rememberScrollState()

        Column(
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
                .verticalScroll(scrollState)
                .padding(horizontal = 12.dp, vertical = 6.dp)
        ) {
            // Material 3 Drag Handle Pill
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 6.dp),
                contentAlignment = Alignment.Center
            ) {
                Box(
                    modifier = Modifier
                        .size(width = 36.dp, height = 4.dp)
                        .clip(RoundedCornerShape(2.dp))
                        .background(FireballBorder)
                )
            }

            // Chromium Top Action Bar: [ ← ] [ → ] [ ⭐ ] [ 📥 ] [ ℹ️ ]
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(14.dp))
                    .background(FireballRaisedSurface)
                    .padding(horizontal = 6.dp, vertical = 4.dp),
                horizontalArrangement = Arrangement.SpaceAround,
                verticalAlignment = Alignment.CenterVertically
            ) {
                IconButton(
                    onClick = { onBackClick(); onDismiss() },
                    enabled = canGoBack,
                    modifier = Modifier.size(44.dp)
                ) {
                    Text(text = "←", color = if (canGoBack) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f), fontSize = 20.sp)
                }

                IconButton(
                    onClick = { onForwardClick(); onDismiss() },
                    enabled = canGoForward,
                    modifier = Modifier.size(44.dp)
                ) {
                    Text(text = "→", color = if (canGoForward) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f), fontSize = 20.sp)
                }

                IconButton(
                    onClick = { onBookmarkClick(); onDismiss() },
                    modifier = Modifier.size(44.dp)
                ) {
                    Icon(
                        imageVector = if (isBookmarked) Icons.Default.Star else Icons.Default.StarBorder,
                        contentDescription = "Bookmark",
                        tint = if (isBookmarked) FireballMeteorOrange else FireballPrimaryText
                    )
                }

                IconButton(
                    onClick = { onDownloadClick(); onDismiss() },
                    modifier = Modifier.size(44.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Download,
                        contentDescription = "Downloads",
                        tint = FireballPrimaryText
                    )
                }

                IconButton(
                    onClick = { onInfoClick(); onDismiss() },
                    modifier = Modifier.size(44.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Info,
                        contentDescription = "Page info",
                        tint = FireballPrimaryText
                    )
                }
            }

            Spacer(modifier = Modifier.height(10.dp))
            HorizontalDivider(color = FireballBorder.copy(alpha = 0.6f), thickness = 0.8.dp)
            Spacer(modifier = Modifier.height(4.dp))

            // Chromium Menu Items List
            ChromiumMenuItem(
                icon = Icons.Default.AutoAwesome,
                title = "Fireball AI (Summarize & Chat)",
                iconTint = FireballElectricLime,
                onClick = { onAiAssistantClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.AutoMirrored.Filled.Article,
                title = "Distraction-Free Reader Mode",
                iconTint = Color(0xFF00F0FF),
                onClick = { onReaderModeClick(); onDismiss() }
            )

            HorizontalDivider(color = FireballBorder.copy(alpha = 0.4f), thickness = 0.5.dp, modifier = Modifier.padding(vertical = 4.dp))

            ChromiumMenuItem(
                icon = Icons.Default.Add,
                title = "New tab",
                onClick = { onNewTabClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.VisibilityOff,
                title = "New Incognito tab",
                iconTint = FireballMeteorOrange,
                onClick = { onNewIncognitoClick(); onDismiss() }
            )

            HorizontalDivider(color = FireballBorder.copy(alpha = 0.4f), thickness = 0.5.dp, modifier = Modifier.padding(vertical = 4.dp))

            ChromiumMenuItem(
                icon = Icons.Default.History,
                title = "History",
                onClick = { onHistoryClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.BookmarkBorder,
                title = "Bookmarks",
                onClick = { onBookmarksListClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.Download,
                title = "Downloads",
                onClick = { onDownloadClick(); onDismiss() }
            )

            HorizontalDivider(color = FireballBorder.copy(alpha = 0.4f), thickness = 0.5.dp, modifier = Modifier.padding(vertical = 4.dp))

            ChromiumMenuItem(
                icon = Icons.Default.FindInPage,
                title = "Find in page",
                onClick = { onFindInPageClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.Security,
                title = "Fireball Shields & Privacy",
                iconTint = FireballElectricLime,
                onClick = { onShieldsClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.QrCode2,
                title = "Chia sẻ / Mã QR Tab",
                iconTint = FireballElectricLime,
                onClick = { onQrShareClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.Key,
                title = "Mật khẩu đã lưu (Password Vault)",
                onClick = { onPasswordsClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.Lock,
                title = "Quyền trang web (Site Permissions)",
                onClick = { onSiteInfoClick(); onDismiss() }
            )

            ChromiumMenuItem(
                icon = Icons.Default.CloudSync,
                title = "Sync with Brave & Firefox",
                iconTint = FireballElectricLime,
                onClick = { onSyncClick(); onDismiss() }
            )

            // Desktop Site Checkbox Item (M3 Compliant Touch Target >= 48dp)
            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .heightIn(min = 48.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .clickable { onToggleDesktopMode() }
                    .padding(horizontal = 12.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = Icons.Default.Computer,
                        contentDescription = "Desktop site",
                        tint = FireballPrimaryText,
                        modifier = Modifier.size(22.dp)
                    )
                    Text(
                        text = "Desktop site",
                        style = MaterialTheme.typography.bodyMedium,
                        color = FireballPrimaryText,
                        modifier = Modifier.padding(start = 16.dp),
                        fontSize = 14.5.sp
                    )
                }

                Checkbox(
                    checked = isDesktopMode,
                    onCheckedChange = { onToggleDesktopMode() },
                    colors = CheckboxDefaults.colors(
                        checkedColor = FireballElectricLime,
                        uncheckedColor = FireballMutedText,
                        checkmarkColor = FireballDeepSurface
                    )
                )
            }

            HorizontalDivider(color = FireballBorder.copy(alpha = 0.4f), thickness = 0.5.dp, modifier = Modifier.padding(vertical = 4.dp))

            ChromiumMenuItem(
                icon = Icons.Default.Settings,
                title = "Settings",
                onClick = { onSettingsClick(); onDismiss() }
            )

            Spacer(modifier = Modifier.height(16.dp))
        }
    }
}

@Composable
private fun ChromiumMenuItem(
    icon: ImageVector,
    title: String,
    iconTint: Color = FireballPrimaryText,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 48.dp)
            .clip(RoundedCornerShape(10.dp))
            .clickable { onClick() }
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            imageVector = icon,
            contentDescription = title,
            tint = iconTint,
            modifier = Modifier.size(22.dp)
        )
        Text(
            text = title,
            style = MaterialTheme.typography.bodyMedium,
            color = FireballPrimaryText,
            modifier = Modifier.padding(start = 16.dp),
            fontSize = 14.5.sp
        )
    }
}
