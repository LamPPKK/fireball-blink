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
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Block
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.CleaningServices
import androidx.compose.material.icons.filled.Cookie
import androidx.compose.material.icons.filled.Fingerprint
import androidx.compose.material.icons.filled.Group
import androidx.compose.material.icons.filled.Link
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Public
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material.icons.filled.Tune
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.BlockerMode
import com.fireball.mini.core.models.EgressMode
import com.fireball.mini.core.models.FilterCategory
import com.fireball.mini.core.models.FilterListSubscription
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
import com.fireball.mini.ui.theme.FireballTorPurple
import com.fireball.mini.ui.theme.FireballWarpBlue
import com.fireball.mini.ui.viewmodels.BrowserViewModel

import androidx.compose.material3.SheetState
import androidx.compose.material3.rememberModalBottomSheetState
import com.fireball.mini.ui.components.AdaptiveDialogContainer
import androidx.compose.material.icons.automirrored.filled.OpenInNew
import androidx.compose.material.icons.filled.Navigation

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ShieldsBottomSheet(
    viewModel: BrowserViewModel,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    onDismiss: () -> Unit
) {
    val uiState by viewModel.uiState.collectAsState()
    val stats by viewModel.stats.collectAsState()
    val egressStatus by viewModel.egressStatus.collectAsState()
    val filterLists by viewModel.filterLists.collectAsState()
    val blockerMode by viewModel.blockerMode.collectAsState()
    val isPopupBlockingEnabled by viewModel.isPopupBlockingEnabled.collectAsState()
    val isRedirectBlockingEnabled by viewModel.isRedirectBlockingEnabled.collectAsState()

    var isShieldsEnabled by remember { mutableStateOf(true) }
    var isUrlCleanerEnabled by remember { mutableStateOf(true) }
    var isHttpsUpgradeEnabled by remember { mutableStateOf(true) }

    val totalActiveRules = filterLists.filter { it.isEnabled }.sumOf { it.rulesCount }

    AdaptiveDialogContainer(
        onDismiss = onDismiss,
        sheetState = sheetState
    ) {
        LazyColumn(
            contentPadding = PaddingValues(horizontal = 20.dp, vertical = 16.dp),
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
        ) {
            // Drag handle pill
            item {
                Box(
                    modifier = Modifier
                        .fillMaxWidth(),
                    contentAlignment = Alignment.Center
                ) {
                    Box(
                        modifier = Modifier
                            .size(width = 36.dp, height = 4.dp)
                            .clip(RoundedCornerShape(2.dp))
                            .background(FireballBorder)
                    )
                }
                Spacer(modifier = Modifier.height(16.dp))
            }

            // Header Banner
            item {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Box(
                            modifier = Modifier
                                .size(38.dp)
                                .clip(CircleShape)
                                .background(FireballMeteorOrange.copy(alpha = 0.15f)),
                            contentAlignment = Alignment.Center
                        ) {
                            Icon(
                                imageVector = Icons.Default.Shield,
                                contentDescription = "Shields",
                                tint = FireballMeteorOrange,
                                modifier = Modifier.size(22.dp)
                            )
                        }
                        Spacer(modifier = Modifier.width(10.dp))
                        Column {
                            Text(
                                text = "Fireball Shields",
                                style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                                color = FireballPrimaryText
                            )
                            Text(
                                text = "187,000+ Filter Rules Active",
                                style = MaterialTheme.typography.bodySmall,
                                color = FireballElectricLime,
                                fontSize = 11.sp,
                                fontWeight = FontWeight.SemiBold
                            )
                        }
                    }

                    Switch(
                        checked = isShieldsEnabled,
                        onCheckedChange = { isShieldsEnabled = it },
                        colors = SwitchDefaults.colors(
                            checkedThumbColor = FireballBackground,
                            checkedTrackColor = FireballElectricLime,
                            uncheckedThumbColor = FireballMutedText,
                            uncheckedTrackColor = FireballRaisedSurface
                        )
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))
            }

            // Mode Selector Tabs (STANDARD / AGGRESSIVE / DISABLED)
            item {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(12.dp))
                        .background(FireballRaisedSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
                        .padding(4.dp)
                ) {
                    BlockerModeTab(
                        title = "Standard",
                        isSelected = blockerMode == BlockerMode.STANDARD,
                        onClick = { viewModel.setBlockerMode(BlockerMode.STANDARD) },
                        modifier = Modifier.weight(1f)
                    )
                    BlockerModeTab(
                        title = "Aggressive",
                        isSelected = blockerMode == BlockerMode.AGGRESSIVE,
                        onClick = { viewModel.setBlockerMode(BlockerMode.AGGRESSIVE) },
                        modifier = Modifier.weight(1f)
                    )
                    BlockerModeTab(
                        title = "Disabled",
                        isSelected = blockerMode == BlockerMode.DISABLED,
                        onClick = { viewModel.setBlockerMode(BlockerMode.DISABLED) },
                        modifier = Modifier.weight(1f)
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))
            }

            // 4-Card Analytics Grid
            item {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    StatCard(
                        title = "Ads Blocked",
                        value = "${stats.totalAdsBlocked}",
                        accentColor = FireballMeteorOrange,
                        modifier = Modifier.weight(1f)
                    )
                    StatCard(
                        title = "Trackers",
                        value = "${stats.totalTrackersBlocked}",
                        accentColor = FireballElectricLime,
                        modifier = Modifier.weight(1f)
                    )
                    StatCard(
                        title = "Annoyances",
                        value = "${stats.totalAnnoyancesBlocked}",
                        accentColor = Color(0xFF00F0FF),
                        modifier = Modifier.weight(1f)
                    )
                    StatCard(
                        title = "Data Saved",
                        value = "${stats.totalBandwidthSavedBytes / 1024 / 1024}MB",
                        accentColor = Color(0xFFA855F7),
                        modifier = Modifier.weight(1f)
                    )
                }

                Spacer(modifier = Modifier.height(18.dp))
            }

            // Active Filter Subscriptions Section Header
            item {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.SpaceBetween,
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(bottom = 8.dp)
                ) {
                    Text(
                        text = "FILTER LISTS & ENGINES (${filterLists.count { it.isEnabled }}/${filterLists.size})",
                        style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                        color = FireballSecondaryText,
                        fontSize = 11.sp
                    )
                    Text(
                        text = "$totalActiveRules rules",
                        style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.SemiBold),
                        color = FireballMutedText,
                        fontSize = 10.5.sp
                    )
                }
            }

            // Filter Subscriptions List
            items(filterLists, key = { it.id }) { filter ->
                FilterListCard(
                    filter = filter,
                    onToggle = { viewModel.toggleFilterList(filter.id) }
                )
                Spacer(modifier = Modifier.height(6.dp))
            }

            // Egress Routing Selector Section
            item {
                Spacer(modifier = Modifier.height(14.dp))
                Text(
                    text = "EGRESS ROUTING & IDENTITY",
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                    color = FireballSecondaryText,
                    fontSize = 11.sp,
                    modifier = Modifier.padding(bottom = 8.dp)
                )

                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(14.dp))
                        .background(FireballCardSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(14.dp))
                ) {
                    EgressOptionItem(
                        title = "Direct Internet",
                        subtitle = "Standard low-latency native socket",
                        latencyBadge = "12ms",
                        badgeColor = FireballElectricLime,
                        isSelected = egressStatus.mode == EgressMode.DIRECT,
                        onClick = { viewModel.setEgressMode(EgressMode.DIRECT) }
                    )

                    EgressOptionItem(
                        title = "Cloudflare WARP",
                        subtitle = "Encrypted egress + DNS-over-HTTPS",
                        latencyBadge = "18ms",
                        badgeColor = FireballWarpBlue,
                        isSelected = egressStatus.mode == EgressMode.WARP,
                        onClick = { viewModel.setEgressMode(EgressMode.WARP) }
                    )

                    EgressOptionItem(
                        title = "Tor Onion Circuit",
                        subtitle = "Triple hop anonymous relay (Per-space)",
                        latencyBadge = "Max Privacy",
                        badgeColor = FireballTorPurple,
                        isSelected = egressStatus.mode == EgressMode.TOR,
                        onClick = { viewModel.setEgressMode(EgressMode.TOR) }
                    )
                }

                Spacer(modifier = Modifier.height(14.dp))
            }

            // Granular Shield Protections
            item {
                Column(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clip(RoundedCornerShape(14.dp))
                        .background(FireballCardSurface)
                        .border(1.dp, FireballBorder, RoundedCornerShape(14.dp))
                ) {
                    ShieldToggleRow(
                        icon = Icons.AutoMirrored.Filled.OpenInNew,
                        label = "Block Unsolicited Popups & New Tabs",
                        checked = isPopupBlockingEnabled,
                        onCheckedChange = { viewModel.setPopupBlockingEnabled(it) }
                    )
                    ShieldToggleRow(
                        icon = Icons.Default.Navigation,
                        label = "Block Abusive Client Redirects & App Intents",
                        checked = isRedirectBlockingEnabled,
                        onCheckedChange = { viewModel.setRedirectBlockingEnabled(it) }
                    )
                    ShieldToggleRow(
                        icon = Icons.Default.Link,
                        label = "Strip Tracking URL Query Parameters",
                        checked = isUrlCleanerEnabled,
                        onCheckedChange = { isUrlCleanerEnabled = it }
                    )
                    ShieldToggleRow(
                        icon = Icons.Default.Lock,
                        label = "Upgrade All Connections to HTTPS",
                        checked = isHttpsUpgradeEnabled,
                        onCheckedChange = { isHttpsUpgradeEnabled = it }
                    )
                }

                Spacer(modifier = Modifier.height(24.dp))
            }
        }
    }
}

