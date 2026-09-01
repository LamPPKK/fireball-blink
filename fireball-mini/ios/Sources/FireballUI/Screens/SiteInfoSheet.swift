import SwiftUI
import FireballCore

public struct SiteInfoSheet: View {
    public let metrics: ShieldsSiteMetrics
    public let onClearSiteData: () -> Void
    public let onDismiss: () -> Void

    @State private var locationAllowed = false
    @State private var cameraAllowed = false
    @State private var micAllowed = false
    @State private var notificationsAllowed = false

    public init(
        metrics: ShieldsSiteMetrics,
        onClearSiteData: @escaping () -> Void,
        onDismiss: @escaping () -> Void
    ) {
        self.metrics = metrics
        self.onClearSiteData = onClearSiteData
        self.onDismiss = onDismiss
    }

    public var body: some View {
        NavigationStack {
            List {
                // Connection Security
                Section(header: Text("CONNECTION SECURITY").foregroundColor(FireballTheme.secondaryText)) {
                    HStack(spacing: 12) {
                        Image(systemName: metrics.isSecureHttps ? "lock.fill" : "lock.open.fill")
                            .font(.system(size: 20))
                            .foregroundColor(metrics.isSecureHttps ? FireballTheme.electricLime : FireballTheme.meteorOrange)

                        VStack(alignment: .leading, spacing: 2) {
                            Text(metrics.isSecureHttps ? "Connection is Secure" : "Connection is Not Secure")
                                .font(.system(size: 15, weight: .semibold))
                                .foregroundColor(FireballTheme.primaryText)
                            Text(metrics.isSecureHttps ? "Your information (passwords, cookies) is private." : "Do not enter sensitive information on this site.")
                                .font(.system(size: 12))
                                .foregroundColor(FireballTheme.mutedText)
                        }
                    }
                    .listRowBackground(FireballTheme.cardSurface)
                }

                // Site Permissions
                Section(header: Text("SITE PERMISSIONS").foregroundColor(FireballTheme.secondaryText)) {
                    Toggle("Location Access", isOn: $locationAllowed)
                        .tint(FireballTheme.electricLime)
                    Toggle("Camera Access", isOn: $cameraAllowed)
                        .tint(FireballTheme.electricLime)
                    Toggle("Microphone Access", isOn: $micAllowed)
                        .tint(FireballTheme.electricLime)
                    Toggle("Notifications", isOn: $notificationsAllowed)
                        .tint(FireballTheme.electricLime)
                }
                .listRowBackground(FireballTheme.cardSurface)

                // Storage & Reset
                Section(header: Text("SITE STORAGE & CACHE").foregroundColor(FireballTheme.secondaryText)) {
                    HStack {
                        Text("Stored Data & Cookies")
                            .foregroundColor(FireballTheme.primaryText)
                        Spacer()
                        Text("Active")
                            .foregroundColor(FireballTheme.mutedText)
                    }

                    Button(role: .destructive, action: onClearSiteData) {
                        HStack {
                            Image(systemName: "trash")
                            Text("Clear Data & Cookies for \(metrics.domain)")
                        }
                    }
                }
                .listRowBackground(FireballTheme.cardSurface)
            }
            .scrollContentBackground(.hidden)
            .background(FireballTheme.background)
            .navigationTitle("Site Information")
            #if os(iOS)
            .navigationBarTitleDisplayMode(.inline)
            #endif
            .toolbar {
                ToolbarItem(placement: .confirmationAction) {
                    Button("Done", action: onDismiss)
                        .foregroundColor(FireballTheme.electricLime)
                }
            }
        }
    }
}
