package com.fireball.mini.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.server.FireballServerToGo
import com.fireball.mini.ui.theme.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ServerToGoScreen(
    server: FireballServerToGo,
    onBack: () -> Unit,
    modifier: Modifier = Modifier
) {
    val isRunning by server.isRunning.collectAsState()
    val clientCount by server.connectedClientsCount.collectAsState()
    val ipAddress by server.serverIpAddress.collectAsState()

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Fireball Server To Go", color = FireballPrimaryText, fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back", tint = FireballPrimaryText)
                    }
                },

                colors = TopAppBarDefaults.topAppBarColors(containerColor = FireballBackground)
            )
        },
        containerColor = FireballBackground
    ) { padding ->
        Column(
            modifier = modifier
                .fillMaxSize()
                .padding(padding)
                .padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Status Card
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .border(1.dp, if (isRunning) FireballElectricLime else FireballBorder, RoundedCornerShape(16.dp)),
                colors = CardDefaults.cardColors(containerColor = FireballCardSurface)
            ) {
                Column(modifier = Modifier.padding(20.dp), horizontalAlignment = Alignment.CenterHorizontally) {
                    Box(
                        modifier = Modifier
                            .size(64.dp)
                            .clip(CircleShape)
                            .background(if (isRunning) FireballElectricLime.copy(alpha = 0.15f) else FireballDeepSurface),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = if (isRunning) Icons.Default.Sensors else Icons.Default.SensorsOff,
                            contentDescription = "Server State",
                            tint = if (isRunning) FireballElectricLime else FireballMutedText,
                            modifier = Modifier.size(32.dp)
                        )
                    }

                    Spacer(modifier = Modifier.height(12.dp))

                    Text(
                        text = if (isRunning) "POCKET SERVER ONLINE" else "SERVER OFFLINE",
                        fontWeight = FontWeight.Bold,
                        fontSize = 16.sp,
                        color = if (isRunning) FireballElectricLime else FireballMutedText
                    )

                    Spacer(modifier = Modifier.height(6.dp))

                    Text(
                        text = if (isRunning) "http://$ipAddress:${server.port}" else "Tap start to broadcast over Hotspot / LAN",
                        fontFamily = FontFamily.Monospace,
                        fontSize = 13.sp,
                        color = FireballSecondaryText
                    )
                }
            }

            Spacer(modifier = Modifier.height(20.dp))

            // Stats Metrics Row
            Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                Card(
                    modifier = Modifier.weight(1f),
                    colors = CardDefaults.cardColors(containerColor = FireballCardSurface)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text("CONNECTED CLIENTS", fontSize = 11.sp, color = FireballMutedText, fontWeight = FontWeight.SemiBold)
                        Spacer(modifier = Modifier.height(4.dp))
                        Text("$clientCount", fontSize = 22.sp, fontWeight = FontWeight.Bold, color = FireballPrimaryText)
                    }
                }

                Card(
                    modifier = Modifier.weight(1f),
                    colors = CardDefaults.cardColors(containerColor = FireballCardSurface)
                ) {
                    Column(modifier = Modifier.padding(16.dp)) {
                        Text("STREAM PROTOCOL", fontSize = 11.sp, color = FireballMutedText, fontWeight = FontWeight.SemiBold)
                        Spacer(modifier = Modifier.height(4.dp))
                        Text("FBEAM / HTTP", fontSize = 16.sp, fontWeight = FontWeight.Bold, color = FireballMeteorOrange)
                    }
                }
            }

            Spacer(modifier = Modifier.weight(1f))

            // Start / Stop Toggle Button
            Button(
                onClick = {
                    if (isRunning) server.stop() else server.start()
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(52.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = if (isRunning) FireballMeteorOrange else FireballElectricLime
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(
                    imageVector = if (isRunning) Icons.Default.Stop else Icons.Default.PlayArrow,
                    contentDescription = null,
                    tint = Color.Black
                )
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = if (isRunning) "STOP SERVER TO GO" else "START SERVER TO GO",
                    color = Color.Black,
                    fontWeight = FontWeight.Bold,
                    fontSize = 15.sp
                )
            }
        }
    }
}
