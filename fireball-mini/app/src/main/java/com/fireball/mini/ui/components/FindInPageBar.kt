package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Close
import androidx.compose.material.icons.filled.KeyboardArrowDown
import androidx.compose.material.icons.filled.KeyboardArrowUp
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextField
import androidx.compose.material3.TextFieldDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballMutedText
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.theme.FireballRaisedSurface
import com.fireball.mini.ui.theme.FireballSecondaryText

@Composable
fun FindInPageBar(
    query: String,
    currentIndex: Int,
    totalMatches: Int,
    onQueryChange: (String) -> Unit,
    onFindPrevious: () -> Unit,
    onFindNext: () -> Unit,
    onClose: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier
            .fillMaxWidth()
            .background(FireballDeepSurface)
            .border(1.dp, FireballBorder)
            .statusBarsPadding()
            .padding(horizontal = 12.dp, vertical = 6.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            // Input Text Field
            Box(
                modifier = Modifier
                    .weight(1f)
                    .height(38.dp)
                    .clip(RoundedCornerShape(8.dp))
                    .background(FireballRaisedSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(8.dp)),
                contentAlignment = Alignment.CenterStart
            ) {
                TextField(
                    value = query,
                    onValueChange = onQueryChange,
                    placeholder = {
                        Text(
                            text = "Find in page...",
                            color = FireballMutedText,
                            fontSize = 13.sp
                        )
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

            Spacer(modifier = Modifier.width(8.dp))

            // Match Count Badge
            val matchText = if (query.isEmpty()) "" else if (totalMatches == 0) "0/0" else "${currentIndex + 1}/$totalMatches"
            if (matchText.isNotEmpty()) {
                Text(
                    text = matchText,
                    style = MaterialTheme.typography.labelSmall.copy(fontWeight = FontWeight.Bold),
                    color = if (totalMatches > 0) FireballElectricLime else FireballMutedText,
                    fontSize = 11.sp,
                    modifier = Modifier.padding(horizontal = 4.dp)
                )
            }

            // Previous Button
            IconButton(
                onClick = onFindPrevious,
                enabled = totalMatches > 0,
                modifier = Modifier.size(32.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.KeyboardArrowUp,
                    contentDescription = "Previous match",
                    tint = if (totalMatches > 0) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f),
                    modifier = Modifier.size(20.dp)
                )
            }

            // Next Button
            IconButton(
                onClick = onFindNext,
                enabled = totalMatches > 0,
                modifier = Modifier.size(32.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.KeyboardArrowDown,
                    contentDescription = "Next match",
                    tint = if (totalMatches > 0) FireballPrimaryText else FireballMutedText.copy(alpha = 0.4f),
                    modifier = Modifier.size(20.dp)
                )
            }

            // Close Button
            IconButton(
                onClick = onClose,
                modifier = Modifier.size(32.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Close,
                    contentDescription = "Close find",
                    tint = FireballPrimaryText,
                    modifier = Modifier.size(18.dp)
                )
            }
        }
    }
}
