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
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Check
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.rememberModalBottomSheetState
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.PermissionStatus
import com.fireball.mini.core.models.SitePermissionType
import com.fireball.mini.data.SiteSettingsRepository
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.viewmodels.BrowserViewModel

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SiteInfoBottomSheet(
    viewModel: BrowserViewModel,
    onDismiss: () -> Unit
) {
    val sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)
    val uiState by viewModel.uiState.collectAsState()
    val cleanDomain = remember(uiState.currentUrl) {
        SiteSettingsRepository.extractCleanDomain(uiState.currentUrl)
    }
    val siteInfo = remember(uiState.currentUrl) {
        viewModel.getSiteInfo(uiState.currentUrl)
    }
    val isHttps = uiState.currentUrl.startsWith("https://", ignoreCase = true)
    var showDataClearedToast by remember { mutableStateOf(false) }

    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = sheetState,
        containerColor = FireballCardSurface,
        scrimColor = Color.Black.copy(alpha = 0.6f),
        shape = RoundedCornerShape(topStart = 24.dp, topEnd = 24.dp),
        dragHandle = {
            Box(
                modifier = Modifier
                    .padding(vertical = 12.dp)
                    .width(48.dp)
                    .height(4.dp)
                    .clip(RoundedCornerShape(2.dp))
                    .background(FireballBorder)
            )
        }
    ) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .navigationBarsPadding()
                .padding(horizontal = 20.dp, vertical = 8.dp)
                .verticalScroll(rememberScrollState())
        ) {
            // Header: Domain & Security Status
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.SpaceBetween
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Box(
                        modifier = Modifier
                            .size(40.dp)
                            .clip(CircleShape)
                            .background(if (isHttps) FireballElectricLime.copy(alpha = 0.15f) else FireballMeteorOrange.copy(alpha = 0.15f)),
                        contentAlignment = Alignment.Center
                    ) {
                        Icon(
                            imageVector = if (isHttps) Icons.Default.Lock else Icons.Default.Warning,
                            contentDescription = null,
                            tint = if (isHttps) FireballElectricLime else FireballMeteorOrange,
                            modifier = Modifier.size(20.dp)
                        )
                    }
                    Spacer(modifier = Modifier.width(12.dp))
                    Column {
                        Text(
                            text = cleanDomain,
                            color = FireballPrimaryText,
                            fontSize = 18.sp,
                            fontWeight = FontWeight.Bold,
                            maxLines = 1,
                            overflow = TextOverflow.Ellipsis
                        )
                        Text(
                            text = if (isHttps) "Kết nối bảo mật (HTTPS SSL)" else "Kết nối không an toàn (HTTP)",
                            color = if (isHttps) FireballElectricLime else FireballMeteorOrange,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.Medium
                        )
                    }
                }
                IconButton(
                    onClick = onDismiss,
                    modifier = Modifier.size(36.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Đóng",
                        tint = FireballMutedText,
                        modifier = Modifier.size(20.dp)
                    )
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Card 1: Site Stats (Cookies, Storage, Trackers Blocked)
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(16.dp))
                    .background(FireballRaisedSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(16.dp))
                    .padding(16.dp)
            ) {
                Text(
                    text = "Dữ liệu & Quyền riêng tư",
                    color = FireballPrimaryText,
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold
                )
                Spacer(modifier = Modifier.height(12.dp))
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween
                ) {
                    SiteStatItem(label = "Cookies", value = "${siteInfo.cookieCount}")
                    SiteStatItem(label = "Dung lượng", value = siteInfo.formattedStorageSize)
                    SiteStatItem(label = "Quảng cáo chặn", value = "${uiState.adsBlockedThisSession}")
                }
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Card 2: Permissions Controls
            Text(
                text = "Quyền truy cập của trang (Permissions)",
                color = FireballPrimaryText,
                fontSize = 15.sp,
                fontWeight = FontWeight.Bold
            )
            Spacer(modifier = Modifier.height(8.dp))

            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .clip(RoundedCornerShape(16.dp))
                    .background(FireballRaisedSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(16.dp))
            ) {
                SitePermissionRow(
                    type = SitePermissionType.LOCATION,
                    currentStatus = viewModel.getSiteInfo(uiState.currentUrl).permissions[SitePermissionType.LOCATION] ?: PermissionStatus.ASK,
                    onStatusSelected = { status ->
                        viewModel.setSitePermission(cleanDomain, SitePermissionType.LOCATION, status)
                    }
                )
                Box(modifier = Modifier.fillMaxWidth().height(1.dp).background(FireballBorder))
                SitePermissionRow(
                    type = SitePermissionType.CAMERA,
                    currentStatus = viewModel.getSiteInfo(uiState.currentUrl).permissions[SitePermissionType.CAMERA] ?: PermissionStatus.ASK,
                    onStatusSelected = { status ->
                        viewModel.setSitePermission(cleanDomain, SitePermissionType.CAMERA, status)
                    }
                )
                Box(modifier = Modifier.fillMaxWidth().height(1.dp).background(FireballBorder))
                SitePermissionRow(
                    type = SitePermissionType.MICROPHONE,
                    currentStatus = viewModel.getSiteInfo(uiState.currentUrl).permissions[SitePermissionType.MICROPHONE] ?: PermissionStatus.ASK,
                    onStatusSelected = { status ->
                        viewModel.setSitePermission(cleanDomain, SitePermissionType.MICROPHONE, status)
                    }
                )
                Box(modifier = Modifier.fillMaxWidth().height(1.dp).background(FireballBorder))
                SitePermissionRow(
                    type = SitePermissionType.NOTIFICATIONS,
                    currentStatus = viewModel.getSiteInfo(uiState.currentUrl).permissions[SitePermissionType.NOTIFICATIONS] ?: PermissionStatus.ASK,
                    onStatusSelected = { status ->
                        viewModel.setSitePermission(cleanDomain, SitePermissionType.NOTIFICATIONS, status)
                    }
                )
                Box(modifier = Modifier.fillMaxWidth().height(1.dp).background(FireballBorder))
                SitePermissionRow(
                    type = SitePermissionType.POPUPS,
                    currentStatus = viewModel.getSiteInfo(uiState.currentUrl).permissions[SitePermissionType.POPUPS] ?: PermissionStatus.BLOCK,
                    onStatusSelected = { status ->
                        viewModel.setSitePermission(cleanDomain, SitePermissionType.POPUPS, status)
                    }
                )
            }

            Spacer(modifier = Modifier.height(16.dp))

            // Action: Clear Site Data
            Button(
                onClick = {
                    viewModel.clearSiteData(cleanDomain) {
                        showDataClearedToast = true
                    }
                },
                modifier = Modifier
                    .fillMaxWidth()
                    .height(48.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = FireballMeteorOrange.copy(alpha = 0.15f),
                    contentColor = FireballMeteorOrange
                ),
                shape = RoundedCornerShape(12.dp)
            ) {
                Icon(imageVector = Icons.Default.Delete, contentDescription = null, modifier = Modifier.size(18.dp))
                Spacer(modifier = Modifier.width(8.dp))
                Text(
                    text = if (showDataClearedToast) "Đã xóa Cookies & Dữ liệu của trang!" else "Xóa Cookie & Bộ nhớ của trang này",
                    fontSize = 14.sp,
                    fontWeight = FontWeight.SemiBold
                )
            }

            Spacer(modifier = Modifier.height(12.dp))
        }
    }
}

