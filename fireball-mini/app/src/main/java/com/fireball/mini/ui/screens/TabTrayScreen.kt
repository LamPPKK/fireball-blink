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
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.GridItemSpan
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.DeleteSweep
import androidx.compose.material.icons.filled.Edit
import androidx.compose.material.icons.filled.Language
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TextFieldDefaults
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.Space
import com.fireball.mini.core.models.TabSection
import com.fireball.mini.ui.components.AddSpaceChip
import com.fireball.mini.ui.components.SpaceChip
import com.fireball.mini.ui.components.SpaceIconHelper
import com.fireball.mini.ui.components.TabThumbnailCard
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

@Composable
fun TabTrayScreen(
    viewModel: BrowserViewModel,
    onCloseTray: () -> Unit
) {
    val spaces by viewModel.spaces.collectAsState()
    val activeSpaceId by viewModel.activeSpaceId.collectAsState()
    val tabs by viewModel.tabs.collectAsState()
    val activeTabId by viewModel.activeTabId.collectAsState()

    var showCreateSpaceDialog by remember { mutableStateOf(false) }
    var spaceToEdit by remember { mutableStateOf<Space?>(null) }
    var spaceToDelete by remember { mutableStateOf<Space?>(null) }

    // Space Form State
    var newSpaceName by remember { mutableStateOf("") }
    var selectedIconName by remember { mutableStateOf("work") }
    var selectedColorHex by remember { mutableStateOf("#B8FF3D") }

    val currentSpace = spaces.find { it.id == activeSpaceId } ?: spaces.first()
    val currentSpaceTabs = tabs.filter { it.spaceId == activeSpaceId }
    val isCurrentSpaceCustom = currentSpace.id != "space-main" && currentSpace.id != "space-incognito"

    val favorites = currentSpaceTabs.filter { it.section == TabSection.FAVORITE }
    val pinned = currentSpaceTabs.filter { it.section == TabSection.PINNED }
    val today = currentSpaceTabs.filter { it.section == TabSection.TODAY }

    val configuration = androidx.compose.ui.platform.LocalConfiguration.current
    val gridColumns = when {
        configuration.screenWidthDp >= 1000 -> 4
        configuration.screenWidthDp >= 600 -> 3
        else -> 2
    }

    Scaffold(
        topBar = {
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .background(FireballDeepSurface)
                    .border(1.dp, FireballBorder)
                    .statusBarsPadding()
                    .padding(vertical = 8.dp)
            ) {
                // Top Action Bar
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(horizontal = 12.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        IconButton(onClick = onCloseTray, modifier = Modifier.size(34.dp)) {
                            Icon(
                                imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                                contentDescription = "Done",
                                tint = FireballPrimaryText,
                                modifier = Modifier.size(20.dp)
                            )
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Text(
                            text = "Tabs & Spaces",
                            style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                            color = FireballPrimaryText
                        )
                    }

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        // Edit & Delete Custom Space buttons in Top Bar
                        if (isCurrentSpaceCustom) {
                            IconButton(
                                onClick = {
                                    spaceToEdit = currentSpace
                                    newSpaceName = currentSpace.name
                                    selectedIconName = currentSpace.iconName
                                    selectedColorHex = currentSpace.accentColorHex
                                },
                                modifier = Modifier.size(32.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Edit,
                                    contentDescription = "Edit Space",
                                    tint = FireballElectricLime,
                                    modifier = Modifier.size(17.dp)
                                )
                            }
                            Spacer(modifier = Modifier.width(2.dp))

                            IconButton(
                                onClick = { spaceToDelete = currentSpace },
                                modifier = Modifier.size(32.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.Delete,
                                    contentDescription = "Delete Space",
                                    tint = FireballMeteorOrange,
                                    modifier = Modifier.size(17.dp)
                                )
                            }
                            Spacer(modifier = Modifier.width(4.dp))
                        }

                        if (currentSpaceTabs.isNotEmpty()) {
                            IconButton(
                                onClick = { viewModel.closeAllTabsInSpace(currentSpace.id) },
                                modifier = Modifier.size(32.dp)
                            ) {
                                Icon(
                                    imageVector = Icons.Default.DeleteSweep,
                                    contentDescription = "Close all tabs",
                                    tint = FireballMutedText,
                                    modifier = Modifier.size(18.dp)
                                )
                            }
                            Spacer(modifier = Modifier.width(4.dp))
                        }

                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(8.dp))
                                .background(FireballRaisedSurface)
                                .border(1.dp, FireballBorder, RoundedCornerShape(8.dp))
                                .padding(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text(
                                text = "${currentSpaceTabs.size} active",
                                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.SemiBold),
                                color = if (currentSpace.isBurner) FireballMeteorOrange else FireballElectricLime
                            )
                        }
                    }
                }

                Spacer(modifier = Modifier.height(10.dp))

                // Spaces Horizontal Selector
                LazyRow(
                    contentPadding = PaddingValues(horizontal = 12.dp),
                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    items(spaces, key = { it.id }) { space ->
                        val spaceTabCount = tabs.count { it.spaceId == space.id }
                        val isCustomSpace = space.id != "space-main" && space.id != "space-incognito"
                        SpaceChip(
                            space = space,
                            tabCount = spaceTabCount,
                            isSelected = space.id == activeSpaceId,
                            canDelete = isCustomSpace,
                            onSpaceClick = { viewModel.selectSpace(space.id) },
                            onDeleteClick = { spaceToDelete = space },
                            onLongClick = {
                                if (isCustomSpace) {
                                    spaceToEdit = space
                                    newSpaceName = space.name
                                    selectedIconName = space.iconName
                                    selectedColorHex = space.accentColorHex
                                }
                            }
                        )
                    }

                    item {
                        AddSpaceChip(onClick = {
                            newSpaceName = ""
                            selectedIconName = "work"
                            selectedColorHex = "#B8FF3D"
                            showCreateSpaceDialog = true
                        })
                    }
                }
            }
        },
        bottomBar = {
            // Bottom Action Bar (Material 3 Surface with proper Button Hierarchy)
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween,
                modifier = Modifier
                    .fillMaxWidth()
                    .background(FireballDeepSurface)
                    .border(1.dp, FireballBorder.copy(alpha = 0.6f))
                    .navigationBarsPadding()
                    .padding(horizontal = 16.dp, vertical = 12.dp)
            ) {
                OutlinedButton(
                    onClick = { onCloseTray() },
                    shape = RoundedCornerShape(12.dp),
                    colors = ButtonDefaults.outlinedButtonColors(
                        contentColor = FireballPrimaryText
                    ),
                    border = androidx.compose.foundation.BorderStroke(1.dp, FireballBorder),
                    contentPadding = PaddingValues(horizontal = 18.dp, vertical = 10.dp)
                ) {
                    Text(
                        text = "Done",
                        style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.Bold),
                        fontSize = 14.sp
                    )
                }

                Button(
                    onClick = {
                        viewModel.createNewTab()
                        onCloseTray()
                    },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = if (currentSpace.isBurner) FireballMeteorOrange else FireballElectricLime,
                        contentColor = FireballBackground
                    ),
                    shape = RoundedCornerShape(12.dp),
                    contentPadding = PaddingValues(horizontal = 20.dp, vertical = 10.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Add,
                        contentDescription = "New Tab",
                        modifier = Modifier.size(18.dp)
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Text(
                        text = if (currentSpace.isBurner) "New Incognito Tab" else "New Tab",
                        style = MaterialTheme.typography.labelLarge.copy(fontWeight = FontWeight.Bold),
                        fontSize = 14.sp
                    )
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
            if (currentSpaceTabs.isEmpty()) {
                // Empty Space Launchpad State with Space Custom Icon
                EmptySpaceLaunchpad(
                    space = currentSpace,
                    onOpenSite = { url ->
                        viewModel.createNewTab(url)
                        onCloseTray()
                    }
                )
            } else {
                LazyVerticalGrid(
                    columns = GridCells.Fixed(gridColumns),
                    contentPadding = PaddingValues(16.dp),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp),
                    modifier = Modifier.fillMaxSize()
                ) {
                    // Section: Favorites
                    if (favorites.isNotEmpty()) {
                        item(span = { GridItemSpan(gridColumns) }) {
                            Text(
                                text = "FAVORITES",
                                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                                color = FireballMeteorOrange,
                                modifier = Modifier.padding(bottom = 4.dp)
                            )
                        }
                        items(favorites, key = { it.id }) { tab ->
                            TabThumbnailCard(
                                tab = tab,
                                isActive = tab.id == activeTabId,
                                allSpaces = spaces,
                                onTabClick = {
                                    viewModel.selectTab(tab.id)
                                    onCloseTray()
                                },
                                onCloseClick = { viewModel.closeTab(tab.id) },
                                onTogglePin = { viewModel.togglePinTab(tab.id) },
                                onToggleFavorite = { viewModel.toggleFavoriteTab(tab.id) },
                                onDuplicateTab = { viewModel.duplicateTab(tab.id) },
                                onMoveToSpace = { targetSpaceId -> viewModel.moveTabToSpace(tab.id, targetSpaceId) }
                            )
                        }
                    }

                    // Section: Pinned
                    if (pinned.isNotEmpty()) {
                        item(span = { GridItemSpan(gridColumns) }) {
                            Text(
                                text = "PINNED",
                                style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                                color = FireballElectricLime,
                                modifier = Modifier.padding(top = 8.dp, bottom = 4.dp)
                            )
                        }
                        items(pinned, key = { it.id }) { tab ->
                            TabThumbnailCard(
                                tab = tab,
                                isActive = tab.id == activeTabId,
                                allSpaces = spaces,
                                onTabClick = {
                                    viewModel.selectTab(tab.id)
                                    onCloseTray()
                                },
                                onCloseClick = { viewModel.closeTab(tab.id) },
                                onTogglePin = { viewModel.togglePinTab(tab.id) },
                                onToggleFavorite = { viewModel.toggleFavoriteTab(tab.id) },
                                onDuplicateTab = { viewModel.duplicateTab(tab.id) },
                                onMoveToSpace = { targetSpaceId -> viewModel.moveTabToSpace(tab.id, targetSpaceId) }
                            )
                        }
                    }

                    // Section: Open Tabs
                    item(span = { GridItemSpan(gridColumns) }) {
                        Text(
                            text = if (currentSpace.isBurner) "INCOGNITO TABS" else "OPEN TABS",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = if (currentSpace.isBurner) FireballMeteorOrange else FireballSecondaryText,
                            modifier = Modifier.padding(top = 8.dp, bottom = 4.dp)
                        )
                    }

                    items(today, key = { it.id }) { tab ->
                        TabThumbnailCard(
                            tab = tab,
                            isActive = tab.id == activeTabId,
                            allSpaces = spaces,
                            onTabClick = {
                                viewModel.selectTab(tab.id)
                                onCloseTray()
                            },
                            onCloseClick = { viewModel.closeTab(tab.id) },
                            onTogglePin = { viewModel.togglePinTab(tab.id) },
                            onToggleFavorite = { viewModel.toggleFavoriteTab(tab.id) },
                            onDuplicateTab = { viewModel.duplicateTab(tab.id) },
                            onMoveToSpace = { targetSpaceId -> viewModel.moveTabToSpace(tab.id, targetSpaceId) }
                        )
                    }
                }
            }
        }
    }

    // Dialog: Create New Space
    if (showCreateSpaceDialog) {
        SpaceEditDialog(
            title = "Create New Space",
            spaceName = newSpaceName,
            selectedIcon = selectedIconName,
            selectedColor = selectedColorHex,
            onNameChange = { newSpaceName = it },
            onIconSelected = { selectedIconName = it },
            onColorSelected = { selectedColorHex = it },
            confirmButtonText = "Create",
            onConfirm = {
                if (newSpaceName.isNotBlank()) {
                    viewModel.createSpace(
                        name = newSpaceName.trim(),
                        iconName = selectedIconName,
                        accentColorHex = selectedColorHex
                    )
                    showCreateSpaceDialog = false
                }
            },
            onDismiss = { showCreateSpaceDialog = false }
        )
    }

    // Dialog: Edit Space
    spaceToEdit?.let { space ->
        SpaceEditDialog(
            title = "Edit Space '${space.name}'",
            spaceName = newSpaceName,
            selectedIcon = selectedIconName,
            selectedColor = selectedColorHex,
            onNameChange = { newSpaceName = it },
            onIconSelected = { selectedIconName = it },
            onColorSelected = { selectedColorHex = it },
            confirmButtonText = "Save",
            onConfirm = {
                if (newSpaceName.isNotBlank()) {
                    viewModel.updateSpace(
                        spaceId = space.id,
                        name = newSpaceName.trim(),
                        iconName = selectedIconName,
                        accentColorHex = selectedColorHex
                    )
                    spaceToEdit = null
                }
            },
            onDismiss = { spaceToEdit = null }
        )
    }

    // Dialog: Delete Space Confirmation
    spaceToDelete?.let { space ->
        AlertDialog(
            onDismissRequest = { spaceToDelete = null },
            containerColor = FireballDeepSurface,
            title = {
                Text(text = "Delete Space '${space.name}'?", color = FireballPrimaryText, fontWeight = FontWeight.Bold)
            },
            text = {
                Text(
                    text = "All open tabs inside this space will be closed. You will return to the Main space.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = FireballMutedText
                )
            },
            confirmButton = {
                Button(
                    onClick = {
                        viewModel.deleteSpace(space.id)
                        spaceToDelete = null
                    },
                    colors = ButtonDefaults.buttonColors(
                        containerColor = FireballMeteorOrange,
                        contentColor = FireballBackground
                    )
                ) {
                    Text("Delete", fontWeight = FontWeight.Bold)
                }
            },
            dismissButton = {
                TextButton(onClick = { spaceToDelete = null }) {
                    Text("Cancel", color = FireballMutedText)
                }
            }
        )
    }
}

