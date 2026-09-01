package com.fireball.mini

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.viewModels
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.fireball.mini.ui.screens.AiAssistantBottomSheet
import com.fireball.mini.ui.screens.BrowserScreen
import com.fireball.mini.ui.screens.ChromiumMenuSheet
import com.fireball.mini.ui.screens.HistoryBookmarksScreen
import com.fireball.mini.ui.screens.MediaDownloadBottomSheet
import com.fireball.mini.ui.screens.ReaderModeScreen
import com.fireball.mini.ui.screens.SettingsScreen
import com.fireball.mini.ui.screens.ShieldsBottomSheet
import com.fireball.mini.ui.screens.SyncBottomSheet
import com.fireball.mini.ui.screens.TabTrayScreen
import com.fireball.mini.ui.screens.TransfersScreen
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballMiniTheme
import com.fireball.mini.ui.viewmodels.BrowserViewModel

class MainActivity : ComponentActivity() {

    private val browserViewModel: BrowserViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            FireballMiniTheme {
                Surface(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(FireballBackground),
                    color = MaterialTheme.colorScheme.background
                ) {
                    FireballAppNavigation(browserViewModel)
                }
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun FireballAppNavigation(browserViewModel: BrowserViewModel) {
    val context = LocalContext.current
    LaunchedEffect(Unit) {
        browserViewModel.initTts(context)
    }

    val navController = rememberNavController()
    val uiState by browserViewModel.uiState.collectAsState()
    var showShieldsSheet by remember { mutableStateOf(false) }
    var showMediaSheet by remember { mutableStateOf(false) }
    var showMenuSheet by remember { mutableStateOf(false) }
    var showAiAssistantSheet by remember { mutableStateOf(false) }
    var showFindInPage by remember { mutableStateOf(false) }
    var showSyncSheet by remember { mutableStateOf(false) }
    var isDesktopMode by remember { mutableStateOf(false) }

    NavHost(navController = navController, startDestination = "browser") {
        composable("browser") {
            BrowserScreen(
                viewModel = browserViewModel,
                showFindInPage = showFindInPage,
                onCloseFindInPage = { showFindInPage = false },
                onNavigateToTabs = { navController.navigate("tabs") },
                onOpenShields = { showShieldsSheet = true },
                onOpenMediaDownloads = { showMediaSheet = true },
                onOpenAiAssistant = { showAiAssistantSheet = true },
                onOpenMenu = { showMenuSheet = true },
                onToggleBookmark = { browserViewModel.toggleBookmark() },
                isDesktopMode = isDesktopMode
            )

            if (showAiAssistantSheet) {
                AiAssistantBottomSheet(
                    viewModel = browserViewModel,
                    sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
                    onDismiss = { showAiAssistantSheet = false },
                    onOpenReaderMode = { navController.navigate("reader_mode") }
                )
            }

            if (showShieldsSheet) {
                ShieldsBottomSheet(
                    viewModel = browserViewModel,
                    sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
                    onDismiss = { showShieldsSheet = false }
                )
            }

            if (showMediaSheet) {
                MediaDownloadBottomSheet(
                    viewModel = browserViewModel,
                    sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
                    onDismiss = { showMediaSheet = false },
                    onNavigateToTransfers = { navController.navigate("transfers") }
                )
            }

            if (showMenuSheet) {
                ChromiumMenuSheet(
                    sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
                    canGoBack = true,
                    canGoForward = false,
                    isBookmarked = uiState.isBookmarked,
                    isDesktopMode = isDesktopMode,
                    onDismiss = { showMenuSheet = false },
                    onBackClick = {},
                    onForwardClick = {},
                    onBookmarkClick = {
                        browserViewModel.toggleBookmark()
                    },
                    onDownloadClick = { navController.navigate("transfers") },
                    onInfoClick = {
                        showMenuSheet = false
                        showShieldsSheet = true
                    },
                    onNewTabClick = {
                        showMenuSheet = false
                        browserViewModel.createNewTab()
                    },
                    onNewIncognitoClick = {
                        showMenuSheet = false
                        browserViewModel.selectSpace("space-incognito")
                        browserViewModel.createNewTab()
                    },
                    onHistoryClick = {
                        showMenuSheet = false
                        navController.navigate("history_bookmarks/0")
                    },
                    onBookmarksListClick = {
                        showMenuSheet = false
                        navController.navigate("history_bookmarks/1")
                    },
                    onFindInPageClick = {
                        showMenuSheet = false
                        showFindInPage = true
                    },
                    onAiAssistantClick = {
                        showMenuSheet = false
                        showAiAssistantSheet = true
                    },
                    onReaderModeClick = {
                        showMenuSheet = false
                        navController.navigate("reader_mode")
                    },
                    onShieldsClick = {
                        showMenuSheet = false
                        showShieldsSheet = true
                    },
                    onSyncClick = {
                        showMenuSheet = false
                        showSyncSheet = true
                    },
                    onToggleDesktopMode = { isDesktopMode = !isDesktopMode },
                    onSettingsClick = {
                        showMenuSheet = false
                        navController.navigate("settings")
                    }
                )
            }

            if (showSyncSheet) {
                SyncBottomSheet(
                    viewModel = browserViewModel,
                    sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
                    onDismiss = { showSyncSheet = false },
                    onOpenRemoteTab = { url ->
                        browserViewModel.submitUrl(url) {}
                    }
                )
            }
        }

        composable("reader_mode") {
            ReaderModeScreen(
                viewModel = browserViewModel,
                onClose = { navController.popBackStack() }
            )
        }

        composable("tabs") {
            TabTrayScreen(
                viewModel = browserViewModel,
                onCloseTray = { navController.popBackStack() }
            )
        }

        composable(
            route = "history_bookmarks/{initialTab}",
            arguments = listOf(navArgument("initialTab") { type = NavType.IntType; defaultValue = 0 })
        ) { backStackEntry ->
            val initialTab = backStackEntry.arguments?.getInt("initialTab") ?: 0
            HistoryBookmarksScreen(
                viewModel = browserViewModel,
                initialTab = initialTab,
                onBack = { navController.popBackStack() },
                onNavigateToUrl = { url ->
                    browserViewModel.submitUrl(url) {}
                    navController.popBackStack()
                }
            )
        }

        composable("transfers") {
            TransfersScreen(
                viewModel = browserViewModel,
                onBack = { navController.popBackStack() }
            )
        }

        composable("settings") {
            SettingsScreen(
                viewModel = browserViewModel,
                onBack = { navController.popBackStack() },
                onOpenShields = { showShieldsSheet = true },
                onOpenSync = { showSyncSheet = true },
                onOpenBookmarks = { navController.navigate("history_bookmarks/1") }
            )
        }
    }
}
