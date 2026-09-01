import SwiftUI
import FireballCore

public struct SearchBangsOverlay: View {
    public let initialText: String
    public let isBurner: Bool
    public let onSubmit: (URL) -> Void
    public let onClose: () -> Void

    @State private var queryText: String = ""

    public let bangChips: [(String, String)] = [
        ("!g", "Google"),
        ("!b", "Brave"),
        ("!yt", "YouTube"),
        ("!gh", "GitHub"),
        ("!w", "Wiki"),
        ("!k", "Kagi"),
        ("!sp", "Startpage"),
        ("!e", "Ecosia"),
        ("!r", "Reddit")
    ]

    public let topSites: [(String, String, String)] = [
        ("DuckDuckGo", "https://duckduckgo.com", "magnifyingglass"),
        ("Google", "https://www.google.com", "magnifyingglass"),
        ("GitHub", "https://github.com", "chevron.left.forwardslash.chevron.right"),
        ("YouTube", "https://www.youtube.com", "play.rectangle"),
        ("Reddit", "https://www.reddit.com", "bubble.left.and.bubble.right"),
        ("Wikipedia", "https://www.wikipedia.org", "book")
    ]

    public init(
        initialText: String,
        isBurner: Bool = false,
        onSubmit: @escaping (URL) -> Void,
        onClose: @escaping () -> Void
    ) {
        self.initialText = initialText
        self.isBurner = isBurner
        self.onSubmit = onSubmit
        self.onClose = onClose
    }

    public var body: some View {
        VStack(spacing: 16) {
            // Header Search Input
            HStack(spacing: 8) {
                Button(action: onClose) {
                    Image(systemName: "chevron.left")
                        .font(.system(size: 16, weight: .bold))
                        .foregroundColor(FireballTheme.primaryText)
                }

                HStack {
                    Image(systemName: "magnifyingglass")
                        .foregroundColor(isBurner ? FireballTheme.meteorOrange : FireballTheme.electricLime)

                    TextField("Search or type URL...", text: $queryText)
                        .foregroundColor(FireballTheme.primaryText)
                        .onSubmit {
                            submitQuery()
                        }

                    if !queryText.isEmpty {
                        Button(action: { queryText = "" }) {
                            Image(systemName: "xmark.circle.fill")
                                .foregroundColor(FireballTheme.mutedText)
                        }
                    }
                }
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(FireballTheme.raisedSurface)
                .cornerRadius(12)
                .overlay(
                    RoundedRectangle(cornerRadius: 12)
                        .stroke(isBurner ? FireballTheme.meteorOrange : FireballTheme.electricLime, lineWidth: 1)
                )

                Button(action: submitQuery) {
                    Text("Go")
                        .font(.system(size: 14, weight: .bold))
                        .foregroundColor(FireballTheme.background)
                        .padding(.horizontal, 14)
                        .padding(.vertical, 8)
                        .background(queryText.isEmpty ? FireballTheme.mutedText : (isBurner ? FireballTheme.meteorOrange : FireballTheme.electricLime))
                        .cornerRadius(10)
                }
                .disabled(queryText.isEmpty)
            }
            .padding(.horizontal, 16)
            .padding(.top, 12)

            // Bang Shortcuts Section
            VStack(alignment: .leading, spacing: 8) {
                Text("BANG SHORTCUTS (!BANG QUERY)")
                    .font(.system(size: 11, weight: .bold))
                    .foregroundColor(FireballTheme.secondaryText)
                    .padding(.horizontal, 16)

                ScrollView(.horizontal, showsIndicators: false) {
                    HStack(spacing: 8) {
                        ForEach(bangChips, id: \.0) { bang, name in
                            HStack(spacing: 4) {
                                Text(bang)
                                    .font(.system(size: 12, weight: .bold))
                                    .foregroundColor(FireballTheme.electricLime)
                                Text(name)
                                    .font(.system(size: 12))
                                    .foregroundColor(FireballTheme.primaryText)
                            }
                            .padding(.horizontal, 10)
                            .padding(.vertical, 6)
                            .background(FireballTheme.cardSurface)
                            .cornerRadius(14)
                            .overlay(
                                RoundedRectangle(cornerRadius: 14)
                                    .stroke(FireballTheme.border, lineWidth: 1)
                            )
                            .onTapGesture {
                                queryText = "\(bang) "
                            }
                        }
                    }
                    .padding(.horizontal, 16)
                }
            }

            // Top Sites Grid
            VStack(alignment: .leading, spacing: 10) {
                Text("TOP SITES")
                    .font(.system(size: 11, weight: .bold))
                    .foregroundColor(FireballTheme.secondaryText)
                    .padding(.horizontal, 16)

                LazyVGrid(columns: [GridItem(.adaptive(minimum: 100), spacing: 12)], spacing: 12) {
                    ForEach(topSites, id: \.0) { name, urlStr, icon in
                        VStack(spacing: 8) {
                            Image(systemName: icon)
                                .font(.system(size: 20))
                                .foregroundColor(FireballTheme.electricLime)
                                .frame(width: 44, height: 44)
                                .background(FireballTheme.raisedSurface)
                                .clipShape(Circle())

                            Text(name)
                                .font(.system(size: 12, weight: .medium))
                                .foregroundColor(FireballTheme.primaryText)
                        }
                        .padding(.vertical, 10)
                        .frame(maxWidth: .infinity)
                        .background(FireballTheme.cardSurface)
                        .cornerRadius(12)
                        .overlay(
                            RoundedRectangle(cornerRadius: 12)
                                .stroke(FireballTheme.border, lineWidth: 1)
                        )
                        .onTapGesture {
                            if let targetUrl = URL(string: urlStr) {
                                onSubmit(targetUrl)
                            }
                        }
                    }
                }
                .padding(.horizontal, 16)
            }

            Spacer()
        }
        .background(FireballTheme.background)
        .onAppear {
            if initialText != "https://duckduckgo.com" && !initialText.isEmpty {
                queryText = initialText
            }
        }
    }

    private func submitQuery() {
        guard !queryText.isEmpty else { return }
        let resolved = BangParser.resolveQuery(input: queryText)
        onSubmit(resolved)
    }
}