@Composable
private fun SpaceEditDialog(
    title: String,
    spaceName: String,
    selectedIcon: String,
    selectedColor: String,
    onNameChange: (String) -> Unit,
    onIconSelected: (String) -> Unit,
    onColorSelected: (String) -> Unit,
    confirmButtonText: String,
    onConfirm: () -> Unit,
    onDismiss: () -> Unit
) {
    val activeColor = try {
        Color(android.graphics.Color.parseColor(selectedColor))
    } catch (_: Exception) {
        FireballElectricLime
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        containerColor = FireballDeepSurface,
        title = {
            Text(text = title, color = FireballPrimaryText, fontWeight = FontWeight.Bold)
        },
        text = {
            Column {
                Text(
                    text = "Name & Profile",
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                    color = FireballSecondaryText
                )
                Spacer(modifier = Modifier.height(6.dp))
                OutlinedTextField(
                    value = spaceName,
                    onValueChange = onNameChange,
                    placeholder = { Text("e.g. Work, Crypto, Gaming") },
                    singleLine = true,
                    colors = TextFieldDefaults.colors(
                        focusedContainerColor = FireballRaisedSurface,
                        unfocusedContainerColor = FireballRaisedSurface,
                        cursorColor = activeColor,
                        focusedTextColor = FireballPrimaryText,
                        unfocusedTextColor = FireballPrimaryText,
                        focusedIndicatorColor = activeColor,
                        unfocusedIndicatorColor = FireballBorder
                    ),
                    modifier = Modifier.fillMaxWidth()
                )

                Spacer(modifier = Modifier.height(14.dp))

                // Icon Selection
                Text(
                    text = "Choose Space Icon",
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                    color = FireballSecondaryText
                )
                Spacer(modifier = Modifier.height(8.dp))

                LazyRow(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    items(SpaceIconHelper.AVAILABLE_ICONS, key = { it.id }) { iconOption ->
                        val isIconSelected = iconOption.id == selectedIcon
                        Box(
                            modifier = Modifier
                                .size(40.dp)
                                .clip(RoundedCornerShape(10.dp))
                                .background(if (isIconSelected) activeColor.copy(alpha = 0.2f) else FireballRaisedSurface)
                                .border(
                                    width = if (isIconSelected) 1.8.dp else 1.dp,
                                    color = if (isIconSelected) activeColor else FireballBorder,
                                    shape = RoundedCornerShape(10.dp)
                                )
                                .clickable { onIconSelected(iconOption.id) },
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = iconOption.icon,
                                contentDescription = iconOption.label,
                                tint = if (isIconSelected) activeColor else FireballMutedText,
                                modifier = Modifier.size(20.dp)
                            )
                        }
                    }
                }

                Spacer(modifier = Modifier.height(14.dp))

                // Accent Color Selection
                Text(
                    text = "Accent Color",
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                    color = FireballSecondaryText
                )
                Spacer(modifier = Modifier.height(8.dp))

                Row(
                    horizontalArrangement = Arrangement.spacedBy(10.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    SpaceIconHelper.PRESET_COLORS.forEach { colorHex ->
                        val color = try {
                            Color(android.graphics.Color.parseColor(colorHex))
                        } catch (_: Exception) {
                            FireballElectricLime
                        }
                        val isColorSelected = colorHex == selectedColor

                        Box(
                            modifier = Modifier
                                .size(28.dp)
                                .clip(CircleShape)
                                .background(color)
                                .clickable { onColorSelected(colorHex) },
                            contentAlignment = Alignment.Center
                        ) {
                            if (isColorSelected) {
                                Icon(
                                    imageVector = Icons.Default.Check,
                                    contentDescription = "Selected",
                                    tint = FireballBackground,
                                    modifier = Modifier.size(16.dp)
                                )
                            }
                        }
                    }
                }
            }
        },
        confirmButton = {
            Button(
                onClick = onConfirm,
                colors = ButtonDefaults.buttonColors(
                    containerColor = activeColor,
                    contentColor = FireballBackground
                )
            ) {
                Text(confirmButtonText, fontWeight = FontWeight.Bold)
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel", color = FireballMutedText)
            }
        }
    )
}

