import SwiftUI
import FireballCore

public struct ShieldsSheet: View {
    @Binding public var config: ShieldsConfig
    public let metrics: ShieldsSiteMetrics
    public let onDismiss: () -> Void

    public init(config: Binding<ShieldsConfig>, metrics: ShieldsSiteMetrics, onDismiss: @escaping () -> Void) {
        self._config = config
        self.metrics = metrics
        self.onDismiss = onDismiss
    }

    public var body: some View {
        NavigationStack {
            List {
                // Header Metric Card
                Section {
                    VStack(alignment: .center, spacing: 10) {
                        Image(systemName: "shield.checkered")
                            .font(.system(size: 40))
                            .foregroundColor(FireballTheme.electricLime)

                        Text("\(metrics.blockedTrackersCount)")
                            .font(.system(size: 36, weight: .black))
                            .foregroundColor(FireballTheme.primaryText)

                        Text("Trackers & Ads Blocked on \(metrics.domain)")
                            .font(.system(size: 13, weight: .medium))
                            .foregroundColor(FireballTheme.secondaryText)
                            .multilineTextAlignment(.center)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 10)
                    .listRowBackground(FireballTheme.cardSurface)
                }

                // Shields Controls
                Section(header: Text("PROTECTION ENGINES").foregroundColor(FireballTheme.secondaryText)) {
                    Toggle("Block Trackers & Ads", isOn: $config.adBlockEnabled)
                        .tint(FireballTheme.electricLime)
                    Toggle("Cosmetic Element Hiding", isOn: $config.cosmeticFilteringEnabled)
                        .tint(FireballTheme.electricLime)
                    Toggle("Strip Tracking URL Parameters", isOn: $config.stripTrackingEnabled)
                        .tint(FireballTheme.electricLime)
                    Toggle("Anti-Fingerprinting Protection", isOn: $config.fingerprintingProtection)
                        .tint(FireballTheme.electricLime)
                    Toggle("Automatic HTTPS Upgrades", isOn: $config.httpsEverywhere)
                        .tint(FireballTheme.electricLime)
                    Toggle("Block All JavaScript", isOn: $config.blockScripts)
                        .tint(FireballTheme.meteorOrange)
                }
                .listRowBackground(FireballTheme.cardSurface)
            }
            .scrollContentBackground(.hidden)
            .background(FireballTheme.background)
            .navigationTitle("Fireball Shields")
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
