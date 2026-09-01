import SwiftUI
import FireballCore

#if canImport(WebKit)
import WebKit
#endif

public struct FireballBrowserView: View {
    @State private var profile: Profile
    @State private var shieldsConfig = ShieldsConfig()
    @State private var vault = PasswordVault()
    
    @State private var showSearchOverlay = false
    @State private var showTabsOverlay = false
    @State private var showShieldsSheet = false
    @State private var showSiteInfoSheet = false
    @State private var showPasswordsSheet = false
    @State private var showBeamPairingSheet = false
    @State private var showMenuSheet = false

    public init() {
        let initialTab = FireballTab(
            url: URL(string: "https://duckduckgo.com")!,
            title: "DuckDuckGo"
        )
        let mainSpace = Space(name: "Main", iconName: "globe", tabs: [initialTab])
        let burnerSpace = Space(name: "Incognito", isBurner: true, iconName: "flame.fill", tabs: [])
        let initialProfile = Profile(name: "Default Profile", spaces: [mainSpace, burnerSpace])
        
        self._profile = State(initialValue: initialProfile)
    }

    public var currentSpace: Space? {
        profile.activeSpace
    }

    public var currentTab: FireballTab? {
        profile.activeSpace?.activeTab
    }

    public var body: some View {
        ZStack {
            FireballTheme.background.ignoresSafeArea()

            VStack(spacing: 0) {
                // Top Omnibox Bar
                TopOmniboxBar(
                    urlString: currentTab?.url.absoluteString ?? "https://duckduckgo.com",
                    isBurner: currentSpace?.isBurner ?? false,
                    isSecure: true,
                    openTabsCount: currentSpace?.tabs.count ?? 1,
                    onOmniboxTap: { showSearchOverlay = true },
                    onSiteInfoTap: { showSiteInfoSheet = true },
                    onShieldsTap: { showShieldsSheet = true },
                    onTabsTap: { showTabsOverlay = true },
                    onMenuTap: { showMenuSheet = true }
                )

                // iPad Horizontal Tab Strip
                if let space = currentSpace, space.tabs.count > 1 {
                    iPadTabStripView(
                        tabs: space.tabs,
                        activeTabId: space.activeTabId,
                        onSelectTab: { tabId in
                            selectTab(tabId)
                        },
                        onCloseTab: { tabId in
                            closeTab(tabId)
                        },
                        onNewTab: {
                            addNewTab()
                        }
                    )
                }

                // Web View Container Placeholder
                ZStack {
                    FireballTheme.cardSurface
                    VStack(spacing: 12) {
                        Image(systemName: "globe")
                            .font(.system(size: 44))
                            .foregroundColor(FireballTheme.electricLime)
                        Text(currentTab?.title ?? "DuckDuckGo")
                            .font(.system(size: 16, weight: .bold))
                            .foregroundColor(FireballTheme.primaryText)
                        Text(currentTab?.url.absoluteString ?? "https://duckduckgo.com")
                            .font(.system(size: 12))
                            .foregroundColor(FireballTheme.secondaryText)
                    }
                }
                .frame(maxWidth: .infinity, maxHeight: .infinity)
            }

            // Search Overlay
            if showSearchOverlay {
                SearchBangsOverlay(
                    initialText: currentTab?.url.absoluteString ?? "",
                    isBurner: currentSpace?.isBurner ?? false,
                    onSubmit: { newUrl in
                        showSearchOverlay = false
                        navigateCurrentTab(to: newUrl)
                    },
                    onClose: { showSearchOverlay = false }
                )
                .transition(.opacity)
                .zIndex(10)
            }
        }
        .sheet(isPresented: $showTabsOverlay) {
            TabGridOverlay(
                spaces: profile.spaces,
                activeSpaceId: profile.activeSpaceId,
                onSelectSpace: { spaceId in
                    profile.activeSpaceId = spaceId
                },
                onSelectTab: { tabId in
                    selectTab(tabId)
                },
                onCloseTab: { tabId in
                    closeTab(tabId)
                },
                onNewTab: {
                    addNewTab()
                },
                onDone: { showTabsOverlay = false }
            )
        }
        .sheet(isPresented: $showShieldsSheet) {
            ShieldsSheet(
                config: $shieldsConfig,
                metrics: ShieldsSiteMetrics(
                    domain: currentTab?.url.host ?? "duckduckgo.com",
                    blockedTrackersCount: 7,
                    isSecureHttps: true
                ),
                onDismiss: { showShieldsSheet = false }
            )
        }
        .sheet(isPresented: $showSiteInfoSheet) {
            SiteInfoSheet(
                metrics: ShieldsSiteMetrics(
                    domain: currentTab?.url.host ?? "duckduckgo.com",
                    blockedTrackersCount: 7,
                    isSecureHttps: true
                ),
                onClearSiteData: {
                    showSiteInfoSheet = false
                },
                onDismiss: { showSiteInfoSheet = false }
            )
        }
        .sheet(isPresented: $showPasswordsSheet) {
            PasswordsView(vault: vault, onDismiss: { showPasswordsSheet = false })
        }
        .sheet(isPresented: $showBeamPairingSheet) {
            BeamPairingView(onDismiss: { showBeamPairingSheet = false })
        }
        .confirmationDialog("Fireball Menu", isPresented: $showMenuSheet, titleVisibility: .visible) {
            Button("Password Vault") { showPasswordsSheet = true }
            Button("Fireball Beam (Remote Streaming)") { showBeamPairingSheet = true }
            Button("Fireball Shields") { showShieldsSheet = true }
            Button("Site Permissions & Storage") { showSiteInfoSheet = true }
            Button("New Tab") { addNewTab() }
            Button("Cancel", role: .cancel) {}
        }
    }

    private func selectTab(_ id: UUID) {
        guard let spaceIndex = profile.spaces.firstIndex(where: { $0.id == profile.activeSpaceId }) else { return }
        profile.spaces[spaceIndex].activeTabId = id
    }

    private func closeTab(_ id: UUID) {
        guard let spaceIndex = profile.spaces.firstIndex(where: { $0.id == profile.activeSpaceId }) else { return }
        profile.spaces[spaceIndex].tabs.removeAll { $0.id == id }
        if profile.spaces[spaceIndex].activeTabId == id {
            profile.spaces[spaceIndex].activeTabId = profile.spaces[spaceIndex].tabs.first?.id
        }
    }

    private func addNewTab() {
        guard let spaceIndex = profile.spaces.firstIndex(where: { $0.id == profile.activeSpaceId }) else { return }
        let newTab = FireballTab(
            url: URL(string: "https://duckduckgo.com")!,
            title: "DuckDuckGo",
            isBurner: profile.spaces[spaceIndex].isBurner
        )
        profile.spaces[spaceIndex].tabs.append(newTab)
        profile.spaces[spaceIndex].activeTabId = newTab.id
    }

    private func navigateCurrentTab(to url: URL) {
        guard let spaceIndex = profile.spaces.firstIndex(where: { $0.id == profile.activeSpaceId }),
              let tabIndex = profile.spaces[spaceIndex].tabs.firstIndex(where: { $0.id == profile.spaces[spaceIndex].activeTabId }) else {
            return
        }
        let cleaned = ShieldsEngine.cleanUrl(url)
        profile.spaces[spaceIndex].tabs[tabIndex].url = cleaned
        profile.spaces[spaceIndex].tabs[tabIndex].title = cleaned.host ?? cleaned.absoluteString
    }
}