@Composable
private fun EmptySpaceLaunchpad(
    space: Space,
    onOpenSite: (String) -> Unit
) {
    val quickSites = listOf(
        Pair("DuckDuckGo", "https://duckduckgo.com"),
        Pair("GitHub", "https://github.com"),
        Pair("Hacker News", "https://news.ycombinator.com"),
        Pair("Reddit", "https://reddit.com"),
        Pair("YouTube", "https://m.youtube.com"),
        Pair("Wikipedia", "https://wikipedia.org")
    )

    val spaceColor = try {
        Color(android.graphics.Color.parseColor(space.accentColorHex))
    } catch (_: Exception) {
        if (space.isBurner) FireballMeteorOrange else FireballElectricLime
    }
    val spaceIcon = SpaceIconHelper.getIcon(space.iconName)

    Box(
        modifier = Modifier
            .fillMaxSize()
            .padding(24.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally,
            modifier = Modifier.fillMaxWidth()
        ) {
            Box(
                modifier = Modifier
                    .size(56.dp)
                    .clip(RoundedCornerShape(16.dp))
                    .background(FireballRaisedSurface)
                    .border(1.2.dp, spaceColor, RoundedCornerShape(16.dp)),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = spaceIcon,
                    contentDescription = space.name,
                    tint = spaceColor,
                    modifier = Modifier.size(28.dp)
                )
            }

            Spacer(modifier = Modifier.height(14.dp))

            Text(
                text = "No open tabs in ${space.name}",
                style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                color = FireballPrimaryText
            )

            Spacer(modifier = Modifier.height(4.dp))

            Text(
                text = if (space.isBurner) "Tabs in Incognito leave no cookies or history" else "Launch a site or tap '+ New Tab' below",
                style = MaterialTheme.typography.bodySmall,
                color = FireballMutedText
            )

            Spacer(modifier = Modifier.height(24.dp))

            // Quick Sites Grid
            Column(
                modifier = Modifier.fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                quickSites.chunked(2).forEach { rowSites ->
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        rowSites.forEach { site ->
                            Box(
                                modifier = Modifier
                                    .weight(1f)
                                    .clip(RoundedCornerShape(12.dp))
                                    .background(FireballCardSurface)
                                    .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
                                    .clickable { onOpenSite(site.second) }
                                    .padding(horizontal = 12.dp, vertical = 10.dp),
                                contentAlignment = Alignment.CenterStart
                            ) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Icon(
                                        imageVector = Icons.Default.Language,
                                        contentDescription = null,
                                        tint = spaceColor,
                                        modifier = Modifier.size(16.dp)
                                    )
                                    Spacer(modifier = Modifier.width(8.dp))
                                    Text(
                                        text = site.first,
                                        style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Medium),
                                        color = FireballPrimaryText,
                                        fontSize = 12.sp
                                    )
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
