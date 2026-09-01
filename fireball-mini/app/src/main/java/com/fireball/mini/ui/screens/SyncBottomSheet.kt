package com.fireball.mini.ui.screens

import android.widget.Toast
import androidx.compose.animation.AnimatedVisibility
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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.CloudDone
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.Computer
import androidx.compose.material.icons.filled.ContentCopy
import androidx.compose.material.icons.filled.Devices
import androidx.compose.material.icons.filled.Link
import androidx.compose.material.icons.filled.OpenInBrowser
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.SyncDisabled
import androidx.compose.material.icons.filled.VpnKey
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SheetState
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.TabRowDefaults
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.RemoteTab
import com.fireball.mini.core.models.SyncDevice
import com.fireball.mini.core.models.SyncProvider
import com.fireball.mini.core.models.SyncState
import com.fireball.mini.core.models.SyncStatus
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
import com.fireball.mini.ui.viewmodels.BrowserViewModel
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SyncBottomSheet(
    viewModel: BrowserViewModel,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    onDismiss: () -> Unit,
    onOpenRemoteTab: (String) -> Unit = {}
) {
    val syncState by viewModel.syncState.collectAsState()
    val remoteTabs by viewModel.remoteTabs.collectAsState()

    var selectedProviderTab by remember { mutableIntStateOf(if (syncState.provider == SyncProvider.FIREFOX_SYNC) 1 else 0) }
    var braveWordsInput by remember { mutableStateOf("") }
    var firefoxEmailInput by remember { mutableStateOf("") }
    var firefoxSyncKeyInput by remember { mutableStateOf("") }
    var firefoxServerUrlInput by remember { mutableStateOf("https://sync.services.mozilla.com/1.5/") }

    val context = LocalContext.current
    val clipboard = LocalClipboardManager.current

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
                .padding(horizontal = 20.dp, vertical = 16.dp)
        ) {
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
                            .clip(CircleShape)
                            .background(FireballRaisedSurface),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = Icons.Default.CloudSync,
                            contentDescription = "Sync",
                            tint = FireballElectricLime,
                            modifier = Modifier.size(20.dp)
                        )
                    }
                    Spacer(modifier = Modifier.width(10.dp))
                    Column {
                        Text(
                            text = "Cross-Browser Sync",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                            color = FireballPrimaryText
                        )
                        Text(
                            text = "Brave Sync Chain & Firefox Sync 1.5",
                            style = MaterialTheme.typography.labelSmall,
                            color = FireballMutedText
                        )
                    }
                }

                // Status Badge
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(8.dp))
                        .background(
                            when (syncState.status) {
                                SyncStatus.SYNCED -> FireballElectricLime.copy(alpha = 0.15f)
                                SyncStatus.SYNCING -> FireballMeteorOrange.copy(alpha = 0.15f)
                                else -> FireballRaisedSurface
                            }
                        )
                        .padding(horizontal = 8.dp, vertical = 4.dp)
                ) {
                    Text(
                        text = when (syncState.status) {
                            SyncStatus.SYNCED -> "● Connected"
                            SyncStatus.SYNCING -> "Syncing..."
                            SyncStatus.CONNECTING -> "Connecting..."
                            SyncStatus.AUTH_ERROR -> "Auth Error"
                            SyncStatus.NETWORK_ERROR -> "Offline"
                            SyncStatus.DISCONNECTED -> "Offline"
                        },
                        fontSize = 11.sp,
                        fontWeight = FontWeight.Bold,
                        color = when (syncState.status) {
                            SyncStatus.SYNCED -> FireballElectricLime
                            SyncStatus.SYNCING -> FireballMeteorOrange
                            SyncStatus.AUTH_ERROR -> Color(0xFFFF4B4B)
                            else -> FireballMutedText
                        }
                    )
                }
            }

            Spacer(modifier = Modifier.height(14.dp))

            // Provider Tab selector: Brave Sync vs Firefox Sync
            TabRow(
                selectedTabIndex = selectedProviderTab,
                containerColor = FireballRaisedSurface,
                contentColor = FireballElectricLime,
                indicator = { tabPositions ->
                    TabRowDefaults.SecondaryIndicator(
                        modifier = Modifier.tabIndicatorOffset(tabPositions[selectedProviderTab]),
                        color = FireballElectricLime,
                        height = 2.5.dp
                    )
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(10.dp))
            ) {
                Tab(
                    selected = selectedProviderTab == 0,
                    onClick = { selectedProviderTab = 0 },
                    text = {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(imageVector = Icons.Default.VpnKey, contentDescription = null, modifier = Modifier.size(14.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Brave Sync Chain", fontSize = 12.5.sp, fontWeight = if (selectedProviderTab == 0) FontWeight.Bold else FontWeight.Normal)
                        }
                    },
                    selectedContentColor = FireballElectricLime,
                    unselectedContentColor = FireballMutedText
                )

                Tab(
                    selected = selectedProviderTab == 1,
                    onClick = { selectedProviderTab = 1 },
                    text = {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(imageVector = Icons.Default.Link, contentDescription = null, modifier = Modifier.size(14.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Firefox Sync", fontSize = 12.5.sp, fontWeight = if (selectedProviderTab == 1) FontWeight.Bold else FontWeight.Normal)
                        }
                    },
                    selectedContentColor = FireballElectricLime,
                    unselectedContentColor = FireballMutedText
                )
            }

            Spacer(modifier = Modifier.height(14.dp))

            LazyColumn(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                if (selectedProviderTab == 0) {
                    // Brave Sync Chain Tab
                    item {
                        BraveSyncCard(
                            syncState = syncState,
                            wordsInput = braveWordsInput,
                            onWordsInputChange = { braveWordsInput = it },
                            onGenerateNewCode = {
                                viewModel.generateNewBraveSyncChain()
                                Toast.makeText(context, "New 24-word sync chain generated!", Toast.LENGTH_SHORT).show()
                            },
                            onJoinChain = {
                                val success = viewModel.joinBraveSyncChain(braveWordsInput)
                                if (success) {
                                    Toast.makeText(context, "Joined Brave sync chain!", Toast.LENGTH_SHORT).show()
                                    braveWordsInput = ""
                                } else {
                                    Toast.makeText(context, "Invalid 24-word phrase", Toast.LENGTH_SHORT).show()
                                }
                            },
                            onCopyWords = { wordsList ->
                                clipboard.setText(AnnotatedString(wordsList.joinToString(" ")))
                                Toast.makeText(context, "24 words copied to clipboard!", Toast.LENGTH_SHORT).show()
                            },
                            onDisconnect = {
                                viewModel.disconnectSync()
                                Toast.makeText(context, "Disconnected from sync", Toast.LENGTH_SHORT).show()
                            }
                        )
                    }
                } else {
                    // Firefox Sync Tab
                    item {
                        FirefoxSyncCard(
                            syncState = syncState,
                            emailInput = firefoxEmailInput,
                            onEmailChange = { firefoxEmailInput = it },
                            keyInput = firefoxSyncKeyInput,
                            onKeyChange = { firefoxSyncKeyInput = it },
                            serverUrlInput = firefoxServerUrlInput,
                            onServerUrlChange = { firefoxServerUrlInput = it },
                            onConnect = {
                                val success = viewModel.connectFirefoxSync(firefoxEmailInput, firefoxSyncKeyInput, firefoxServerUrlInput)
                                if (success) {
                                    Toast.makeText(context, "Connected to Firefox Sync!", Toast.LENGTH_SHORT).show()
                                } else {
                                    Toast.makeText(context, "Check email and sync key", Toast.LENGTH_SHORT).show()
                                }
                            },
                            onDisconnect = {
                                viewModel.disconnectSync()
                                Toast.makeText(context, "Disconnected from Firefox Sync", Toast.LENGTH_SHORT).show()
                            }
                        )
                    }
                }

                // Sync Controls & Remote Tabs
                if (syncState.status == SyncStatus.SYNCED || syncState.status == SyncStatus.SYNCING) {
                    item {
                        SyncPreferencesCard(
                            syncState = syncState,
                            onToggleBookmarks = { viewModel.toggleSyncCategory(syncBookmarks = !syncState.syncBookmarks) },
                            onToggleHistory = { viewModel.toggleSyncCategory(syncHistory = !syncState.syncHistory) },
                            onToggleTabs = { viewModel.toggleSyncCategory(syncTabs = !syncState.syncOpenTabs) },
                            onSyncNow = {
                                viewModel.performSyncNow()
                                Toast.makeText(context, "Sync completed successfully!", Toast.LENGTH_SHORT).show()
                            }
                        )
                    }

                    if (remoteTabs.isNotEmpty()) {
                        item {
                            Text(
                                text = "OPEN TABS ON OTHER DEVICES (${remoteTabs.size})",
                                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                                color = FireballSecondaryText,
                                fontSize = 11.sp,
                                modifier = Modifier.padding(top = 6.dp)
                            )
                        }

                        items(remoteTabs) { rTab ->
                            RemoteTabRow(
                                tab = rTab,
                                onClick = {
                                    onOpenRemoteTab(rTab.url)
                                    onDismiss()
                                }
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(16.dp))
        }
    }
}

@Composable
private fun BraveSyncCard(
    syncState: SyncState,
    wordsInput: String,
    onWordsInputChange: (String) -> Unit,
    onGenerateNewCode: () -> Unit,
    onJoinChain: () -> Unit,
    onCopyWords: (List<String>) -> Unit,
    onDisconnect: () -> Unit
) {
    val isBraveConnected = syncState.provider == SyncProvider.BRAVE_SYNC_CHAIN && syncState.status == SyncStatus.SYNCED

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(14.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth()
        ) {
            Column {
                Text(
                    text = "Brave Sync Chain (BIP39 E2EE)",
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                    color = FireballPrimaryText
                )
                Text(
                    text = "Zero-knowledge sync compatible with Brave Desktop & Mobile",
                    style = MaterialTheme.typography.labelSmall,
                    color = FireballMutedText,
                    fontSize = 11.sp
                )
            }

            if (isBraveConnected) {
                IconButton(onClick = onDisconnect) {
                    Icon(imageVector = Icons.Default.SyncDisabled, contentDescription = "Disconnect", tint = FireballMeteorOrange)
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        if (isBraveConnected && syncState.braveSyncWords.isNotEmpty()) {
            Text(
                text = "Your 24-Word Recovery Phrase:",
                color = FireballElectricLime,
                fontSize = 11.5.sp,
                fontWeight = FontWeight.SemiBold
            )
            Spacer(modifier = Modifier.height(6.dp))

            // 24 Words Grid
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(8.dp))
                    .background(FireballDeepSurface)
                    .padding(8.dp)
            ) {
                syncState.braveSyncWords.chunked(4).forEachIndexed { chunkIndex, chunk ->
                    Row(
                        modifier = Modifier.fillMaxWidth().padding(vertical = 2.dp),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        chunk.forEachIndexed { wordIdx, word ->
                            val num = chunkIndex * 4 + wordIdx + 1
                            Text(
                                text = "$num. $word",
                                fontSize = 10.5.sp,
                                color = FireballPrimaryText,
                                modifier = Modifier.weight(1f)
                            )
                        }
                    }
                }
            }

            Spacer(modifier = Modifier.height(8.dp))

            Button(
                onClick = { onCopyWords(syncState.braveSyncWords) },
                colors = ButtonDefaults.buttonColors(containerColor = FireballDeepSurface, contentColor = FireballElectricLime),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(imageVector = Icons.Default.ContentCopy, contentDescription = null, modifier = Modifier.size(14.dp))
                Spacer(modifier = Modifier.width(6.dp))
                Text("Copy Sync Phrase", fontSize = 12.sp)
            }
        } else {
            // Unconnected state: Action buttons
            Button(
                onClick = onGenerateNewCode,
                colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Icon(imageVector = Icons.Default.VpnKey, contentDescription = null, modifier = Modifier.size(16.dp))
                Spacer(modifier = Modifier.width(6.dp))
                Text("Start a New Sync Chain", fontWeight = FontWeight.Bold)
            }

            Spacer(modifier = Modifier.height(10.dp))
            HorizontalDivider(color = FireballBorder)
            Spacer(modifier = Modifier.height(10.dp))

            Text(text = "Or Enter Existing 24-Word Code:", fontSize = 12.sp, color = FireballSecondaryText)
            Spacer(modifier = Modifier.height(6.dp))

            OutlinedTextField(
                value = wordsInput,
                onValueChange = onWordsInputChange,
                placeholder = { Text("abandon ability able about...", fontSize = 12.sp) },
                modifier = Modifier.fillMaxWidth().height(80.dp)
            )

            Spacer(modifier = Modifier.height(8.dp))

            Button(
                onClick = onJoinChain,
                enabled = wordsInput.trim().split("\\s+".toRegex()).size >= 24,
                colors = ButtonDefaults.buttonColors(containerColor = FireballActiveSurface, contentColor = FireballElectricLime),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Join Sync Chain", fontSize = 12.5.sp)
            }
        }
    }
}

@Composable
private fun FirefoxSyncCard(
    syncState: SyncState,
    emailInput: String,
    onEmailChange: (String) -> Unit,
    keyInput: String,
    onKeyChange: (String) -> Unit,
    serverUrlInput: String,
    onServerUrlChange: (String) -> Unit,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit
) {
    val isFirefoxConnected = syncState.provider == SyncProvider.FIREFOX_SYNC && syncState.status == SyncStatus.SYNCED

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(14.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth()
        ) {
            Column {
                Text(
                    text = "Firefox Sync 1.5 (FxA)",
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                    color = FireballPrimaryText
                )
                Text(
                    text = "Connects to Mozilla Storage Server or self-hosted Sync server",
                    style = MaterialTheme.typography.labelSmall,
                    color = FireballMutedText,
                    fontSize = 11.sp
                )
            }

            if (isFirefoxConnected) {
                IconButton(onClick = onDisconnect) {
                    Icon(imageVector = Icons.Default.SyncDisabled, contentDescription = "Disconnect", tint = FireballMeteorOrange)
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        if (isFirefoxConnected) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(imageVector = Icons.Default.CheckCircle, contentDescription = null, tint = FireballElectricLime, modifier = Modifier.size(16.dp))
                Spacer(modifier = Modifier.width(6.dp))
                Text(text = "Logged in as ${syncState.accountIdentifier}", color = FireballPrimaryText, fontSize = 12.5.sp)
            }
        } else {
            OutlinedTextField(
                value = emailInput,
                onValueChange = onEmailChange,
                label = { Text("Firefox Account Email") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(8.dp))

            OutlinedTextField(
                value = keyInput,
                onValueChange = onKeyChange,
                label = { Text("Sync Key / Token") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(8.dp))

            OutlinedTextField(
                value = serverUrlInput,
                onValueChange = onServerUrlChange,
                label = { Text("Sync Server URL") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(modifier = Modifier.height(12.dp))

            Button(
                onClick = onConnect,
                colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground),
                shape = RoundedCornerShape(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Connect Firefox Account", fontWeight = FontWeight.Bold)
            }
        }
    }
}

@Composable
private fun SyncPreferencesCard(
    syncState: SyncState,
    onToggleBookmarks: () -> Unit,
    onToggleHistory: () -> Unit,
    onToggleTabs: () -> Unit,
    onSyncNow: () -> Unit
) {
    val lastSyncStr = remember(syncState.lastSyncedTimestampMs) {
        if (syncState.lastSyncedTimestampMs > 0) {
            val sdf = SimpleDateFormat("HH:mm:ss", Locale.getDefault())
            sdf.format(Date(syncState.lastSyncedTimestampMs))
        } else "Never"
    }

    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(14.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(
                text = "DATA TO SYNCHRONIZE",
                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                color = FireballSecondaryText,
                fontSize = 11.sp
            )

            Text(
                text = "Last: $lastSyncStr",
                style = MaterialTheme.typography.labelSmall,
                color = FireballMutedText,
                fontSize = 10.5.sp
            )
        }

        Spacer(modifier = Modifier.height(8.dp))

        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().clickable { onToggleBookmarks() }
        ) {
            Checkbox(
                checked = syncState.syncBookmarks,
                onCheckedChange = { onToggleBookmarks() },
                colors = CheckboxDefaults.colors(checkedColor = FireballElectricLime, uncheckedColor = FireballBorder)
            )
            Text(text = "Bookmarks & Folders", color = FireballPrimaryText, fontSize = 13.sp)
        }

        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().clickable { onToggleHistory() }
        ) {
            Checkbox(
                checked = syncState.syncHistory,
                onCheckedChange = { onToggleHistory() },
                colors = CheckboxDefaults.colors(checkedColor = FireballElectricLime, uncheckedColor = FireballBorder)
            )
            Text(text = "Browsing History", color = FireballPrimaryText, fontSize = 13.sp)
        }

        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.fillMaxWidth().clickable { onToggleTabs() }
        ) {
            Checkbox(
                checked = syncState.syncOpenTabs,
                onCheckedChange = { onToggleTabs() },
                colors = CheckboxDefaults.colors(checkedColor = FireballElectricLime, uncheckedColor = FireballBorder)
            )
            Text(text = "Open Tabs Across Devices", color = FireballPrimaryText, fontSize = 13.sp)
        }

        Spacer(modifier = Modifier.height(10.dp))

        Button(
            onClick = onSyncNow,
            enabled = syncState.status != SyncStatus.SYNCING,
            colors = ButtonDefaults.buttonColors(containerColor = FireballActiveSurface, contentColor = FireballElectricLime),
            shape = RoundedCornerShape(8.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            if (syncState.status == SyncStatus.SYNCING) {
                CircularProgressIndicator(modifier = Modifier.size(16.dp), color = FireballElectricLime, strokeWidth = 2.dp)
                Spacer(modifier = Modifier.width(8.dp))
                Text("Syncing with cloud...")
            } else {
                Icon(imageVector = Icons.Default.Refresh, contentDescription = null, modifier = Modifier.size(16.dp))
                Spacer(modifier = Modifier.width(6.dp))
                Text("Sync Now")
            }
        }
    }
}

@Composable
private fun RemoteTabRow(
    tab: RemoteTab,
    onClick: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(10.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(10.dp))
            .clickable { onClick() }
            .padding(10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(imageVector = Icons.Default.Devices, contentDescription = null, tint = FireballElectricLime, modifier = Modifier.size(18.dp))
        Spacer(modifier = Modifier.width(10.dp))
        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = tab.title,
                style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                color = FireballPrimaryText,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
            Text(
                text = "${tab.deviceName} • ${tab.url}",
                style = MaterialTheme.typography.labelSmall,
                color = FireballMutedText,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis,
                fontSize = 11.sp
            )
        }
        Icon(imageVector = Icons.Default.OpenInBrowser, contentDescription = null, tint = FireballMutedText, modifier = Modifier.size(16.dp))
    }
}