@Composable
private fun BlockerModeTab(
    title: String,
    isSelected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(if (isSelected) FireballActiveSurface else Color.Transparent)
            .border(
                width = if (isSelected) 1.dp else 0.dp,
                color = if (isSelected) FireballElectricLime else Color.Transparent,
                shape = RoundedCornerShape(8.dp)
            )
            .clickable { onClick() }
            .padding(vertical = 6.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text = title,
            style = MaterialTheme.typography.labelMedium.copy(
                fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium
            ),
            color = if (isSelected) FireballElectricLime else FireballMutedText,
            fontSize = 12.sp
        )
    }
}

@Composable
private fun FilterListCard(
    filter: FilterListSubscription,
    onToggle: () -> Unit
) {
    val icon = when (filter.category) {
        FilterCategory.ADS -> Icons.Default.Shield
        FilterCategory.TRACKERS -> Icons.Default.Fingerprint
        FilterCategory.ANNOYANCES -> Icons.Default.Cookie
        FilterCategory.SECURITY -> Icons.Default.Security
        FilterCategory.SOCIAL -> Icons.Default.Group
        FilterCategory.COSMETIC -> Icons.Default.CleaningServices
    }

    val iconColor = when (filter.category) {
        FilterCategory.ADS -> FireballMeteorOrange
        FilterCategory.TRACKERS -> FireballElectricLime
        FilterCategory.ANNOYANCES -> Color(0xFF00F0FF)
        FilterCategory.SECURITY -> Color(0xFFF59E0B)
        FilterCategory.SOCIAL -> Color(0xFFEC4899)
        FilterCategory.COSMETIC -> Color(0xFFA855F7)
    }

    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(12.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(horizontal = 12.dp, vertical = 10.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .weight(1f)
                .padding(end = 8.dp)
        ) {
            Box(
                modifier = Modifier
                    .size(34.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(iconColor.copy(alpha = 0.15f)),
                contentAlignment = Alignment.Center
            ) {
                Icon(
                    imageVector = icon,
                    contentDescription = null,
                    tint = iconColor,
                    modifier = Modifier.size(18.dp)
                )
            }

            Spacer(modifier = Modifier.width(10.dp))

            Column(modifier = Modifier.weight(1f)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = filter.name,
                        style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                        color = FireballPrimaryText,
                        fontSize = 12.5.sp,
                        maxLines = 1,
                        overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis,
                        modifier = Modifier.weight(1f, fill = false)
                    )
                    Spacer(modifier = Modifier.width(6.dp))
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(4.dp))
                            .background(FireballRaisedSurface)
                            .padding(horizontal = 5.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "${filter.rulesCount / 1000}k rules",
                            style = MaterialTheme.typography.labelSmall,
                            color = FireballMutedText,
                            fontSize = 8.5.sp,
                            maxLines = 1,
                            softWrap = false
                        )
                    }
                }
                Spacer(modifier = Modifier.height(2.dp))
                Text(
                    text = filter.description,
                    style = MaterialTheme.typography.bodySmall,
                    color = FireballMutedText,
                    fontSize = 10.5.sp,
                    maxLines = 1,
                    overflow = androidx.compose.ui.text.style.TextOverflow.Ellipsis
                )
            }
        }

        Switch(
            checked = filter.isEnabled,
            onCheckedChange = { onToggle() },
            colors = SwitchDefaults.colors(
                checkedThumbColor = FireballBackground,
                checkedTrackColor = iconColor,
                uncheckedThumbColor = FireballMutedText,
                uncheckedTrackColor = FireballRaisedSurface
            )
        )
    }
}

