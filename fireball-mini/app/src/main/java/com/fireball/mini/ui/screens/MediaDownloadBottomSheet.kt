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
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
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
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Text
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
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.viewmodels.BrowserViewModel

import androidx.compose.material3.SheetState
import androidx.compose.material3.rememberModalBottomSheetState

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import com.fireball.mini.core.models.MediaQualityTrack

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MediaDownloadBottomSheet(
    viewModel: BrowserViewModel,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    onDismiss: () -> Unit,
    onNavigateToTransfers: () -> Unit
) {
    val discoveredMedia by viewModel.discoveredMedia.collectAsState()

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = FireballDeepSurface,
        scrimColor = FireballBackground.copy(alpha = 0.7f),
        dragHandle = null
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(20.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = Icons.Default.VideoLibrary,
                        contentDescription = "Media Sniffer",
                        tint = FireballElectricLime,
                        modifier = Modifier.size(24.dp)
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(
                        text = "Discovered Media Streams",
                        style = MaterialTheme.typography.titleMedium,
                        color = FireballPrimaryText
                    )
                }

                Text(
                    text = "${discoveredMedia.size} streams",
                    style = MaterialTheme.typography.labelSmall,
                    color = FireballMutedText
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            if (discoveredMedia.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 24.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text(
                        text = "No downloadable media found on this page.",
                        style = MaterialTheme.typography.bodyMedium,
                        color = FireballMutedText
                    )
                }
            } else {
                LazyColumn(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    items(discoveredMedia) { media ->
                        MediaItemRow(
                            media = media,
                            onSelectQuality = { qualityId ->
                                viewModel.selectMediaQuality(media.id, qualityId)
                            },
                            onDownloadClick = { qualityTrack ->
                                viewModel.startMediaDownload(media, qualityTrack)
                                onDismiss()
                                onNavigateToTransfers()
                            }
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))
        }
    }
}

@Composable
private fun MediaItemRow(
    media: DiscoveredMedia,
    onSelectQuality: (String) -> Unit,
    onDownloadClick: (MediaQualityTrack?) -> Unit
) {
    val selectedQuality = media.availableQualities.find { it.id == media.selectedQualityId }
        ?: media.availableQualities.firstOrNull()

    Box(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(12.dp)
    ) {
        Column(modifier = Modifier.fillMaxWidth()) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.weight(1f)) {
                    Text(
                        text = media.title,
                        style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                        color = FireballPrimaryText,
                        maxLines = 1,
                        overflow = TextOverflow.Ellipsis
                    )
                    Text(
                        text = when (media.mediaKind) {
                            MediaKind.HLS_VOD -> "HLS Adaptive Stream (Multi-Quality)"
                            MediaKind.DASH_VOD -> "DASH Stream (fMP4 Stream-Copy)"
                            MediaKind.DIRECT_VIDEO -> "Direct Video Stream"
                            MediaKind.DIRECT_AUDIO -> "Direct Audio Stream"
                        },
                        style = MaterialTheme.typography.bodyMedium,
                        color = FireballElectricLime,
                        fontSize = 11.sp
                    )
                }

                Button(
                    onClick = { onDownloadClick(selectedQuality) },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = FireballElectricLime,
                        contentColor = FireballBackground
                    ),
                    shape = RoundedCornerShape(8.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Download,
                        contentDescription = "Download",
                        modifier = Modifier.size(16.dp)
                    )
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(text = "Download", style = MaterialTheme.typography.labelSmall)
                }
            }

            if (media.availableQualities.isNotEmpty()) {
                Spacer(modifier = Modifier.height(10.dp))
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .horizontalScroll(rememberScrollState()),
                    horizontalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    media.availableQualities.forEach { quality ->
                        val isSelected = quality.id == selectedQuality?.id
                        val sizeMb = quality.estimatedSizeBytes / 1024 / 1024
                        val sizeLabel = if (sizeMb > 0) " (${sizeMb}MB)" else ""
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(6.dp))
                                .background(if (isSelected) FireballActiveSurface else FireballDeepSurface)
                                .border(
                                    1.dp,
                                    if (isSelected) FireballElectricLime else FireballBorder,
                                    RoundedCornerShape(6.dp)
                                )
                                .clickable { onSelectQuality(quality.id) }
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text(
                                text = "${quality.label}$sizeLabel",
                                fontSize = 10.5.sp,
                                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                                color = if (isSelected) FireballElectricLime else FireballMutedText
                            )
                        }
                    }
                }
            }
        }
    }
}
