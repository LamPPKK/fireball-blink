package com.fireball.mini.ui.screens

import android.widget.Toast
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.Article
import androidx.compose.material.icons.filled.AutoAwesome
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.ChevronRight
import androidx.compose.material.icons.filled.CleaningServices
import androidx.compose.material.icons.filled.CloudSync
import androidx.compose.material.icons.filled.Computer
import androidx.compose.material.icons.filled.DarkMode
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.FolderZip
import androidx.compose.material.icons.filled.Headphones
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Memory
import androidx.compose.material.icons.filled.Palette
import androidx.compose.material.icons.filled.Public
import androidx.compose.material.icons.filled.RecordVoiceOver
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Speed
import androidx.compose.material.icons.filled.Storage
import androidx.compose.material.icons.filled.Sync
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material.icons.filled.Tv
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material.icons.filled.VpnKey
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.RadioButtonDefaults
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.AutoArchiveDuration
import com.fireball.mini.core.models.BlockerMode
import com.fireball.mini.core.models.PreferredVideoQuality
import com.fireball.mini.core.models.ReaderTheme
import com.fireball.mini.core.models.SearchEngine
import com.fireball.mini.core.models.SyncProvider
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

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    viewModel: BrowserViewModel,
    onBack: () -> Unit,
    onOpenShields: () -> Unit = {},
    onOpenSync: () -> Unit = {},
    onOpenBookmarks: () -> Unit = {}
) {
    val settings by viewModel.browserSettings.collectAsState()
    val syncState by viewModel.syncState.collectAsState()
    val blockerMode by viewModel.blockerMode.collectAsState()
    val isPopupBlockingEnabled by viewModel.isPopupBlockingEnabled.collectAsState()
    val isRedirectBlockingEnabled by viewModel.isRedirectBlockingEnabled.collectAsState()
    val readerTheme by viewModel.readerTheme.collectAsState()
    val activeSpace = viewModel.uiState.collectAsState().value.activeSpace

    val context = LocalContext.current

    var showSearchEngineDialog by remember { mutableStateOf(false) }
    var showArchiveDurationDialog by remember { mutableStateOf(false) }
    var showVideoQualityDialog by remember { mutableStateOf(false) }
    var showDownloadThreadsDialog by remember { mutableStateOf(false) }
    var showClearDataDialog by remember { mutableStateOf(false) }
    var showReaderThemeDialog by remember { mutableStateOf(false) }

    // Dialog 1: Search Engine Selection
    if (showSearchEngineDialog) {
        AlertDialog(
            onDismissRequest = { showSearchEngineDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.Search, contentDescription = null, tint = FireballElectricLime)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Default Search Engine", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    SearchEngine.entries.forEach { engine ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .clickable {
                                    viewModel.setSearchEngine(engine)
                                    showSearchEngineDialog = false
                                }
                                .padding(vertical = 8.dp, horizontal = 4.dp)
                        ) {
                            RadioButton(
                                selected = settings.searchEngine == engine,
                                onClick = {
                                    viewModel.setSearchEngine(engine)
                                    showSearchEngineDialog = false
                                },
                                colors = RadioButtonDefaults.colors(selectedColor = FireballElectricLime, unselectedColor = FireballMutedText)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Column {
                                Text(text = engine.displayName, color = FireballPrimaryText, fontWeight = FontWeight.SemiBold, fontSize = 14.sp)
                                Text(text = engine.homeUrl, color = FireballMutedText, fontSize = 11.sp)
                            }
                        }
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = { showSearchEngineDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Close")
                }
            }
        )
    }

    // Dialog 2: Auto-Archive Duration
    if (showArchiveDurationDialog) {
        AlertDialog(
            onDismissRequest = { showArchiveDurationDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.Memory, contentDescription = null, tint = FireballElectricLime)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Auto-Archive Duration", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    AutoArchiveDuration.entries.forEach { duration ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .clickable {
                                    viewModel.setAutoArchiveDuration(duration)
                                    showArchiveDurationDialog = false
                                }
                                .padding(vertical = 8.dp, horizontal = 4.dp)
                        ) {
                            RadioButton(
                                selected = settings.autoArchiveDuration == duration,
                                onClick = {
                                    viewModel.setAutoArchiveDuration(duration)
                                    showArchiveDurationDialog = false
                                },
                                colors = RadioButtonDefaults.colors(selectedColor = FireballElectricLime, unselectedColor = FireballMutedText)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(text = duration.displayName, color = FireballPrimaryText, fontSize = 13.5.sp)
                        }
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = { showArchiveDurationDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Close")
                }
            }
        )
    }

    // Dialog 3: Preferred Video Quality
    if (showVideoQualityDialog) {
        AlertDialog(
            onDismissRequest = { showVideoQualityDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.Tv, contentDescription = null, tint = FireballElectricLime)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Preferred Video Quality", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    PreferredVideoQuality.entries.forEach { q ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .clickable {
                                    viewModel.setPreferredVideoQuality(q)
                                    showVideoQualityDialog = false
                                }
                                .padding(vertical = 8.dp, horizontal = 4.dp)
                        ) {
                            RadioButton(
                                selected = settings.preferredVideoQuality == q,
                                onClick = {
                                    viewModel.setPreferredVideoQuality(q)
                                    showVideoQualityDialog = false
                                },
                                colors = RadioButtonDefaults.colors(selectedColor = FireballElectricLime, unselectedColor = FireballMutedText)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(text = q.displayName, color = FireballPrimaryText, fontSize = 13.5.sp)
                        }
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = { showVideoQualityDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Close")
                }
            }
        )
    }

    // Dialog 4: Download Acceleration Threads
    if (showDownloadThreadsDialog) {
        val options = listOf(2, 4, 8)
        AlertDialog(
            onDismissRequest = { showDownloadThreadsDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.Speed, contentDescription = null, tint = FireballElectricLime)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Parallel Download Connections", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    options.forEach { count ->
                        Row(
                            verticalAlignment = Alignment.CenterVertically,
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .clickable {
                                    viewModel.setDownloadThreads(count)
                                    showDownloadThreadsDialog = false
                                }
                                .padding(vertical = 8.dp, horizontal = 4.dp)
                        ) {
                            RadioButton(
                                selected = settings.downloadThreads == count,
                                onClick = {
                                    viewModel.setDownloadThreads(count)
                                    showDownloadThreadsDialog = false
                                },
                                colors = RadioButtonDefaults.colors(selectedColor = FireballElectricLime, unselectedColor = FireballMutedText)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(text = "$count Parallel Thread Connections", color = FireballPrimaryText, fontSize = 13.5.sp)
                        }
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = { showDownloadThreadsDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Close")
                }
            }
        )
    }

    // Dialog 5: Clear Browsing Data
    if (showClearDataDialog) {
        var clearHistoryChecked by remember { mutableStateOf(true) }
        var clearBookmarksChecked by remember { mutableStateOf(false) }
        var clearTransfersChecked by remember { mutableStateOf(false) }

        AlertDialog(
            onDismissRequest = { showClearDataDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.DeleteOutline, contentDescription = null, tint = FireballMeteorOrange)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Clear Browsing Data", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    Text(text = "Choose data to permanently remove from this device:", color = FireballSecondaryText, fontSize = 12.sp)
                    Spacer(modifier = Modifier.height(12.dp))

                    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().clickable { clearHistoryChecked = !clearHistoryChecked }) {
                        Checkbox(checked = clearHistoryChecked, onCheckedChange = { clearHistoryChecked = it }, colors = CheckboxDefaults.colors(checkedColor = FireballMeteorOrange))
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(text = "Browsing History & Cache", color = FireballPrimaryText, fontSize = 13.sp)
                    }

                    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().clickable { clearBookmarksChecked = !clearBookmarksChecked }) {
                        Checkbox(checked = clearBookmarksChecked, onCheckedChange = { clearBookmarksChecked = it }, colors = CheckboxDefaults.colors(checkedColor = FireballMeteorOrange))
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(text = "Saved Bookmarks", color = FireballPrimaryText, fontSize = 13.sp)
                    }

                    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth().clickable { clearTransfersChecked = !clearTransfersChecked }) {
                        Checkbox(checked = clearTransfersChecked, onCheckedChange = { clearTransfersChecked = it }, colors = CheckboxDefaults.colors(checkedColor = FireballMeteorOrange))
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(text = "Download History List", color = FireballPrimaryText, fontSize = 13.sp)
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        viewModel.clearBrowsingData(clearHistoryChecked, clearBookmarksChecked, clearTransfersChecked)
                        Toast.makeText(context, "Selected browsing data cleared!", Toast.LENGTH_SHORT).show()
                        showClearDataDialog = false
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballMeteorOrange, contentColor = FireballBackground)
                ) {
                    Text("Clear Data", fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                Button(
                    onClick = { showClearDataDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballRaisedSurface, contentColor = FireballMutedText)
                ) {
                    Text("Cancel")
                }
            }
        )
    }

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
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 8.dp)
                ) {
                    IconButton(onClick = onBack) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = "Back",
                            tint = FireballPrimaryText
                        )
                    }
                    Spacer(modifier = Modifier.width(4.dp))
                    Text(
                        text = "Settings",
                        style = MaterialTheme.typography.titleLarge.copy(fontWeight = FontWeight.Bold),
                        color = FireballPrimaryText,
                        modifier = Modifier.weight(1f)
                    )
                }
                HorizontalDivider(color = FireballBorder, thickness = 0.5.dp)
            }
        }
    ) { paddingValues ->
        LazyColumn(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .background(FireballBackground)
                .padding(horizontal = 16.dp, vertical = 12.dp)
                .navigationBarsPadding(),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Profile & Active Space Card
            item {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp))
                        .background(FireballRaisedSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(16.dp))
                        .padding(14.dp)
                ) {
                    Box(
                        modifier = Modifier
                            .size(44.dp)
                            .clip(CircleShape)
                            .background(FireballElectricLime.copy(alpha = 0.15f))
                            .border(1.5.dp, FireballElectricLime, CircleShape),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(imageVector = Icons.Default.Public, contentDescription = null, tint = FireballElectricLime, modifier = Modifier.size(24.dp))
                    }
                    Spacer(modifier = Modifier.width(12.dp))
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = activeSpace?.name ?: "Main Space",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                            color = FireballPrimaryText
                        )
                        Text(
                            text = "Partition: ${activeSpace?.profileId ?: "profile-main"} · Isolated Cookies",
                            style = MaterialTheme.typography.labelSmall,
                            color = FireballMutedText
                        )
                    }
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(8.dp))
                            .background(if (syncState.status == SyncStatus.SYNCED) FireballElectricLime.copy(alpha = 0.2f) else FireballDeepSurface)
                            .padding(horizontal = 8.dp, vertical = 4.dp)
                    ) {
                        Text(
                            text = if (syncState.status == SyncStatus.SYNCED) "Synced" else "Local",
                            fontSize = 11.sp,
                            fontWeight = FontWeight.Bold,
                            color = if (syncState.status == SyncStatus.SYNCED) FireballElectricLime else FireballMutedText
                        )
                    }
                }
            }

            // Group 1: General & Search
            item {
                SettingsSectionTitle("SEARCH & GENERAL")
                SettingsGroupContainer {
                    SettingsRowClickable(
                        icon = Icons.Default.Search,
                        title = "Default Search Engine",
                        subtitle = settings.searchEngine.displayName,
                        onClick = { showSearchEngineDialog = true }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.Lock,
                        title = "Strict URL Tracker Cleaner",
                        subtitle = "Bóc tách tham số tracking (utm_*, fbclid, gclid...)",
                        checked = settings.isUrlCleanerEnabled,
                        onCheckedChange = { viewModel.setUrlCleanerEnabled(it) }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.Computer,
                        title = "Desktop Site by Default",
                        subtitle = "Yêu cầu giao diện máy tính trên máy tính bảng & PC",
                        checked = settings.isDesktopModeDefault,
                        onCheckedChange = { viewModel.setDesktopModeDefault(it) }
                    )
                }
            }

            // Group 2: Cross-Device Synchronization
            item {
                SettingsSectionTitle("CROSS-BROWSER SYNCHRONIZATION")
                SettingsGroupContainer {
                    SettingsRowClickable(
                        icon = Icons.Default.CloudSync,
                        title = "Brave Sync Chain & Firefox Sync",
                        subtitle = when (syncState.status) {
                            SyncStatus.SYNCED -> "Connected to ${if (syncState.provider == SyncProvider.BRAVE_SYNC_CHAIN) "Brave Sync Chain" else "Firefox Account"}"
                            SyncStatus.SYNCING -> "Syncing data in progress..."
                            SyncStatus.AUTH_ERROR -> "Auth error, tap to reconnect"
                            else -> "Disconnected · Tap to pair with Brave or Firefox"
                        },
                        iconTint = if (syncState.status == SyncStatus.SYNCED) FireballElectricLime else FireballPrimaryText,
                        onClick = onOpenSync
                    )
                }
            }

            // Group 3: Fireball Shields & Privacy
            item {
                SettingsSectionTitle("SHIELDS & SECURITY")
                SettingsGroupContainer {
                    SettingsRowClickable(
                        icon = Icons.Default.Security,
                        title = "Fireball Shields & Content Filters",
                        subtitle = "Chế độ: ${blockerMode.name} · 6 bộ lọc (187k+ quy tắc)",
                        iconTint = FireballElectricLime,
                        onClick = onOpenShields
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.Lock,
                        title = "HTTPS-Only Mode",
                        subtitle = "Tự động nâng cấp kết nối HTTP sang HTTPS bảo mật",
                        checked = settings.isHttpsOnly,
                        onCheckedChange = { viewModel.setHttpsOnly(it) }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.VisibilityOff,
                        title = "Block Pop-ups & New Windows",
                        subtitle = "Ngăn chặn các cửa sổ pop-up và tab quảng cáo",
                        checked = isPopupBlockingEnabled,
                        onCheckedChange = { viewModel.setPopupBlockingEnabled(it) }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.CleaningServices,
                        title = "Block Malicious Redirects",
                        subtitle = "Ngăn chặn trang tự chuyển hướng trái phép",
                        checked = isRedirectBlockingEnabled,
                        onCheckedChange = { viewModel.setRedirectBlockingEnabled(it) }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.Security,
                        title = "Send 'Do Not Track' (DNT)",
                        subtitle = "Gửi yêu cầu không theo dõi hành vi duyệt web",
                        checked = settings.isDoNotTrackEnabled,
                        onCheckedChange = { viewModel.setDoNotTrackEnabled(it) }
                    )
                }
            }

            // Group 4: Media Sniffer & Downloader
            item {
                SettingsSectionTitle("MEDIA & DOWNLOADS")
                SettingsGroupContainer {
                    SettingsRowToggle(
                        icon = Icons.Default.Tv,
                        title = "Smart Media Sniffer",
                        subtitle = "Tự động phát hiện và bóc tách luồng HLS / MP4 để tải",
                        checked = settings.isMediaSnifferEnabled,
                        onCheckedChange = { viewModel.setMediaSnifferEnabled(it) }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowClickable(
                        icon = Icons.Default.Download,
                        title = "Preferred Video Quality",
                        subtitle = settings.preferredVideoQuality.displayName,
                        onClick = { showVideoQualityDialog = true }
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowClickable(
                        icon = Icons.Default.Speed,
                        title = "Download Thread Connections",
                        subtitle = "${settings.downloadThreads} parallel connection threads",
                        onClick = { showDownloadThreadsDialog = true }
                    )
                }
            }

            // Group 5: Tabs & Performance
            item {
                SettingsSectionTitle("TABS & PERFORMANCE")
                SettingsGroupContainer {
                    SettingsRowToggle(
                        icon = Icons.Default.Memory,
                        title = "Auto-Archive Inactive Tabs",
                        subtitle = "Tự động đưa tab ít dùng vào kho lưu trữ",
                        checked = settings.isAutoArchiveEnabled,
                        onCheckedChange = { viewModel.setAutoArchiveEnabled(it) }
                    )

                    if (settings.isAutoArchiveEnabled) {
                        HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)
                        SettingsRowClickable(
                            icon = Icons.Default.Storage,
                            title = "Archive Threshold",
                            subtitle = settings.autoArchiveDuration.displayName,
                            onClick = { showArchiveDurationDialog = true }
                        )
                    }

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowToggle(
                        icon = Icons.Default.Memory,
                        title = "Aggressive RAM Discarding",
                        subtitle = "Đóng băng tab chạy ngầm để tiết kiệm pin và RAM",
                        checked = settings.isAggressiveTabDiscarding,
                        onCheckedChange = { viewModel.setAggressiveTabDiscarding(it) }
                    )
                }
            }

            // Group 6: Data, Backup & History
            item {
                SettingsSectionTitle("DATA & BACKUP")
                SettingsGroupContainer {
                    SettingsRowClickable(
                        icon = Icons.Default.FolderZip,
                        title = "E2EE Backup & Bookmark Manager",
                        subtitle = "Xuất/nhập file HTML Netscape và mã hoá sao lưu AES-256",
                        onClick = onOpenBookmarks
                    )

                    HorizontalDivider(color = FireballBorder.copy(alpha = 0.5f), thickness = 0.5.dp)

                    SettingsRowClickable(
                        icon = Icons.Default.DeleteOutline,
                        title = "Clear Browsing Data",
                        subtitle = "Xóa lịch sử duyệt web, bộ nhớ tạm, dữ liệu tải về",
                        iconTint = FireballMeteorOrange,
                        onClick = { showClearDataDialog = true }
                    )
                }
            }

            // Group 7: About
            item {
                SettingsSectionTitle("ABOUT FIREBALL MINI")
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(16.dp))
                        .background(FireballRaisedSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(16.dp))
                        .padding(16.dp)
                ) {
                    Column {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Box(
                                modifier = Modifier
                                    .size(36.dp)
                                    .clip(CircleShape)
                                    .background(FireballMeteorOrange),
                                contentAlignment = Alignment.Center
                            ) {
                                Text("F", color = FireballBackground, fontWeight = FontWeight.Black, fontSize = 20.sp)
                            }
                            Spacer(modifier = Modifier.width(12.dp))
                            Column {
                                Text(
                                    text = "Fireball Mini Browser",
                                    style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                                    color = FireballPrimaryText
                                )
                                Text(
                                    text = "Version 1.0.0-PROD · Chromium Blink Architecture",
                                    style = MaterialTheme.typography.labelSmall,
                                    color = FireballMutedText
                                )
                            }
                        }

                        Spacer(modifier = Modifier.height(12.dp))
                        HorizontalDivider(color = FireballBorder)
                        Spacer(modifier = Modifier.height(10.dp))

                        Text(
                            text = "• Core: Google Blink Engine + Partition Isolation\n• Shields: adblock-rust 0.13.2 (Brave / uBlock Origin compatible)\n• Sync: BIP-39 Brave Sync Chain + Firefox Sync 1.5 (E2EE)\n• Acceleration: Multi-segment threaded downloader with HLS quality sniffing",
                            color = FireballSecondaryText,
                            fontSize = 11.5.sp,
                            lineHeight = 18.sp
                        )
                    }
                }
                Spacer(modifier = Modifier.height(20.dp))
            }
        }
    }
}

