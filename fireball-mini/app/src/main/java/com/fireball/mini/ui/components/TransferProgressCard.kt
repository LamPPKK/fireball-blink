package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Pause
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.TransferItem
import com.fireball.mini.core.models.TransferStatus
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

@Composable
fun TransferProgressCard(
    transfer: TransferItem,
    onPauseClick: () -> Unit,
    onResumeClick: () -> Unit,
    onCancelClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(12.dp)
    ) {
        Column {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = transfer.title,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                    color = FireballPrimaryText,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f)
                )

                Row {
                    if (transfer.status == TransferStatus.ACTIVE) {
                        IconButton(onClick = onPauseClick, modifier = Modifier.size(28.dp)) {
                            Icon(
                                imageVector = Icons.Default.Pause,
                                contentDescription = "Pause",
                                tint = FireballMeteorOrange,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                    } else if (transfer.status == TransferStatus.PAUSED) {
                        IconButton(onClick = onResumeClick, modifier = Modifier.size(28.dp)) {
                            Icon(
                                imageVector = Icons.Default.PlayArrow,
                                contentDescription = "Resume",
                                tint = FireballElectricLime,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                    }

                    IconButton(onClick = onCancelClick, modifier = Modifier.size(28.dp)) {
                        Icon(
                            imageVector = Icons.Default.Close,
                            contentDescription = "Cancel",
                            tint = FireballMutedText,
                            modifier = Modifier.size(18.dp)
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            LinearProgressIndicator(
                progress = { transfer.progressFraction },
                modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                color = FireballElectricLime,
                trackColor = FireballBorder
            )

            Spacer(modifier = Modifier.height(6.dp))

            Row(
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text(
                    text = when (transfer.status) {
                        TransferStatus.ACTIVE -> {
                            val speedMb = transfer.downloadSpeedBytesPerSec / 1024 / 1024
                            val etaSec = transfer.estimatedRemainingSeconds
                            val etaStr = if (etaSec > 0) " · ${etaSec}s left" else ""
                            "$speedMb MB/s · ${transfer.connectionCount} conns$etaStr"
                        }
                        TransferStatus.PAUSED -> "Paused"
                        TransferStatus.COMPLETE -> "Complete"
                        TransferStatus.FAILED -> "Failed"
                        TransferStatus.CANCELLED -> "Cancelled"
                        TransferStatus.QUEUED -> "Queued"
                    },
                    style = MaterialTheme.typography.labelSmall,
                    color = if (transfer.status == TransferStatus.ACTIVE) FireballElectricLime else FireballMutedText,
                    fontSize = 11.sp
                )

                Text(
                    text = "${transfer.downloadedBytes / 1024 / 1024}MB / ${transfer.totalBytes / 1024 / 1024}MB",
                    style = MaterialTheme.typography.labelSmall,
                    color = FireballMutedText,
                    fontSize = 11.sp
                )
            }
        }
    }
}
