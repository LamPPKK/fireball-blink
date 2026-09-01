package com.fireball.mini.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.SheetState
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballBorder
import com.fireball.mini.ui.theme.FireballDeepSurface

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AdaptiveDialogContainer(
    onDismiss: () -> Unit,
    sheetState: SheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
    content: @Composable () -> Unit
) {
    val configuration = LocalConfiguration.current
    val isTabletOrPc = configuration.screenWidthDp >= 600

    if (isTabletOrPc) {
        Dialog(
            onDismissRequest = onDismiss,
            properties = DialogProperties(usePlatformDefaultWidth = false)
        ) {
            Box(
                modifier = Modifier
                    .widthIn(min = 460.dp, max = 640.dp)
                    .heightIn(max = 720.dp)
                    .clip(RoundedCornerShape(20.dp))
                    .background(FireballDeepSurface)
                    .border(1.dp, FireballBorder, RoundedCornerShape(20.dp))
                    .padding(vertical = 12.dp)
            ) {
                content()
            }
        }
    } else {
        ModalBottomSheet(
            onDismissRequest = onDismiss,
            sheetState = sheetState,
            containerColor = FireballDeepSurface,
            scrimColor = FireballBackground.copy(alpha = 0.75f),
            dragHandle = null
        ) {
            content()
        }
    }
}