@Composable
private fun SiteStatItem(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(text = value, color = FireballElectricLime, fontSize = 16.sp, fontWeight = FontWeight.Bold)
        Text(text = label, color = FireballMutedText, fontSize = 11.sp)
    }
}

@Composable
private fun SitePermissionRow(
    type: SitePermissionType,
    currentStatus: PermissionStatus,
    onStatusSelected: (PermissionStatus) -> Unit
) {
    var expanded by remember { mutableStateOf(false) }

    Row(
        modifier = Modifier
            .fillMaxWidth()
            .clickable { expanded = true }
            .padding(horizontal = 16.dp, vertical = 12.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(text = type.iconEmoji, fontSize = 16.sp)
            Spacer(modifier = Modifier.width(10.dp))
            Text(
                text = type.displayName,
                color = FireballPrimaryText,
                fontSize = 13.sp,
                fontWeight = FontWeight.Medium
            )
        }

        Box {
            Text(
                text = when (currentStatus) {
                    PermissionStatus.ALLOW -> "Cho phép"
                    PermissionStatus.BLOCK -> "Chặn"
                    PermissionStatus.ASK -> "Hỏi trước"
                },
                color = when (currentStatus) {
                    PermissionStatus.ALLOW -> FireballElectricLime
                    PermissionStatus.BLOCK -> FireballMeteorOrange
                    PermissionStatus.ASK -> FireballMutedText
                },
                fontSize = 12.sp,
                fontWeight = FontWeight.SemiBold
            )

            DropdownMenu(
                expanded = expanded,
                onDismissRequest = { expanded = false },
                modifier = Modifier.background(FireballRaisedSurface)
            ) {
                DropdownMenuItem(
                    text = { Text("Cho phép (Allow)", color = FireballElectricLime) },
                    onClick = {
                        onStatusSelected(PermissionStatus.ALLOW)
                        expanded = false
                    }
                )
                DropdownMenuItem(
                    text = { Text("Chặn (Block)", color = FireballMeteorOrange) },
                    onClick = {
                        onStatusSelected(PermissionStatus.BLOCK)
                        expanded = false
                    }
                )
                DropdownMenuItem(
                    text = { Text("Hỏi trước (Ask)", color = FireballPrimaryText) },
                    onClick = {
                        onStatusSelected(PermissionStatus.ASK)
                        expanded = false
                    }
                )
            }
        }
    }
}
