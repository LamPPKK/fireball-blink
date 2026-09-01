package com.fireball.mini.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import android.widget.Toast
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Bookmark
import androidx.compose.material.icons.filled.Clear
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.CloudDownload
import androidx.compose.material.icons.filled.CloudUpload
import androidx.compose.material.icons.filled.DeleteOutline
import androidx.compose.material.icons.filled.FileDownload
import androidx.compose.material.icons.filled.FileUpload
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Search
import androidx.compose.material.icons.filled.Security
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.TabRowDefaults
import androidx.compose.material3.TabRowDefaults.tabIndicatorOffset
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.BookmarkItem
import com.fireball.mini.core.models.HistoryItem
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

@Composable
fun HistoryBookmarksScreen(
    viewModel: BrowserViewModel,
    initialTab: Int = 0, // 0 = History, 1 = Bookmarks
    onBack: () -> Unit,
    onNavigateToUrl: (String) -> Unit
) {
    var selectedTab by remember { mutableIntStateOf(initialTab) }
    var searchQuery by remember { mutableStateOf("") }

    val context = LocalContext.current
    val clipboardManager = LocalClipboardManager.current

    var showExportHtmlDialog by remember { mutableStateOf(false) }
    var showImportHtmlDialog by remember { mutableStateOf(false) }
    var showBackupDialog by remember { mutableStateOf(false) }
    var showRestoreDialog by remember { mutableStateOf(false) }

    var importHtmlText by remember { mutableStateOf("") }
    var backupPassphrase by remember { mutableStateOf("") }
    var restoreBundleText by remember { mutableStateOf("") }
    var generatedBackupText by remember { mutableStateOf("") }

    val historyItems by viewModel.history.collectAsState()
    val bookmarkItems by viewModel.bookmarks.collectAsState()

    val filteredHistory = historyItems.filter {
        searchQuery.isEmpty() || it.title.contains(searchQuery, ignoreCase = true) || it.url.contains(searchQuery, ignoreCase = true)
    }

    val filteredBookmarks = bookmarkItems.filter {
        searchQuery.isEmpty() || it.title.contains(searchQuery, ignoreCase = true) || it.url.contains(searchQuery, ignoreCase = true)
    }

    // Export HTML Dialog
    if (showExportHtmlDialog) {
        val htmlContent = remember { viewModel.exportBookmarksHtml() }
        AlertDialog(
            onDismissRequest = { showExportHtmlDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Text(text = "Export Bookmarks (Netscape HTML)", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
            },
            text = {
                Column {
                    Text(
                        text = "Standard HTML format compatible with Chrome, Firefox, Safari, Edge and Brave.",
                        color = FireballSecondaryText,
                        fontSize = 12.sp
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    OutlinedTextField(
                        value = htmlContent,
                        onValueChange = {},
                        readOnly = true,
                        modifier = Modifier.fillMaxWidth().height(140.dp)
                    )
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        clipboardManager.setText(AnnotatedString(htmlContent))
                        Toast.makeText(context, "Copied HTML to clipboard!", Toast.LENGTH_SHORT).show()
                        showExportHtmlDialog = false
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Copy HTML")
                }
            },
            dismissButton = {
                Button(
                    onClick = { showExportHtmlDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballRaisedSurface, contentColor = FireballMutedText)
                ) {
                    Text("Close")
                }
            }
        )
    }

    // Import HTML Dialog
    if (showImportHtmlDialog) {
        AlertDialog(
            onDismissRequest = { showImportHtmlDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Text(text = "Import Bookmarks (HTML)", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
            },
            text = {
                Column {
                    Text(
                        text = "Paste the Netscape Bookmark HTML content below:",
                        color = FireballSecondaryText,
                        fontSize = 12.sp
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    OutlinedTextField(
                        value = importHtmlText,
                        onValueChange = { importHtmlText = it },
                        placeholder = { Text("<!DOCTYPE NETSCAPE-Bookmark-file-1>...") },
                        modifier = Modifier.fillMaxWidth().height(140.dp)
                    )
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        val count = viewModel.importBookmarksHtml(importHtmlText)
                        Toast.makeText(context, "Imported $count bookmarks!", Toast.LENGTH_SHORT).show()
                        showImportHtmlDialog = false
                        importHtmlText = ""
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Import")
                }
            },
            dismissButton = {
                Button(
                    onClick = { showImportHtmlDialog = false },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballRaisedSurface, contentColor = FireballMutedText)
                ) {
                    Text("Cancel")
                }
            }
        )
    }

    // Encrypted Backup Dialog
    if (showBackupDialog) {
        AlertDialog(
            onDismissRequest = { showBackupDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.Security, contentDescription = null, tint = FireballElectricLime)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "E2EE Encrypted Backup", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    Text(
                        text = "Secured with AES-256-GCM + PBKDF2. Backs up Spaces, Bookmarks, and History.",
                        color = FireballSecondaryText,
                        fontSize = 12.sp
                    )
                    Spacer(modifier = Modifier.height(10.dp))
                    OutlinedTextField(
                        value = backupPassphrase,
                        onValueChange = { backupPassphrase = it },
                        label = { Text("Encryption Passphrase") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                    if (generatedBackupText.isNotEmpty()) {
                        Spacer(modifier = Modifier.height(10.dp))
                        OutlinedTextField(
                            value = generatedBackupText,
                            onValueChange = {},
                            readOnly = true,
                            label = { Text("Encrypted Payload") },
                            modifier = Modifier.fillMaxWidth().height(100.dp)
                        )
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        if (backupPassphrase.length < 4) {
                            Toast.makeText(context, "Passphrase must be at least 4 characters", Toast.LENGTH_SHORT).show()
                            return@Button
                        }
                        if (generatedBackupText.isEmpty()) {
                            generatedBackupText = viewModel.exportEncryptedBackup(backupPassphrase)
                        } else {
                            clipboardManager.setText(AnnotatedString(generatedBackupText))
                            Toast.makeText(context, "Encrypted backup copied to clipboard!", Toast.LENGTH_SHORT).show()
                            showBackupDialog = false
                            generatedBackupText = ""
                            backupPassphrase = ""
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text(if (generatedBackupText.isEmpty()) "Encrypt" else "Copy Payload")
                }
            },
            dismissButton = {
                Button(
                    onClick = {
                        showBackupDialog = false
                        generatedBackupText = ""
                        backupPassphrase = ""
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballRaisedSurface, contentColor = FireballMutedText)
                ) {
                    Text("Close")
                }
            }
        )
    }

    // Restore Encrypted Backup Dialog
    if (showRestoreDialog) {
        AlertDialog(
            onDismissRequest = { showRestoreDialog = false },
            containerColor = FireballCardSurface,
            title = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(imageVector = Icons.Default.CloudDownload, contentDescription = null, tint = FireballElectricLime)
                    Spacer(modifier = Modifier.width(8.dp))
                    Text(text = "Restore Encrypted Backup", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
                }
            },
            text = {
                Column {
                    Text(
                        text = "Paste encrypted backup JSON and enter the passphrase used during backup:",
                        color = FireballSecondaryText,
                        fontSize = 12.sp
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = restoreBundleText,
                        onValueChange = { restoreBundleText = it },
                        placeholder = { Text("Paste JSON payload here...") },
                        modifier = Modifier.fillMaxWidth().height(90.dp)
                    )
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedTextField(
                        value = backupPassphrase,
                        onValueChange = { backupPassphrase = it },
                        label = { Text("Passphrase") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        val success = viewModel.importEncryptedBackup(restoreBundleText, backupPassphrase)
                        if (success) {
                            Toast.makeText(context, "Backup restored successfully!", Toast.LENGTH_SHORT).show()
                            showRestoreDialog = false
                            restoreBundleText = ""
                            backupPassphrase = ""
                        } else {
                            Toast.makeText(context, "Failed to decrypt. Check password or payload.", Toast.LENGTH_LONG).show()
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = FireballElectricLime, contentColor = FireballBackground)
                ) {
                    Text("Decrypt & Restore")
                }
            },
            dismissButton = {
                Button(
                    onClick = { showRestoreDialog = false },
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
                // Top Header Row
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 8.dp, vertical = 6.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    IconButton(onClick = onBack) {
                        Icon(
                            imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                            contentDescription = "Back",
                            tint = FireballPrimaryText
                        )
                    }

                    Text(
                        text = if (selectedTab == 0) "Browsing History" else "Bookmarks",
                        style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                        color = FireballPrimaryText,
                        modifier = Modifier.weight(1f)
                    )

                    if (selectedTab == 0 && historyItems.isNotEmpty()) {
                        IconButton(onClick = { viewModel.clearAllHistory() }) {
                            Icon(
                                imageVector = Icons.Default.DeleteOutline,
                                contentDescription = "Clear all",
                                tint = FireballMeteorOrange
                            )
                        }
                    } else if (selectedTab == 1) {
                        IconButton(onClick = { showExportHtmlDialog = true }) {
                            Icon(
                                imageVector = Icons.Default.FileDownload,
                                contentDescription = "Export HTML",
                                tint = FireballElectricLime
                            )
                        }
                        IconButton(onClick = { showImportHtmlDialog = true }) {
                            Icon(
                                imageVector = Icons.Default.FileUpload,
                                contentDescription = "Import HTML",
                                tint = FireballSecondaryText
                            )
                        }
                        IconButton(onClick = { showBackupDialog = true }) {
                            Icon(
                                imageVector = Icons.Default.Security,
                                contentDescription = "E2EE Backup",
                                tint = FireballElectricLime
                            )
                        }
                        IconButton(onClick = { showRestoreDialog = true }) {
                            Icon(
                                imageVector = Icons.Default.CloudDownload,
                                contentDescription = "Restore",
                                tint = FireballSecondaryText
                            )
                        }
                    }
                }

                // Search Bar
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 16.dp, vertical = 6.dp)
                        .clip(RoundedCornerShape(12.dp))
                        .background(FireballRaisedSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
                ) {
                    TextField(
                        value = searchQuery,
                        onValueChange = { searchQuery = it },
                        placeholder = {
                            Text(
                                text = if (selectedTab == 0) "Search history..." else "Search bookmarks...",
                                color = FireballMutedText,
                                fontSize = 13.sp
                            )
                        },
                        leadingIcon = {
                            Icon(
                                imageVector = Icons.Default.Search,
                                contentDescription = null,
                                tint = FireballMutedText,
                                modifier = Modifier.size(18.dp)
                            )
                        },
                        trailingIcon = {
                            if (searchQuery.isNotEmpty()) {
                                IconButton(onClick = { searchQuery = "" }) {
                                    Icon(
                                        imageVector = Icons.Default.Clear,
                                        contentDescription = "Clear",
                                        tint = FireballMutedText,
                                        modifier = Modifier.size(16.dp)
                                    )
                                }
                            }
                        },
                        colors = TextFieldDefaults.colors(
                            focusedContainerColor = Color.Transparent,
                            unfocusedContainerColor = Color.Transparent,
                            disabledContainerColor = Color.Transparent,
                            cursorColor = FireballElectricLime,
                            focusedTextColor = FireballPrimaryText,
                            unfocusedTextColor = FireballPrimaryText,
                            focusedIndicatorColor = Color.Transparent,
                            unfocusedIndicatorColor = Color.Transparent
                        ),
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                }

                // Tabs: History vs Bookmarks
                TabRow(
                    selectedTabIndex = selectedTab,
                    containerColor = FireballDeepSurface,
                    contentColor = FireballElectricLime,
                    indicator = { tabPositions ->
                        TabRowDefaults.SecondaryIndicator(
                            modifier = Modifier.tabIndicatorOffset(tabPositions[selectedTab]),
                            color = FireballElectricLime,
                            height = 2.5.dp
                        )
                    },
                    divider = {
                        HorizontalDivider(color = FireballBorder, thickness = 1.dp)
                    }
                ) {
                    Tab(
                        selected = selectedTab == 0,
                        onClick = { selectedTab = 0 },
                        text = {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    imageVector = Icons.Default.History,
                                    contentDescription = null,
                                    modifier = Modifier.size(16.dp)
                                )
                                Spacer(modifier = Modifier.width(6.dp))
                                Text(
                                    text = "History (${historyItems.size})",
                                    fontWeight = if (selectedTab == 0) FontWeight.Bold else FontWeight.Normal,
                                    fontSize = 13.sp
                                )
                            }
                        },
                        selectedContentColor = FireballElectricLime,
                        unselectedContentColor = FireballMutedText
                    )

                    Tab(
                        selected = selectedTab == 1,
                        onClick = { selectedTab = 1 },
                        text = {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Icon(
                                    imageVector = Icons.Default.Bookmark,
                                    contentDescription = null,
                                    modifier = Modifier.size(16.dp)
                                )
                                Spacer(modifier = Modifier.width(6.dp))
                                Text(
                                    text = "Bookmarks (${bookmarkItems.size})",
                                    fontWeight = if (selectedTab == 1) FontWeight.Bold else FontWeight.Normal,
                                    fontSize = 13.sp
                                )
                            }
                        },
                        selectedContentColor = FireballElectricLime,
                        unselectedContentColor = FireballMutedText
                    )
                }
            }
        },
        bottomBar = {
            if (selectedTab == 0 && historyItems.isNotEmpty()) {
                Box(
                    modifier = Modifier
                        .fillMaxWidth()
                        .background(FireballDeepSurface)
                        .border(1.dp, FireballBorder)
                        .navigationBarsPadding()
                        .padding(horizontal = 16.dp, vertical = 10.dp)
                ) {
                    Button(
                        onClick = { viewModel.clearAllHistory() },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = FireballRaisedSurface,
                            contentColor = FireballMeteorOrange
                        ),
                        shape = RoundedCornerShape(10.dp),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Icon(
                            imageVector = Icons.Default.DeleteOutline,
                            contentDescription = null,
                            modifier = Modifier.size(18.dp)
                        )
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(text = "Clear Browsing Data", fontWeight = FontWeight.Bold)
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
            if (selectedTab == 0) {
                // History List
                if (filteredHistory.isEmpty()) {
                    EmptyHistoryPlaceholder(isSearch = searchQuery.isNotEmpty())
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        contentPadding = PaddingValues(16.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        items(filteredHistory, key = { it.id }) { item ->
                            HistoryRowCard(
                                item = item,
                                onClick = { onNavigateToUrl(item.url) },
                                onDelete = { viewModel.removeHistoryItem(item.id) }
                            )
                        }
                    }
                }
            } else {
                // Bookmarks List
                if (filteredBookmarks.isEmpty()) {
                    EmptyBookmarksPlaceholder(isSearch = searchQuery.isNotEmpty())
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        contentPadding = PaddingValues(16.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        items(filteredBookmarks, key = { it.id }) { bookmark ->
                            BookmarkRowCard(
                                item = bookmark,
                                onClick = { onNavigateToUrl(bookmark.url) },
                                onDelete = { viewModel.removeBookmark(bookmark.id) }
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun HistoryRowCard(
    item: HistoryItem,
    onClick: () -> Unit,
    onDelete: () -> Unit
) {
    val dateStr = remember(item.visitedAtTimestamp) {
        val sdf = SimpleDateFormat("HH:mm • dd/MM", Locale.getDefault())
        sdf.format(Date(item.visitedAtTimestamp))
    }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 52.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .clickable { onClick() }
            .padding(horizontal = 12.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(36.dp)
                .clip(CircleShape)
                .background(FireballRaisedSurface),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Default.History,
                contentDescription = null,
                tint = FireballElectricLime,
                modifier = Modifier.size(18.dp)
            )
        }

        Spacer(modifier = Modifier.width(12.dp))

        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = item.title,
                style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                color = FireballPrimaryText,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )

            Spacer(modifier = Modifier.height(2.dp))

            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = item.url,
                    style = MaterialTheme.typography.labelSmall,
                    color = FireballMutedText,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    modifier = Modifier.weight(1f, fill = false)
                )

                Spacer(modifier = Modifier.width(8.dp))

                Text(
                    text = "• $dateStr",
                    style = MaterialTheme.typography.labelSmall,
                    color = FireballSecondaryText,
                    fontSize = 10.sp
                )
            }
        }

        IconButton(
            onClick = onDelete,
            modifier = Modifier.size(32.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Delete",
                tint = FireballMutedText,
                modifier = Modifier.size(16.dp)
            )
        }
    }
}

@Composable
private fun BookmarkRowCard(
    item: BookmarkItem,
    onClick: () -> Unit,
    onDelete: () -> Unit
) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 52.dp)
            .clip(RoundedCornerShape(12.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .clickable { onClick() }
            .padding(horizontal = 12.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier
                .size(36.dp)
                .clip(CircleShape)
                .background(FireballRaisedSurface),
            contentAlignment = Alignment.Center
        ) {
            Icon(
                imageVector = Icons.Default.Bookmark,
                contentDescription = null,
                tint = FireballMeteorOrange,
                modifier = Modifier.size(18.dp)
            )
        }

        Spacer(modifier = Modifier.width(12.dp))

        Column(modifier = Modifier.weight(1f)) {
            Text(
                text = item.title,
                style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                color = FireballPrimaryText,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )

            Spacer(modifier = Modifier.height(2.dp))

            Text(
                text = item.url,
                style = MaterialTheme.typography.labelSmall,
                color = FireballMutedText,
                maxLines = 1,
                overflow = TextOverflow.Ellipsis
            )
        }

        IconButton(
            onClick = onDelete,
            modifier = Modifier.size(32.dp)
        ) {
            Icon(
                imageVector = Icons.Default.Close,
                contentDescription = "Delete",
                tint = FireballMutedText,
                modifier = Modifier.size(16.dp)
            )
        }
    }
}

@Composable
private fun EmptyHistoryPlaceholder(isSearch: Boolean) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Icon(
                imageVector = Icons.Default.History,
                contentDescription = null,
                tint = FireballMutedText.copy(alpha = 0.5f),
                modifier = Modifier.size(48.dp)
            )
            Spacer(modifier = Modifier.height(12.dp))
            Text(
                text = if (isSearch) "No history matches your search" else "No browsing history yet",
                style = MaterialTheme.typography.bodyMedium,
                color = FireballMutedText
            )
        }
    }
}

@Composable
private fun EmptyBookmarksPlaceholder(isSearch: Boolean) {
    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(32.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            Icon(
                imageVector = Icons.Default.Bookmark,
                contentDescription = null,
                tint = FireballMutedText.copy(alpha = 0.5f),
                modifier = Modifier.size(48.dp)
            )
            Spacer(modifier = Modifier.height(12.dp))
            Text(
                text = if (isSearch) "No bookmarks match your search" else "No saved bookmarks yet",
                style = MaterialTheme.typography.bodyMedium,
                color = FireballMutedText
            )
        }
    }
}
