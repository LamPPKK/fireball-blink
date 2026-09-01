import Foundation

public struct ShieldsConfig: Codable, Sendable, Equatable {
    public var adBlockEnabled: Bool
    public var cosmeticFilteringEnabled: Bool
    public var stripTrackingEnabled: Bool
    public var fingerprintingProtection: Bool
    public var httpsEverywhere: Bool
    public var blockScripts: Bool

    public init(
        adBlockEnabled: Bool = true,
        cosmeticFilteringEnabled: Bool = true,
        stripTrackingEnabled: Bool = true,
        fingerprintingProtection: Bool = true,
        httpsEverywhere: Bool = true,
        blockScripts: Bool = false
    ) {
        self.adBlockEnabled = adBlockEnabled
        self.cosmeticFilteringEnabled = cosmeticFilteringEnabled
        self.stripTrackingEnabled = stripTrackingEnabled
        self.fingerprintingProtection = fingerprintingProtection
        self.httpsEverywhere = httpsEverywhere
        self.blockScripts = blockScripts
    }
}

public struct ShieldsSiteMetrics: Identifiable, Codable, Sendable, Equatable {
    public let id: String
    public var domain: String
    public var blockedTrackersCount: Int
    public var isSecureHttps: Bool
    public var cookieUsageBytes: Int64
    public var cacheUsageBytes: Int64

    public init(
        domain: String,
        blockedTrackersCount: Int = 0,
        isSecureHttps: Bool = true,
        cookieUsageBytes: Int64 = 0,
        cacheUsageBytes: Int64 = 0
    ) {
        self.id = domain
        self.domain = domain
        self.blockedTrackersCount = blockedTrackersCount
        self.isSecureHttps = isSecureHttps
        self.cookieUsageBytes = cookieUsageBytes
        self.cacheUsageBytes = cacheUsageBytes
    }
}
