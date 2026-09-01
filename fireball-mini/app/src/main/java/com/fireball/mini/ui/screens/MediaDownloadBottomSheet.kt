package com.fireball.mini.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.VideoLibrary
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.SheetState
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.DiscoveredMedia
import com.fireball.mini.core.models.MediaKind
import com.fireball.mini.core.models.MediaQualityTrack
import com.fireball.mini.ui.components.AdaptiveDialogContainer
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.viewmodels.BrowserViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MediaDownloadBottomSheet(
    viewModel: BrowserViewModel,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    onDismiss: () -> Unit,
    onNavigateToTransfers: () -> Unit
) {
    val discoveredMedia by viewModel.discoveredMedia.collectAsState()

    AdaptiveDialogContainer(
        onDismiss = onDismiss,
        sheetState = sheetState
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
                .padding(horizontal = 20.dp, vertical = 14.dp)
        ) {
            // Drag Handle Pill
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(bottom = 12.dp),
                contentAlignment = Alignment.Center
            ) {
                Box(
                    modifier = Modifier
                        .size(width = 36.dp, height = 4.dp)
                        .clip(RoundedCornerShape(2.dp))
                        .background(FireballBorder)
                )
            }

            // Header
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .size(36.dp)
                            .clip(RoundedCornerShape(10.dp))
                            .background(FireballElectricLime.copy(alpha = 0.15f)),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.VideoLibrary,
                            contentDescription = "Media Sniffer",
                            tint = FireballElectricLime,
                            modifier = Modifier.size(20.dp)
                        )
                    }
                    Spacer(modifier = Modifier.width(10.dp))
                    Column {
                        Text(
                            text = "Discovered Media Streams",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                            color = FireballPrimaryText,
                            fontSize = 16.sp
                        )
                        Text(
                            text = "Smart Media Sniffer (HLS, DASH, MP4)",
                            style = MaterialTheme.typography.bodySmall,
                            color = FireballMutedText,
                            fontSize = 11.5.sp
                        )
                    }
                }

                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(FireballRaisedSurface)
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        text = "${discoveredMedia.size} stream(s)",
                        style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                        color = FireballElectricLime,
                        fontSize = 11.sp
                    )
                }
            }

            Spacer(modifier = Modifier.height(14.dp))

            if (discoveredMedia.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .height(180.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "No media streams detected on this page yet.\nPlay a video or audio stream to capture.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = FireballMutedText,
                        fontSize = 13.sp,
                        textAlign = androidx.compose.ui.text.style.TextAlign.Center
                    )
                }
            } else {
                LazyColumn(
                    verticalArrangement = Arrangement.spacedBy(10.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    items(discoveredMedia, key = { it.id }) { media ->
                        MediaStreamCard(
                            media = media,
                            onDownload = { selectedTrack ->
                                viewModel.startMediaDownload(media, selectedTrack)
                                onDismiss()
                                onNavigateToTransfers()
                            }
                        )
                    }
                }
            }
        }
    }
}

@Composable
private fun MediaStreamCard(
    media: DiscoveredMedia,
    onDownload: (MediaQualityTrack?) -> Unit
) {
    val isAudio = media.mediaKind == MediaKind.DIRECT_AUDIO || media.mimeType.startsWith("audio/")
    val accentColor = if (isAudio) FireballMeteorOrange else FireballElectricLime

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(14.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(14.dp))
            .padding(14.dp)
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
                        .size(34.dp)
                        .clip(RoundedCornerShape(8.dp))
                        .background(accentColor.copy(alpha = 0.15f)),
                    contentAlignment = Alignment.Center
                ) {
                    Icon(
                        imageVector = if (isAudio) Icons.Default.PlayArrow else Icons.Default.VideoLibrary,
                        contentDescription = null,
                        tint = accentColor,
                        modifier = Modifier.size(18.dp)
                    )
                }

                Spacer(modifier = Modifier.width(10.dp))

                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = if (media.title.isNotBlank()) media.title else "Media Stream",
                        style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                        color = FireballPrimaryText,
                        fontSize = 13.sp,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = "${media.mimeType} • ${formatFileSize(media.estimatedSizeBytes)}",
                        style = MaterialTheme.typography.bodySmall,
                        color = FireballMutedText,
                        fontSize = 11.sp
                    )
                }
            }
        }

        // Quality Tracks Selector (if multi-bitrate HLS/DASH)
        if (media.availableQualities.isNotEmpty()) {
            Spacer(modifier = Modifier.height(10.dp))
            Text(
                text = "AVAILABLE QUALITY TRACKS:",
                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                color = FireballMutedText,
                fontSize = 9.5.sp
            )
            Spacer(modifier = Modifier.height(6.dp))

            Row(
                modifier = Modifier
                    .fillMaxWidth()
                    .horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                for (track in media.availableQualities) {
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(FireballRaisedSurface)
                            .border(1.dp, FireballBorder, RoundedCornerShape(8.dp))
                            .clickable { onDownload(track) }
                            .padding(horizontal = 10.dp, vertical = 6.dp)
                    ) {
                        Text(
                            text = "${track.label} (${track.resolution ?: ""})",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = FireballElectricLime,
                            fontSize = 11.sp
                        )
                    }
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        Button(
            onClick = { onDownload(media.availableQualities.firstOrNull()) },
            colors = ButtonDefaults.buttonColors(
                containerColor = accentColor,
                contentColor = FireballBackground
            ),
            shape = RoundedCornerShape(10.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            Icon(
                imageVector = Icons.Default.Download,
                contentDescription = "Download",
                modifier = Modifier.size(16.dp)
            )
            Spacer(modifier = Modifier.width(6.dp))
            Text(
                text = if (isAudio) "Download Audio Stream" else "Download Video Stream",
                style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.Bold),
                fontSize = 13.sp
            )
        }
    }
}

private fun formatFileSize(bytes: Long): String {
    if (bytes <= 0) return "Stream"
    val kb = bytes / 1024.0
    val mb = kb / 1024.0
    val gb = mb / 1024.0
    return when {
        gb >= 1.0 -> String.format(java.util.Locale.US, "%.1f GB", gb)
        mb >= 1.0 -> String.format(java.util.Locale.US, "%.1f MB", mb)
        else -> String.format(java.util.Locale.US, "%.0f KB", kb)
    }
}
