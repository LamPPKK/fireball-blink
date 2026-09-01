import Foundation

public struct SearchEngine: Identifiable, Codable, Sendable, Equatable {
    public let id: String
    public let name: String
    public let searchUrlTemplate: String
    public let bangShortcut: String
    public let iconName: String

    public init(
        id: String,
        name: String,
        searchUrlTemplate: String,
        bangShortcut: String,
        iconName: String
    ) {
        self.id = id
        self.name = name
        self.searchUrlTemplate = searchUrlTemplate
        self.bangShortcut = bangShortcut
        self.iconName = iconName
    }

    public func buildSearchUrl(query: String) -> URL {
        let encodedQuery = query.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) ?? query
        let urlString = searchUrlTemplate.replacingOccurrences(of: "%s", with: encodedQuery)
        return URL(string: urlString) ?? URL(string: "https://duckduckgo.com/?q=\(encodedQuery)")!
    }
}

public enum SearchEngineDefaults {
    public static let duckduckgo = SearchEngine(
        id: "duckduckgo",
        name: "DuckDuckGo",
        searchUrlTemplate: "https://duckduckgo.com/?q=%s",
        bangShortcut: "!d",
        iconName: "magnifyingglass"
    )
    public static let google = SearchEngine(
        id: "google",
        name: "Google",
        searchUrlTemplate: "https://www.google.com/search?q=%s",
        bangShortcut: "!g",
        iconName: "magnifyingglass"
    )
    public static let brave = SearchEngine(
        id: "brave",
        name: "Brave Search",
        searchUrlTemplate: "https://search.brave.com/search?q=%s",
        bangShortcut: "!b",
        iconName: "shield"
    )
    public static let bing = SearchEngine(
        id: "bing",
        name: "Bing",
        searchUrlTemplate: "https://www.bing.com/search?q=%s",
        bangShortcut: "!bi",
        iconName: "magnifyingglass"
    )
    public static let ecosia = SearchEngine(
        id: "ecosia",
        name: "Ecosia",
        searchUrlTemplate: "https://www.ecosia.org/search?q=%s",
        bangShortcut: "!e",
        iconName: "leaf"
    )
    public static let startpage = SearchEngine(
        id: "startpage",
        name: "Startpage",
        searchUrlTemplate: "https://www.startpage.com/sp/search?query=%s",
        bangShortcut: "!sp",
        iconName: "lock.shield"
    )
    public static let kagi = SearchEngine(
        id: "kagi",
        name: "Kagi",
        searchUrlTemplate: "https://kagi.com/search?q=%s",
        bangShortcut: "!k",
        iconName: "sparkles"
    )
    public static let wikipedia = SearchEngine(
        id: "wikipedia",
        name: "Wikipedia",
        searchUrlTemplate: "https://en.wikipedia.org/wiki/Special:Search?search=%s",
        bangShortcut: "!w",
        iconName: "book"
    )
    public static let youtube = SearchEngine(
        id: "youtube",
        name: "YouTube",
        searchUrlTemplate: "https://www.youtube.com/results?search_query=%s",
        bangShortcut: "!yt",
        iconName: "play.rectangle"
    )
    public static let github = SearchEngine(
        id: "github",
        name: "GitHub",
        searchUrlTemplate: "https://github.com/search?q=%s",
        bangShortcut: "!gh",
        iconName: "chevron.left.forwardslash.chevron.right"
    )
    public static let reddit = SearchEngine(
        id: "reddit",
        name: "Reddit",
        searchUrlTemplate: "https://www.reddit.com/search/?q=%s",
        bangShortcut: "!r",
        iconName: "bubble.left.and.bubble.right"
    )

    public static let builtInEngines: [SearchEngine] = [
        duckduckgo, google, brave, bing, ecosia, startpage, kagi,
        wikipedia, youtube, github, reddit
    ]
}

public enum BangParser {
    public static func resolveQuery(
        input: String,
        defaultEngine: SearchEngine = SearchEngineDefaults.duckduckgo
    ) -> URL {
        let trimmed = input.trimmingCharacters(in: .whitespacesAndNewlines)
        if trimmed.isEmpty {
            return defaultEngine.buildSearchUrl(query: "")
        }

        // Direct URL check
        if trimmed.hasPrefix("http://") || trimmed.hasPrefix("https://") {
            if let directUrl = URL(string: trimmed) {
                return directUrl
            }
        }

        if trimmed.contains(".") && !trimmed.contains(" ") {
            if let guessedUrl = URL(string: "https://\(trimmed)") {
                return guessedUrl
            }
        }

        // Bang shortcut parsing: e.g. "!gh fireball"
        if trimmed.hasPrefix("!") {
            let components = trimmed.split(separator: " ", maxSplits: 1).map(String.init)
            let bang = components[0]
            let query = components.count > 1 ? components[1] : ""

            if let matchedEngine = SearchEngineDefaults.builtInEngines.first(where: { $0.bangShortcut.lowercased() == bang.lowercased() }) {
                return matchedEngine.buildSearchUrl(query: query)
            }
        }

        return defaultEngine.buildSearchUrl(query: trimmed)
    }
}
