package com.fireball.mini.ui.theme

import android.app.Activity
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.platform.LocalView
import androidx.core.view.WindowCompat

private val FireballDarkColorScheme = darkColorScheme(
    primary = FireballMeteorOrange,
    secondary = FireballElectricLime,
    tertiary = FireballElectricLime,
    background = FireballBackground,
    surface = FireballDeepSurface,
    surfaceVariant = FireballRaisedSurface,
    onPrimary = FireballBackground,
    onSecondary = FireballBackground,
    onBackground = FireballPrimaryText,
    onSurface = FireballPrimaryText,
    onSurfaceVariant = FireballSecondaryText,
    error = FireballDestructive,
    outline = FireballBorder
)

@Composable
fun FireballMiniTheme(
    isBurnerMode: Boolean = false,
    content: @Composable () -> Unit
) {
    val colorScheme = if (isBurnerMode) {
        FireballDarkColorScheme.copy(
            primary = FireballBurnerAccent,
            secondary = FireballBurnerAccent,
            surface = FireballBackground,
            surfaceVariant = FireballRaisedSurface
        )
    } else {
        FireballDarkColorScheme
    }

    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val window = (view.context as? Activity)?.window
            if (window != null) {
                WindowCompat.setDecorFitsSystemWindows(window, false)
                val insetsController = WindowCompat.getInsetsController(window, view)
                insetsController.isAppearanceLightStatusBars = false
                insetsController.isAppearanceLightNavigationBars = false
            }
        }
    }

    MaterialTheme(
        colorScheme = colorScheme,
        typography = Typography,
        content = content
    )
}
