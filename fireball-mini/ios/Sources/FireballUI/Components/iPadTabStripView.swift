import SwiftUI
import FireballCore

public struct iPadTabStripView: View {
    public let tabs: [FireballTab]
    public let activeTabId: UUID?
    public let onSelectTab: (UUID) -> Void
    public let onCloseTab: (UUID) -> Void
    public let onNewTab: () -> Void

    public init(
        tabs: [FireballTab],
        activeTabId: UUID?,
        onSelectTab: @escaping (UUID) -> Void,
        onCloseTab: @escaping (UUID) -> Void,
        onNewTab: @escaping () -> Void
    ) {
        self.tabs = tabs
        self.activeTabId = activeTabId
        self.onSelectTab = onSelectTab
        self.onCloseTab = onCloseTab
        self.onNewTab = onNewTab
    }

    public var body: some View {
        ScrollView(.horizontal, showsIndicators: false) {
            HStack(spacing: 6) {
                ForEach(tabs) { tab in
                    let isActive = tab.id == activeTabId
                    HStack(spacing: 6) {
                        Image(systemName: tab.isBurner ? "flame.fill" : "globe")
                            .font(.system(size: 11))
                            .foregroundColor(isActive ? FireballTheme.electricLime : FireballTheme.mutedText)

                        Text(tab.title.isEmpty ? "New Tab" : tab.title)
                            .font(.system(size: 12, weight: isActive ? .semibold : .regular))
                            .foregroundColor(isActive ? FireballTheme.primaryText : FireballTheme.secondaryText)
                            .lineLimit(1)
                            .frame(maxWidth: 160, alignment: .leading)

                        Button(action: { onCloseTab(tab.id) }) {
                            Image(systemName: "xmark")
                                .font(.system(size: 10, weight: .bold))
                                .foregroundColor(FireballTheme.mutedText)
                        }
                        .buttonStyle(.plain)
                    }
                    .padding(.horizontal, 10)
                    .padding(.vertical, 6)
                    .background(isActive ? FireballTheme.raisedSurface : FireballTheme.cardSurface)
                    .cornerRadius(8)
                    .overlay(
                        RoundedRectangle(cornerRadius: 8)
                            .stroke(isActive ? FireballTheme.electricLime.opacity(0.6) : FireballTheme.border, lineWidth: 1)
                    )
                    .onTapGesture {
                        onSelectTab(tab.id)
                    }
                }

                // Plus Button
                Button(action: onNewTab) {
                    Image(systemName: "plus")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundColor(FireballTheme.electricLime)
                        .padding(6)
                        .background(FireballTheme.cardSurface)
                        .clipShape(Circle())
                }
                .buttonStyle(.plain)
            }
            .padding(.horizontal, 8)
            .padding(.vertical, 4)
        }
        .background(FireballTheme.deepSurface)
    }
}
