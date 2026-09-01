import SwiftUI
import FireballCore

public struct TopOmniboxBar: View {
    public let urlString: String
    public let isBurner: Bool
    public let isSecure: Bool
    public let openTabsCount: Int
    public let onOmniboxTap: () -> Void
    public let onSiteInfoTap: () -> Void
    public let onShieldsTap: () -> Void
    public let onTabsTap: () -> Void
    public let onMenuTap: () -> Void

    public init(
        urlString: String,
        isBurner: Bool = false,
        isSecure: Bool = true,
        openTabsCount: Int = 1,
        onOmniboxTap: @escaping () -> Void,
        onSiteInfoTap: @escaping () -> Void,
        onShieldsTap: @escaping () -> Void,
        onTabsTap: @escaping () -> Void,
        onMenuTap: @escaping () -> Void
    ) {
        self.urlString = urlString
        self.isBurner = isBurner
        self.isSecure = isSecure
        self.openTabsCount = openTabsCount
        self.onOmniboxTap = onOmniboxTap
        self.onSiteInfoTap = onSiteInfoTap
        self.onShieldsTap = onShieldsTap
        self.onTabsTap = onTabsTap
        self.onMenuTap = onMenuTap
    }

    public var displayHost: String {
        guard let url = URL(string: urlString), let host = url.host, !host.isEmpty else {
            return urlString.isEmpty ? "Search or type URL" : urlString
        }
        return host
    }

    public var body: some View {
        HStack(spacing: 8) {
            // Omnibox Capsule
            HStack(spacing: 8) {
                // Lock Icon
                Button(action: onSiteInfoTap) {
                    Image(systemName: isSecure ? "lock.fill" : "lock.open.fill")
                        .font(.system(size: 13, weight: .bold))
                        .foregroundColor(isSecure ? FireballTheme.electricLime : FireballTheme.meteorOrange)
                }
                .buttonStyle(.plain)

                // Host Text
                Text(displayHost)
                    .font(.system(size: 14, weight: .medium))
                    .foregroundColor(FireballTheme.primaryText)
                    .lineLimit(1)
                    .truncationMode(.tail)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .contentShape(Rectangle())
                    .onTapGesture {
                        onOmniboxTap()
                    }

                // Shields Trigger
                Button(action: onShieldsTap) {
                    Image(systemName: "shield.fill")
                        .font(.system(size: 14, weight: .semibold))
                        .foregroundColor(isBurner ? FireballTheme.meteorOrange : FireballTheme.electricLime)
                }
                .buttonStyle(.plain)
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)
            .background(FireballTheme.raisedSurface)
            .cornerRadius(20)
            .overlay(
                RoundedRectangle(cornerRadius: 20)
                    .stroke(isBurner ? FireballTheme.meteorOrange.opacity(0.5) : FireballTheme.border, lineWidth: 1)
            )

            // Tab Counter Button
            Button(action: onTabsTap) {
                ZStack {
                    RoundedRectangle(cornerRadius: 8)
                        .stroke(FireballTheme.primaryText, lineWidth: 1.5)
                        .frame(width: 26, height: 26)
                    Text("\(openTabsCount)")
                        .font(.system(size: 12, weight: .bold))
                        .foregroundColor(FireballTheme.primaryText)
                }
            }
            .buttonStyle(.plain)

            // 3-dot Menu Button
            Button(action: onMenuTap) {
                Image(systemName: "ellipsis")
                    .font(.system(size: 16, weight: .bold))
                    .foregroundColor(FireballTheme.primaryText)
                    .frame(width: 28, height: 28)
            }
            .buttonStyle(.plain)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(FireballTheme.deepSurface)
    }
}