@Composable
private fun StatCard(
    title: String,
    value: String,
    accentColor: Color,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(FireballCardSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .padding(horizontal = 8.dp, vertical = 10.dp)
    ) {
        Column {
            Text(
                text = value,
                style = MaterialTheme.typography.titleMedium.copy(fontWeight = FontWeight.Bold),
                color = accentColor,
                fontSize = 15.sp
            )
            Spacer(modifier = Modifier.height(2.dp))
            Text(
                text = title,
                style = MaterialTheme.typography.labelSmall,
                color = FireballMutedText,
                fontSize = 9.5.sp
            )
        }
    }
}

@Composable
private fun EgressOptionItem(
    title: String,
    subtitle: String,
    latencyBadge: String,
    badgeColor: Color,
    isSelected: Boolean,
    onClick: () -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier
            .fillMaxWidth()
            .clickable { onClick() }
            .background(if (isSelected) FireballActiveSurface else FireballCardSurface)
            .padding(horizontal = 14.dp, vertical = 10.dp)
    ) {
        Column(modifier = Modifier.weight(1f)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.Bold),
                    color = if (isSelected) FireballPrimaryText else FireballSecondaryText
                )
                Spacer(modifier = Modifier.width(8.dp))
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(4.dp))
                        .background(badgeColor.copy(alpha = 0.15f))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                ) {
                    Text(
                        text = latencyBadge,
                        style = MaterialTheme.typography.labelSmall,
                        color = badgeColor,
                        fontSize = 9.sp
                    )
                }
            }
            Text(
                text = subtitle,
                style = MaterialTheme.typography.bodySmall,
                color = FireballMutedText,
                fontSize = 11.sp
            )
        }

        Box(
            modifier = Modifier
                .size(18.dp)
                .clip(CircleShape)
                .background(if (isSelected) FireballElectricLime else FireballRaisedSurface)
                .border(1.dp, if (isSelected) FireballElectricLime else FireballBorder, CircleShape),
            contentAlignment = Alignment.Center
        ) {
            if (isSelected) {
                Icon(
                    imageVector = Icons.Default.Check,
                    contentDescription = "Selected",
                    tint = FireballBackground,
                    modifier = Modifier.size(12.dp)
                )
            }
        }
    }
}

@Composable
private fun ShieldToggleRow(
    icon: ImageVector,
    label: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween,
        modifier = Modifier
            .fillMaxWidth()
            .padding(horizontal = 14.dp, vertical = 8.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.weight(1f)
        ) {
            Icon(
                imageVector = icon,
                contentDescription = label,
                tint = FireballSecondaryText,
                modifier = Modifier.size(18.dp)
            )
            Spacer(modifier = Modifier.width(10.dp))
            Text(
                text = label,
                style = MaterialTheme.typography.bodyMedium,
                color = FireballPrimaryText,
                fontSize = 13.sp
            )
        }

        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = FireballBackground,
                checkedTrackColor = FireballElectricLime,
                uncheckedThumbColor = FireballMutedText,
                uncheckedTrackColor = FireballRaisedSurface
            )
        )
    }
}
