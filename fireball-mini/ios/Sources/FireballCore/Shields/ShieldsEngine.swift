import Foundation

public enum ShieldsEngine {
    public static let trackingParameters: Set<String> = [
        "utm_source", "utm_medium", "utm_campaign", "utm_term", "utm_content",
        "fbclid", "gclid", "gbraid", "wbraid", "mc_eid", "yclid", "_hsenc",
        "_openstat", "igshid", "s_kwcid", "msclkid", "dclid"
    ]

    public static func cleanUrl(_ url: URL) -> URL {
        guard var components = URLComponents(url: url, resolvingAgainstBaseURL: false),
              let queryItems = components.queryItems,
              !queryItems.isEmpty else {
            return url
        }

        let filteredItems = queryItems.filter { item in
            !trackingParameters.contains(item.name.lowercased())
        }

        if filteredItems.isEmpty {
            components.query = nil
        } else {
            components.queryItems = filteredItems
        }

        return components.url ?? url
    }

    public static func generateCosmeticCss() -> String {
        return """
        /* Fireball Native Cosmetic Blocker */
        .ad, .ads, .ad-banner, .ad-container, .advertisement,
        div[id^='google_ads_iframe'], div[id*='taboola-'], div[id*='outbrain'],
        div[class*='sponsored-post'], .native-ad, .ad-slot {
            display: none !important;
            visibility: hidden !important;
            height: 0 !important;
            max-height: 0 !important;
            opacity: 0 !important;
            pointer-events: none !important;
        }
        """
    }
}
