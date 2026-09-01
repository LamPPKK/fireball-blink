package com.fireball.mini.ui.screens

import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInVertically
import androidx.compose.animation.slideOutVertically
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Download
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.key.Key
import androidx.compose.ui.input.key.isCtrlPressed
import androidx.compose.ui.input.key.isMetaPressed
import androidx.compose.ui.input.key.key
import androidx.compose.ui.input.key.onKeyEvent
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.fireball.mini.core.ai.ArticleExtractorHelper
import com.fireball.mini.core.engine.FireballWebChromeClient
import com.fireball.mini.core.engine.FireballWebView
import com.fireball.mini.core.engine.FireballWebViewClient
import com.fireball.mini.ui.components.ChromiumTopToolbar
import com.fireball.mini.ui.components.FindInPageBar
import com.fireball.mini.ui.components.TabletTabStrip
import com.fireball.mini.ui.theme.FireballBackground
import com.fireball.mini.ui.theme.FireballCardSurface
import com.fireball.mini.ui.theme.FireballElectricLime
import com.fireball.mini.ui.theme.FireballPrimaryText
import com.fireball.mini.ui.viewmodels.BrowserViewModel

@Composable
fun BrowserScreen(
    viewModel: BrowserViewModel,
    showFindInPage: Boolean,
    onCloseFindInPage: () -> Unit,
    onNavigateToTabs: () -> Unit,
    onOpenShields: () -> Unit,
    onOpenMediaDownloads: () -> Unit,
    onOpenAiAssistant: () -> Unit = {},
    onOpenSiteInfo: () -> Unit = {},
    onOpenQrShare: () -> Unit = {},
    onOpenMenu: () -> Unit,
    onToggleBookmark: () -> Unit,
    isDesktopMode: Boolean = false
) {
    val uiState by viewModel.uiState.collectAsState()
    val context = LocalContext.current
    var webViewInstance by remember { mutableStateOf<FireballWebView?>(null) }
    var showSearchOverlay by remember { mutableStateOf(false) }

    val configuration = LocalConfiguration.current
    val isTabletLayout = configuration.screenWidthDp >= 600

    // Find in Page state
    var findQuery by remember { mutableStateOf("") }
    var currentMatchIndex by remember { mutableIntStateOf(0) }
    var totalMatchesCount by remember { mutableIntStateOf(0) }

    Scaffold(
        topBar = {
            if (showFindInPage) {
                FindInPageBar(
                    query = findQuery,
                    currentIndex = currentMatchIndex,
                    totalMatches = totalMatchesCount,
                    onQueryChange = { query ->
                        findQuery = query
                        if (query.isEmpty()) {
                            webViewInstance?.clearMatches()
                            currentMatchIndex = 0
                            totalMatchesCount = 0
                        } else {
                            webViewInstance?.findAllAsync(query)
                        }
                    },
                    onFindPrevious = { webViewInstance?.findNext(false) },
                    onFindNext = { webViewInstance?.findNext(true) },
                    onClose = {
                        webViewInstance?.clearMatches()
                        findQuery = ""
                        currentMatchIndex = 0
                        totalMatchesCount = 0
                        onCloseFindInPage()
                    }
                )
            } else {
                Column {
                    // Desktop / Tablet Horizontal Tab Strip
                    if (isTabletLayout) {
                        TabletTabStrip(
                            tabs = uiState.spaceTabs,
                            activeTabId = uiState.activeTab?.id,
                            activeSpace = uiState.activeSpace,
                            isBurner = uiState.isBurnerMode,
                            engineType = uiState.engineType,
                            onToggleEngine = { viewModel.toggleBrowserEngine() },
                            onTabClick = { tabId -> viewModel.selectTab(tabId) },
                            onTabClose = { tabId -> viewModel.closeTab(tabId) },
                            onNewTabClick = { viewModel.createNewTab() },
                            onSpaceClick = onNavigateToTabs
                        )

                    }

                    // Unified Chromium Toolbar
                    ChromiumTopToolbar(
                        urlText = uiState.currentUrl,
                        isLoading = uiState.isLoading,
                        pageProgress = uiState.pageProgress,
                        isBurner = uiState.isBurnerMode,
                        tabCount = uiState.totalTabCount,
                        adsBlockedCount = uiState.adsBlockedThisSession,
                        isTabletLayout = isTabletLayout,
                        canGoBack = webViewInstance?.canGoBack() ?: false,
                        canGoForward = webViewInstance?.canGoForward() ?: false,
                        isBookmarked = uiState.isBookmarked,
                        onBackClick = { webViewInstance?.goBack() },
                        onForwardClick = { webViewInstance?.goForward() },
                        onHomeClick = {
                            viewModel.submitUrl("https://duckduckgo.com") { cleaned ->
                                webViewInstance?.loadUrl(cleaned)
                            }
                        },
                        onOmniboxClick = { showSearchOverlay = true },
                        onSwipeNextTab = { viewModel.selectNextTab() },
                        onSwipePrevTab = { viewModel.selectPrevTab() },
                        onReload = { webViewInstance?.reload() },
                        onAiClick = {
                            webViewInstance?.evaluateJavascript(ArticleExtractorHelper.extractionJs) { rawJson ->
                                if (!rawJson.isNullOrBlank() && rawJson != "null") {
                                    viewModel.onArticleExtracted(rawJson, uiState.currentUrl)
                                }
                            }
                            onOpenAiAssistant()
                        },
                        onShieldsClick = onOpenShields,
                        onSiteInfoClick = onOpenSiteInfo,
                        onTabsClick = {
                            val wv = webViewInstance
                            if (wv != null && wv.width > 0 && wv.height > 0) {
                                try {
                                    val bitmap = android.graphics.Bitmap.createBitmap(wv.width / 2, wv.height / 2, android.graphics.Bitmap.Config.ARGB_8888)
                                    val canvas = android.graphics.Canvas(bitmap)
                                    canvas.scale(0.5f, 0.5f)
                                    wv.draw(canvas)
                                    viewModel.saveTabSnapshot(uiState.activeTab?.id, bitmap, context)
                                } catch (_: Exception) {}
                            }
                            onNavigateToTabs()
                        },
                        onBookmarkClick = onToggleBookmark,
                        onMenuClick = onOpenMenu
                    )
                }
            }
        },
        modifier = Modifier.onKeyEvent { keyEvent ->
            val action = com.fireball.mini.core.engine.DesktopKeyShortcutHandler.handleKeyEvent(keyEvent.nativeKeyEvent)
            if (action != null) {
                when (action) {
                    com.fireball.mini.core.engine.BrowserShortcutAction.NEW_TAB -> viewModel.createNewTab()
                    com.fireball.mini.core.engine.BrowserShortcutAction.CLOSE_TAB -> uiState.activeTab?.id?.let { viewModel.closeTab(it) }
                    com.fireball.mini.core.engine.BrowserShortcutAction.REOPEN_CLOSED_TAB -> viewModel.createNewTab()
                    com.fireball.mini.core.engine.BrowserShortcutAction.NEXT_TAB -> viewModel.selectNextTab()
                    com.fireball.mini.core.engine.BrowserShortcutAction.PREV_TAB -> viewModel.selectPrevTab()
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_1 -> viewModel.selectTabByIndex(0)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_2 -> viewModel.selectTabByIndex(1)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_3 -> viewModel.selectTabByIndex(2)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_4 -> viewModel.selectTabByIndex(3)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_5 -> viewModel.selectTabByIndex(4)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_6 -> viewModel.selectTabByIndex(5)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_7 -> viewModel.selectTabByIndex(6)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_8 -> viewModel.selectTabByIndex(7)
                    com.fireball.mini.core.engine.BrowserShortcutAction.SELECT_TAB_9 -> viewModel.selectTabByIndex(8)
                    com.fireball.mini.core.engine.BrowserShortcutAction.RELOAD,
                    com.fireball.mini.core.engine.BrowserShortcutAction.FORCE_RELOAD -> webViewInstance?.reload()
                    com.fireball.mini.core.engine.BrowserShortcutAction.FOCUS_OMNIBOX -> showSearchOverlay = true
                    com.fireball.mini.core.engine.BrowserShortcutAction.BOOKMARK_PAGE -> onToggleBookmark()
                    com.fireball.mini.core.engine.BrowserShortcutAction.NAVIGATE_BACK -> if (webViewInstance?.canGoBack() == true) webViewInstance?.goBack()
                    com.fireball.mini.core.engine.BrowserShortcutAction.NAVIGATE_FORWARD -> if (webViewInstance?.canGoForward() == true) webViewInstance?.goForward()
                    com.fireball.mini.core.engine.BrowserShortcutAction.NEW_INCOGNITO -> viewModel.createSpace("Incognito", isBurner = true)
                    else -> {}
                }
                true
            } else if (keyEvent.key == androidx.compose.ui.input.key.Key.Escape) {
                if (showSearchOverlay) {
                    showSearchOverlay = false
                    true
                } else if (showFindInPage) {
                    onCloseFindInPage()
                    true
                } else {
                    false
                }
            } else {
                false
            }
        }
    ) { paddingValues ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(paddingValues)
                .background(FireballBackground)
        ) {
            // Browser Engine: Native WebView vs Fireball Beam Stream
            if (uiState.engineType == com.fireball.mini.core.models.BrowserEngineType.FIREBALL_BEAM_STREAM) {
                com.fireball.mini.ui.components.BeamStreamStage(
                    beamClient = viewModel.beamClient,
                    modifier = Modifier.fillMaxSize()
                )
            } else {
                // Main WebView with Space Session Isolation
                androidx.compose.runtime.key(uiState.activeTab?.spaceId, uiState.isBurnerMode) {
                    AndroidView(
                        modifier = Modifier.fillMaxSize(),
                        factory = { context ->
                            FireballWebView(
                                context = context,
                                spaceId = uiState.activeTab?.spaceId ?: "space-main",
                                profileId = uiState.activeTab?.profileId ?: "profile-main",
                                isOffTheRecord = uiState.isBurnerMode
                            ).apply {
                                layoutParams = FrameLayout.LayoutParams(
                                    ViewGroup.LayoutParams.MATCH_PARENT,
                                    ViewGroup.LayoutParams.MATCH_PARENT
                                )
                                setFindListener { activeMatchOrdinal, numberOfMatches, isDoneCounting ->
                                    currentMatchIndex = activeMatchOrdinal
                                    totalMatchesCount = numberOfMatches
                                }
                                webViewClient = FireballWebViewClient(
                                    tabId = uiState.activeTab?.id ?: "tab-default",
                                    profileId = uiState.activeTab?.profileId ?: "default-profile",
                                    onPageStartedCallback = { url, _ -> viewModel.onPageStarted(url) },
                                    onPageFinishedCallback = { url, title ->
                                        viewModel.onPageFinished(url, title)
                                        // Pre-extract article content in background for instant AI readiness
                                        evaluateJavascript(ArticleExtractorHelper.extractionJs) { rawJson ->
                                            if (!rawJson.isNullOrBlank() && rawJson != "null") {
                                                viewModel.onArticleExtracted(rawJson, url)
                                            }
                                        }
                                    },
                                    onAdBlockedCallback = { categoryCode -> viewModel.onAdBlocked(categoryCode) },
                                    onMediaDiscoveredCallback = { media -> viewModel.onMediaDiscovered(media) },
                                    isRedirectBlockingEnabled = { viewModel.isRedirectBlockingEnabled.value },
                                    onRedirectBlockedCallback = { _ -> viewModel.recordRedirectBlocked() }
                                )
                                webChromeClient = FireballWebChromeClient(
                                    onProgressChangedCallback = { progress -> viewModel.onProgressChanged(progress) },
                                    onTitleReceivedCallback = { title -> viewModel.onPageFinished(url ?: "", title) },
                                    onIconReceivedCallback = {},
                                    onCustomViewShowCallback = { _, _ -> },
                                    onCustomViewHideCallback = {},
                                    isPopupBlockingEnabled = { viewModel.isPopupBlockingEnabled.value },
                                    onPopupBlockedCallback = { viewModel.recordPopupBlocked() },
                                    onNewTabRequestedCallback = { newUrl -> viewModel.createNewTab(newUrl) }
                                )
                                setDesktopMode(isDesktopMode)
                                loadUrl(uiState.currentUrl)
                                webViewInstance = this
                            }
                        },
                        update = { webView ->
                            webViewInstance = webView
                            webView.setDesktopMode(isDesktopMode)
                            if (webView.url != uiState.currentUrl && !uiState.isLoading && uiState.currentUrl != "about:blank") {
                                webView.loadUrl(uiState.currentUrl)
                            }
                        }
                    )
                }
            }


            // Floating Media Sniffer SnackBar (if media discovered on page)
            AnimatedVisibility(
                visible = uiState.discoveredMediaCount > 0,
                enter = slideInVertically(initialOffsetY = { -it }),
                exit = slideOutVertically(targetOffsetY = { -it }),
                modifier = Modifier
                    .align(Alignment.TopCenter)
                    .padding(top = 10.dp)
            ) {
                Box(
                    modifier = Modifier
                        .clip(RoundedCornerShape(20.dp))
                        .background(FireballCardSurface)
                        .border(1.dp, FireballElectricLime, RoundedCornerShape(20.dp))
                        .clickable { onOpenMediaDownloads() }
                        .padding(horizontal = 14.dp, vertical = 8.dp)
                ) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(
                            imageVector = Icons.Default.Download,
                            contentDescription = "Media Sniffer",
                            tint = FireballElectricLime,
                            modifier = Modifier.size(16.dp)
                        )
                        Spacer(modifier = Modifier.width(6.dp))
                        Text(
                            text = "${uiState.discoveredMediaCount} Media Stream(s) Ready to Download",
                            style = MaterialTheme.typography.bodyMedium,
                            color = FireballPrimaryText,
                            fontSize = 12.sp
                        )
                    }
                }
            }

            // Full-Screen Search Overlay
            AnimatedVisibility(
                visible = showSearchOverlay,
                enter = fadeIn(),
                exit = fadeOut()
            ) {
                SearchOverlay(
                    initialText = uiState.currentUrl,
                    isBurner = uiState.isBurnerMode,
                    onClose = { showSearchOverlay = false },
                    onSubmit = { query ->
                        showSearchOverlay = false
                        viewModel.submitUrl(query) { cleaned ->
                            webViewInstance?.loadUrl(cleaned)
                        }
                    }
                )
            }
        }
    }

    DisposableEffect(Unit) {
        onDispose {
            webViewInstance?.cleanDestroy()
        }
    }
}
