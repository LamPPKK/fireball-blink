import SwiftUI
import FireballCore

public struct TabGridOverlay: View {
    public let spaces: [Space]
    public let activeSpaceId: UUID?
    public let onSelectSpace: (UUID) -> Void
    public let onSelectTab: (UUID) -> Void
    public let onCloseTab: (UUID) -> Void
    public let onNewTab: () -> Void
    public let onDone: () -> Void

    public init(
        spaces: [Space],
        activeSpaceId: UUID?,
        onSelectSpace: @escaping (UUID) -> Void,
        onSelectTab: @escaping (UUID) -> Void,
        onCloseTab: @escaping (UUID) -> Void,
        onNewTab: @escaping () -> Void,
        onDone: @escaping () -> Void
    ) {
        self.spaces = spaces
        self.activeSpaceId = activeSpaceId
        self.onSelectSpace = onSelectSpace
        self.onSelectTab = onSelectTab
        self.onCloseTab = onCloseTab
        self.onNewTab = onNewTab
        self.onDone = onDone
    }

    public var currentSpace: Space? {
        spaces.first(where: { $0.id == activeSpaceId }) ?? spaces.first
    }

    public var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                // Spaces Segment Bar
                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(spaces) { space in
                            let isSpaceActive = space.id == activeSpaceId
                            HStack(spacing: 6) {
                                Image(systemName: space.isBurner ? "flame.fill" : space.iconName)
                                    .font(.system(size: 12))
                                    .foregroundColor(space.isBurner ? FireballTheme.meteorOrange : (isSpaceActive ? FireballTheme.electricLime : FireballTheme.mutedText))

                                Text(space.name)
                                    .font(.system(size: 13, weight: isSpaceActive ? .bold : .medium))
                                    .foregroundColor(isSpaceActive ? FireballTheme.primaryText : FireballTheme.secondaryText)
                            }
                            .padding(.horizontal, 12)
                            .padding(.vertical, 8)
                            .background(isSpaceActive ? FireballTheme.cardSurface : FireballTheme.deepSurface)
                            .cornerRadius(16)
                            .overlay(
                                RoundedRectangle(cornerRadius: 16)
                                    .stroke(isSpaceActive ? (space.isBurner ? FireballTheme.meteorOrange : FireballTheme.electricLime) : FireballTheme.border, lineWidth: 1)
                            )
                            .onTapGesture {
                                onSelectSpace(space.id)
                            }
                        }
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 10)
                }
                .background(FireballTheme.deepSurface)

                // Tab Grid
                if let space = currentSpace {
                    ScrollView {
                        LazyVGrid(columns: [GridItem(.adaptive(minimum: 150), spacing: 14)], spacing: 14) {
                            ForEach(space.tabs) { tab in
                                let isActiveTab = tab.id == space.activeTabId
                                VStack(alignment: .leading, spacing: 6) {
                                    HStack {
                                        Image(systemName: tab.isBurner ? "flame.fill" : "globe")
                                            .font(.system(size: 11))
                                            .foregroundColor(isActiveTab ? FireballTheme.electricLime : FireballTheme.mutedText)
                                        Text(tab.title)
                                            .font(.system(size: 12, weight: .semibold))
                                            .foregroundColor(FireballTheme.primaryText)
                                            .lineLimit(1)
                                        Spacer()
                                        Button(action: { onCloseTab(tab.id) }) {
                                            Image(systemName: "xmark")
                                                .font(.system(size: 10, weight: .bold))
                                                .foregroundColor(FireballTheme.mutedText)
                                        }
                                    }

                                    Spacer()

                                    Text(tab.url.host ?? tab.url.absoluteString)
                                        .font(.system(size: 10))
                                        .foregroundColor(FireballTheme.mutedText)
                                        .lineLimit(1)
                                }
                                .padding(12)
                                .frame(height: 140)
                                .background(FireballTheme.cardSurface)
                                .cornerRadius(12)
                                .overlay(
                                    RoundedRectangle(cornerRadius: 12)
                                        .stroke(isActiveTab ? (space.isBurner ? FireballTheme.meteorOrange : FireballTheme.electricLime) : FireballTheme.border, lineWidth: isActiveTab ? 2 : 1)
                                )
                                .onTapGesture {
                                    onSelectTab(tab.id)
                                    onDone()
                                }
                            }
                        }
                        .padding(16)
                    }
                }

                // Bottom Action Bar
                HStack {
                    Button(action: onDone) {
                        Text("Done")
                            .font(.system(size: 15, weight: .semibold))
                            .foregroundColor(FireballTheme.primaryText)
                            .padding(.horizontal, 16)
                            .padding(.vertical, 8)
                            .background(FireballTheme.raisedSurface)
                            .cornerRadius(10)
                    }

                    Spacer()

                    Button(action: onNewTab) {
                        HStack(spacing: 6) {
                            Image(systemName: "plus")
                            Text("New Tab")
                        }
                        .font(.system(size: 14, weight: .bold))
                        .foregroundColor(FireballTheme.background)
                        .padding(.horizontal, 18)
                        .padding(.vertical, 10)
                        .background(FireballTheme.electricLime)
                        .cornerRadius(20)
                    }
                }
                .padding(.horizontal, 16)
                .padding(.vertical, 12)
                .background(FireballTheme.deepSurface)
            }
            .background(FireballTheme.background)
            .navigationTitle("Tabs & Spaces")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
        }
    }
}
