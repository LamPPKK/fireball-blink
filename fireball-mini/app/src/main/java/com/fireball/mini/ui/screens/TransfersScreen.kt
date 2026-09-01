package com.fireball.mini.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Download
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.components.TransferProgressCard
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.viewmodels.BrowserViewModel

@Composable
fun TransfersScreen(
    viewModel: BrowserViewModel,
    onBack: () -> Unit
) {
    val transfers by viewModel.transfers.collectAsState()

    Scaffold(
        topBar = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(FireballDeepSurface)
                    .statusBarsPadding()
            ) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 8.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        IconButton(onClick = onBack, modifier = Modifier.size(44.dp)) {
                            Icon(
                                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                                contentDescription = "Back",
                                tint = FireballPrimaryText,
                                modifier = Modifier.size(22.dp)
                            )
                        }
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            text = "Downloads & Transfers",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                            color = FireballPrimaryText,
                            fontSize = 17.sp
                        )
                    }

                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(FireballRaisedSurface)
                            .border(1.dp, FireballBorder, RoundedCornerShape(8.dp))
                            .padding(horizontal = 10.dp, vertical = 5.dp)
                    ) {
                        Text(
                            text = "${transfers.size} items",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = FireballElectricLime,
                            fontSize = 11.5.sp
                        )
                    }
                }
            }
        }
    ) { paddingValues ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .background(FireballBackground)
        ) {
            if (transfers.isEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .navigationBarsPadding(),
                    contentAlignment = Alignment.Center
                ) {
                    Column(horizontalAlignment = Alignment.CenterHorizontally) {
                        Box(
                            modifier = Modifier
                                .size(64.dp)
                                .clip(RoundedCornerShape(20.dp))
                                .background(FireballRaisedSurface)
                                .border(1.dp, FireballBorder, RoundedCornerShape(20.dp)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Download,
                                contentDescription = "No Transfers",
                                tint = FireballMutedText,
                                modifier = Modifier.size(32.dp)
                            )
                        }
                        Spacer(modifier = Modifier.height(14.dp))
                        Text(
                            text = "No active or completed downloads",
                            style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Medium),
                            color = FireballMutedText,
                            fontSize = 14.sp
                        )
                    }
                }
            } else {
                LazyColumn(
                    contentPadding = PaddingValues(16.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                    modifier = Modifier
                        .fillMaxSize()
                        .navigationBarsPadding()
                ) {
                    items(transfers, key = { it.id }) { transfer ->
                        TransferProgressCard(
                            transfer = transfer,
                            onPauseClick = { viewModel.pauseTransfer(transfer.id) },
                            onResumeClick = { viewModel.resumeTransfer(transfer.id) },
                            onCancelClick = { viewModel.cancelTransfer(transfer.id) }
                        )
                    }
                }
            }
        }
    }
}
