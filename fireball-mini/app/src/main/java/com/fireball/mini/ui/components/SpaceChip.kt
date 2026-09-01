package com.fireball.mini.ui.components

import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Add
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.LocalFireDepartment
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.core.models.Space
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

@OptIn(ExperimentalFoundationApi::class)
@Composable
fun SpaceChip(
    space: Space,
    tabCount: Int,
    isSelected: Boolean,
    onSpaceClick: () -> Unit,
    canDelete: Boolean = false,
    onDeleteClick: (() -> Unit)? = null,
    onLongClick: (() -> Unit)? = null,
    modifier: Modifier = Modifier
) {
    val accentColor = try {
        Color(android.graphics.Color.parseColor(space.accentColorHex))
    } catch (_: Exception) {
        if (space.isBurner) FireballMeteorOrange else FireballElectricLime
    }

    val spaceIcon = if (space.isBurner) {
        Icons.Default.LocalFireDepartment
    } else {
        SpaceIconHelper.getIcon(space.iconName)
    }

    Box(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(if (isSelected) FireballActiveSurface else FireballRaisedSurface)
            .border(
                width = if (isSelected) 1.8.dp else 1.dp,
                color = if (isSelected) accentColor else FireballBorder,
                shape = RoundedCornerShape(12.dp)
            ),
        contentAlignment = Alignment.Center
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier.padding(vertical = 4.dp)
        ) {
            // Main clickable body with Long-Click support for edit / delete
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier
                    .combinedClickable(
                        onClick = onSpaceClick,
                        onLongClick = onLongClick
                    )
                    .padding(start = 10.dp, end = if (canDelete) 2.dp else 10.dp, top = 4.dp, bottom = 4.dp)
            ) {
                Icon(
                    imageVector = spaceIcon,
                    contentDescription = space.name,
                    tint = if (isSelected) accentColor else FireballMutedText,
                    modifier = Modifier.size(16.dp)
                )
                Spacer(modifier = Modifier.width(6.dp))

                Text(
                    text = space.name,
                    style = MaterialTheme.typography.bodyMedium.copy(
                        fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Medium
                    ),
                    color = if (isSelected) FireballPrimaryText else FireballMutedText,
                    fontSize = 13.sp
                )

                if (tabCount > 0) {
                    Spacer(modifier = Modifier.width(8.dp))
                    Box(
                        modifier = Modifier
                            .clip(RoundedCornerShape(6.dp))
                            .background(if (isSelected) accentColor.copy(alpha = 0.2f) else FireballDeepSurface)
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    ) {
                        Text(
                            text = "$tabCount",
                            style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                            color = if (isSelected) accentColor else FireballMutedText,
                            fontSize = 10.sp
                        )
                    }
                }
            }

            // Explicit Delete Icon button
            if (canDelete && onDeleteClick != null) {
                IconButton(
                    onClick = onDeleteClick,
                    modifier = Modifier.size(28.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Close,
                        contentDescription = "Delete Space",
                        tint = FireballMutedText,
                        modifier = Modifier.size(13.dp)
                    )
                }
            }
        }
    }
}

@Composable
fun AddSpaceChip(
    onClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(12.dp))
            .background(FireballRaisedSurface)
            .border(1.dp, FireballBorder, RoundedCornerShape(12.dp))
            .clickable { onClick() }
            .padding(horizontal = 12.dp, vertical = 8.dp),
        contentAlignment = Alignment.Center
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                imageVector = Icons.Default.Add,
                contentDescription = "New Space",
                tint = FireballElectricLime,
                modifier = Modifier.size(16.dp)
            )
            Spacer(modifier = Modifier.width(4.dp))
            Text(
                text = "New Space",
                style = MaterialTheme.typography.bodyMedium.copy(fontWeight = FontWeight.SemiBold),
                color = FireballElectricLime,
                fontSize = 13.sp
            )
        }
    }
}
