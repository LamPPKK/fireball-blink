package com.fireball.mini.ui.components

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.MenuBook
import androidx.compose.material.icons.filled.Bolt
import androidx.compose.material.icons.filled.Code
import androidx.compose.material.icons.filled.Language
import androidx.compose.material.icons.filled.LocalFireDepartment
import androidx.compose.material.icons.filled.Movie
import androidx.compose.material.icons.filled.RocketLaunch
import androidx.compose.material.icons.filled.Security
import androidx.compose.material.icons.filled.ShoppingCart
import androidx.compose.material.icons.filled.SportsEsports
import androidx.compose.material.icons.filled.Star
import androidx.compose.material.icons.filled.Work
import androidx.compose.ui.graphics.vector.ImageVector

data class SpaceIconOption(
    val id: String,
    val label: String,
    val icon: ImageVector
)

object SpaceIconHelper {
    val AVAILABLE_ICONS = listOf(
        SpaceIconOption("globe", "Web", Icons.Default.Language),
        SpaceIconOption("work", "Work", Icons.Default.Work),
        SpaceIconOption("code", "Dev", Icons.Default.Code),
        SpaceIconOption("rocket", "Rocket", Icons.Default.RocketLaunch),
        SpaceIconOption("bolt", "Crypto", Icons.Default.Bolt),
        SpaceIconOption("shield", "Shield", Icons.Default.Security),
        SpaceIconOption("cart", "Shop", Icons.Default.ShoppingCart),
        SpaceIconOption("study", "Study", Icons.AutoMirrored.Filled.MenuBook),
        SpaceIconOption("game", "Game", Icons.Default.SportsEsports),
        SpaceIconOption("movie", "Media", Icons.Default.Movie),
        SpaceIconOption("star", "Star", Icons.Default.Star),
        SpaceIconOption("fire", "Burner", Icons.Default.LocalFireDepartment)
    )

    val PRESET_COLORS = listOf(
        "#B8FF3D", // Electric Lime
        "#FF5A1F", // Meteor Orange
        "#00F0FF", // Cyber Cyan
        "#A855F7", // Neon Purple
        "#F59E0B", // Amber Gold
        "#EC4899", // Hot Pink
        "#10B981"  // Emerald Green
    )

    fun getIcon(iconName: String): ImageVector {
        return AVAILABLE_ICONS.find { it.id == iconName }?.icon ?: Icons.Default.Language
    }
}
