package com.fireball.mini.ui.components

import androidx.compose.animation.AnimatedVisibility
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.automirrored.filled.ArrowForward
import androidx.compose.material.icons.filled.Download
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.Shield
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.theme.FireballSecondaryText

@Composable
fun BottomCommandDeck(
    urlText: String,
    isBurner: Boolean,
    canGoBack: Boolean,
    canGoForward: Boolean,
    tabCount: Int,
    adsBlockedCount: Long,
    discoveredMediaCount: Int,
    onOmniboxClick: () -> Unit,
    onReload: () -> Unit,
    onBackClick: () -> Unit,
    onForwardClick: () -> Unit,
    onShieldsClick: () -> Unit,
    onTabsClick: () -> Unit,
    onMediaClick: () -> Unit,
    onMenuClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .background(FireballDeepSurface)
            .border(1.dp, FireballBorder)
            .navigationBarsPadding()
            .padding(horizontal = 8.dp, vertical = 6.dp)
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth()
        ) {
            // Back Button
            IconButton(
                onClick = onBackClick,
                enabled = canGoBack,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.ArrowBack,
                    contentDescription = "Back",
                    tint = if (canGoBack) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f),
                    modifier = Modifier.size(20.dp)
                )
            }

            // Forward Button
            IconButton(
                onClick = onForwardClick,
                enabled = canGoForward,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.AutoMirrored.Filled.ArrowForward,
                    contentDescription = "Forward",
                    tint = if (canGoForward) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f),
                    modifier = Modifier.size(20.dp)
                )
            }

            // Omnibox Bar
            Box(
                modifier = Modifier
                    .weight(1f)
                    .padding(horizontal = 4.dp)
            ) {
                OmniboxField(
                    urlText = urlText,
                    isBurner = isBurner,
                    onClick = onOmniboxClick,
                    onReload = onReload
                )
            }

            // Shields Pill Badge
            Box(
                modifier = Modifier
                    .height(34.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(if (adsBlockedCount > 0) FireballMeteorOrange.copy(alpha = 0.15f) else FireballRaisedSurface)
                    .border(
                        1.dp,
                        if (adsBlockedCount > 0) FireballMeteorOrange.copy(alpha = 0.6f) else FireballBorder,
                        RoundedCornerShape(10.dp)
                    )
                    .clickable { onShieldsClick() }
                    .padding(horizontal = 6.dp),
                contentAlignment = Alignment.Center
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(
                        imageVector = Icons.Default.Shield,
                        contentDescription = "Shields",
                        tint = if (adsBlockedCount > 0) FireballMeteorOrange else FireballMutedText,
                        modifier = Modifier.size(16.dp)
                    )
                    if (adsBlockedCount > 0) {
                        Spacer(modifier = Modifier.width(3.dp))
                        Text(
                            text = if (adsBlockedCount > 99) "99+" else adsBlockedCount.toString(),
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = FireballMeteorOrange,
                            fontSize = 10.sp
                        )
                    }
                }
            }

            Spacer(modifier = Modifier.width(4.dp))

            // Tab Counter Button
            Box(
                modifier = Modifier
                    .size(34.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(FireballRaisedSurface)
                    .border(
                        1.dp,
                        if (isBurner) FireballMeteorOrange else FireballBorder,
                        RoundedCornerShape(10.dp)
                    )
                    .clickable { onTabsClick() },
                contentAlignment = Alignment.Center
            ) {
                Text(
                    text = tabCount.toString(),
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                    color = if (isBurner) FireballMeteorOrange else FireballPrimaryText,
                    fontSize = 12.sp
                )
            }

            // Menu Button
            IconButton(
                onClick = onMenuClick,
                modifier = Modifier.size(36.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.MoreVert,
                    contentDescription = "Menu",
                    tint = FireballPrimaryText,
                    modifier = Modifier.size(20.dp)
                )
            }
        }
    }
}
