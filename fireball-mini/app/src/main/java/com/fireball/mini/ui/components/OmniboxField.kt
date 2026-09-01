package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material.icons.filled.Search
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.theme.FireballActiveSurface
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMeteorOrange
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface

@Composable
fun OmniboxField(
    urlText: String,
    isBurner: Boolean,
    onClick: () -> Unit,
    onReload: () -> Unit,
    modifier: Modifier = Modifier
) {
    val displayHost = when {
        urlText.isEmpty() || urlText == "about:blank" -> "Search or enter address"
        urlText.startsWith("https://") -> urlText.removePrefix("https://").substringBefore('/')
        urlText.startsWith("http://") -> urlText.removePrefix("http://").substringBefore('/')
        else -> urlText
    }

    val isSecure = urlText.startsWith("https://")

    Box(
        modifier = modifier
            .fillMaxWidth()
            .height(44.dp)
            .clip(RoundedCornerShape(14.dp))
            .background(FireballRaisedSurface)
            .border(
                1.dp,
                if (isBurner) FireballMeteorOrange.copy(alpha = 0.5f) else FireballBorder,
                RoundedCornerShape(14.dp)
            )
            .clickable { onClick() }
            .padding(horizontal = 10.dp),
        contentAlignment = Alignment.CenterStart
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier.fillMaxWidth()
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.weight(1f)
            ) {
                Icon(
                    imageVector = if (isSecure) Icons.Default.Lock else Icons.Default.Search,
                    contentDescription = "Security",
                    tint = if (isSecure) FireballElectricLime else FireballMutedText,
                    modifier = Modifier.size(15.dp)
                )

                Spacer(modifier = Modifier.width(8.dp))

                Text(
                    text = displayHost,
                    style = MaterialTheme.typography.bodyMedium.copy(
                        fontWeight = if (displayHost.contains('.')) FontWeight.SemiBold else FontWeight.Normal
                    ),
                    color = if (urlText.isEmpty() || urlText == "about:blank") FireballMutedText else FireballPrimaryText,
                    maxLines = 1,
                    overflow = TextOverflow.Ellipsis,
                    fontSize = 13.sp
                )
            }

            if (urlText.isNotEmpty() && urlText != "about:blank") {
                IconButton(
                    onClick = onReload,
                    modifier = Modifier.size(24.dp)
                ) {
                    Icon(
                        imageVector = Icons.Default.Refresh,
                        contentDescription = "Reload",
                        tint = FireballMutedText,
                        modifier = Modifier.size(15.dp)
                    )
                }
            }
        }
    }
}
