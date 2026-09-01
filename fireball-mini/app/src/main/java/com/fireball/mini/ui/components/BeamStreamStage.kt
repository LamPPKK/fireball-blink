package com.fireball.mini.ui.components

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.beam.BeamStreamingClient
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange

/**
 * Jetpack Compose Stage for rendering the remote Fireball Server browser stream
 * with full touch interaction and status badge.
 */
@Composable
fun BeamStreamStage(
    beamClient: BeamStreamingClient,
    modifier: Modifier = Modifier
) {
    val currentFrame by beamClient.currentFrame.collectAsState()
    val isStreaming by beamClient.isStreaming.collectAsState()
    val statusMessage by beamClient.statusMessage.collectAsState()

    LaunchedEffect(Unit) {
        beamClient.startStream()
    }

    Box(
        modifier = modifier
            .fillMaxSize()
            .background(FireballBackground)
            .pointerInput(Unit) {
                detectTapGestures { offset ->
                    val normX = (offset.x / size.width).coerceIn(0f, 1f)
                    val normY = (offset.y / size.height).coerceIn(0f, 1f)
                    beamClient.sendClick(normX, normY)
                }
            }
    ) {
        val frame = currentFrame
        if (frame != null) {
            Image(
                bitmap = frame.asImageBitmap(),
                contentDescription = "Fireball Remote Browser Stream",
                contentScale = ContentScale.FillBounds,
                modifier = Modifier.fillMaxSize()
            )
        } else {
            Column(
                modifier = Modifier.align(Alignment.Center),
                horizontalAlignment = Alignment.CenterHorizontally
            ) {
                CircularProgressIndicator(
                    color = FireballElectricLime,
                    modifier = Modifier.size(36.dp),
                    strokeWidth = 3.dp
                )
                Spacer(modifier = Modifier.height(12.dp))
                Text(
                    text = statusMessage,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    fontSize = 13.sp
                )
            }
        }

        // Top Stream Status Badge
        Surface(
            color = FireballBackground.copy(alpha = 0.85f),
            shape = RoundedCornerShape(20.dp),
            border = androidx.compose.foundation.BorderStroke(1.dp, FireballElectricLime.copy(alpha = 0.5f)),
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(12.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(horizontal = 10.dp, vertical = 5.dp)
            ) {
                Box(
                    modifier = Modifier
                        .size(8.dp)
                        .clip(RoundedCornerShape(4.dp))
                        .background(if (isStreaming) FireballElectricLime else FireballMeteorOrange)
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    text = "⚡ BEAM STREAM",
                    color = FireballElectricLime,
                    fontSize = 11.sp,
                    fontWeight = androidx.compose.ui.text.font.FontWeight.Bold
                )
            }
        }
    }
}