@Composable
private fun SettingsSectionTitle(title: String) {
    Text(
        text = title,
        style = MaterialTheme.typography.labelSmall,
        color = FireballElectricLime,
        fontWeight = FontWeight.Bold,
        letterSpacing = 0.5.sp,
        modifier = Modifier.padding(start = 4.dp, bottom = 6.dp)
    )
}

@Composable
private fun SettingsGroupContainer(content: @Composable () -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(16.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(16.dp))
    ) {
        content()
    }
}

@Composable
private fun SettingsRowClickable(
    icon: ImageVector,
    title: String,
    subtitle: String,
    iconTint: Color = FireballPrimaryText,
    onClick: () -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .padding(horizontal = 14.dp, vertical = 12.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.weight(1f)
        ) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = iconTint,
                modifier = Modifier.size(20.dp)
            )
            Spacer(modifier = Modifier.width(12.dp))
            Column {
                Text(
                    text = title,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                    color = FireballPrimaryText
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodyMedium,
                    color = FireballMutedText,
                    fontSize = 11.5.sp
                )
            }
        }

        Icon(
            imageVector = Icons.Default.ChevronRight,
            contentDescription = null,
            tint = FireballMutedText,
            modifier = Modifier.size(18.dp)
        )
    }
}

@Composable
private fun SettingsRowToggle(
    icon: ImageVector,
    title: String,
    subtitle: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 14.dp, vertical = 12.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.weight(1f)
        ) {
            Icon(
                imageVector = icon,
                contentDescription = title,
                tint = FireballPrimaryText,
                modifier = Modifier.size(20.dp)
            )
            Spacer(modifier = Modifier.width(12.dp))
            Column {
                Text(
                    text = title,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                    color = FireballPrimaryText
                )
                Text(
                    text = subtitle,
                    style = MaterialTheme.typography.bodyMedium,
                    color = FireballMutedText,
                    fontSize = 11.5.sp
                )
            }
        }

        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = FireballBackground,
                checkedTrackColor = FireballElectricLime,
                uncheckedThumbColor = FireballMutedText,
                uncheckedTrackColor = FireballDeepSurface
            )
        )
    }
}
