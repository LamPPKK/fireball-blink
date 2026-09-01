import Foundation

public enum TabTier: String, Codable, Sendable, CaseIterable {
    case favorite = "FAVORITE"
    case pinned = "PINNED"
    case today = "TODAY"
    case burner = "BURNER"
}

public enum TabPresentation: String, Codable, Sendable, CaseIterable {
    case classic = "CLASSIC"
    case floating = "FLOATING"
    case sidebar = "SIDEBAR"
    case grid = "GRID"
}

public struct FireballTab: Identifiable, Codable, Sendable, Equatable {
    public let id: UUID
    public var url: URL
    public var title: String
    public var isBurner: Bool
    public var isPinned: Bool
    public var isFavorite: Bool
    public var tier: TabTier
    public var isSuspended: Bool
    public var createdAt: Date
    public var lastActiveAt: Date

    public init(
        id: UUID = UUID(),
        url: URL = URL(string: "https://duckduckgo.com")!,
        title: String = "DuckDuckGo",
        isBurner: Bool = false,
        isPinned: Bool = false,
        isFavorite: Bool = false,
        tier: TabTier = .today,
        isSuspended: Bool = false,
        createdAt: Date = Date(),
        lastActiveAt: Date = Date()
    ) {
        self.id = id
        self.url = url
        self.title = title
        self.isBurner = isBurner
        self.isPinned = isPinned
        self.isFavorite = isFavorite
        self.tier = isBurner ? .burner : (isFavorite ? .favorite : (isPinned ? .pinned : tier))
        self.isSuspended = isSuspended
        self.createdAt = createdAt
        self.lastActiveAt = lastActiveAt
    }
}

public struct Space: Identifiable, Codable, Sendable, Equatable {
    public let id: UUID
    public var name: String
    public var isBurner: Bool
    public var iconName: String
    public var tabs: [FireballTab]
    public var activeTabId: UUID?

    public init(
        id: UUID = UUID(),
        name: String,
        isBurner: Bool = false,
        iconName: String = "globe",
        tabs: [FireballTab] = [],
        activeTabId: UUID? = nil
    ) {
        self.id = id
        self.name = name
        self.isBurner = isBurner
        self.iconName = iconName
        self.tabs = tabs
        self.activeTabId = activeTabId ?? tabs.first?.id
    }

    public var activeTab: FireballTab? {
        tabs.first(where: { $0.id == activeTabId }) ?? tabs.first
    }
}

public struct Profile: Identifiable, Codable, Sendable, Equatable {
    public let id: UUID
    public var name: String
    public var isOffTheRecord: Bool
    public var spaces: [Space]
    public var activeSpaceId: UUID?

    public init(
        id: UUID = UUID(),
        name: String,
        isOffTheRecord: Bool = false,
        spaces: [Space] = [],
        activeSpaceId: UUID? = nil
    ) {
        self.id = id
        self.name = name
        self.isOffTheRecord = isOffTheRecord
        self.spaces = spaces
        self.activeSpaceId = activeSpaceId ?? spaces.first?.id
    }

    public var activeSpace: Space? {
        spaces.first(where: { $0.id == activeSpaceId }) ?? spaces.first
    }
}
