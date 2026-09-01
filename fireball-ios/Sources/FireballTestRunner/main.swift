import Foundation
import FireballCore

@main
struct FireballTestRunner {
    static func main() async {
        print("🚀 Running FireballCore Test Suite...")
        var passed = 0
        var failed = 0

        func assertEqual<T: Equatable>(_ actual: T, _ expected: T, _ message: String) {
            if actual == expected {
                passed += 1
            } else {
                print("❌ FAILED: \(message) | Expected \(expected), got \(actual)")
                failed += 1
            }
        }

        func assertTrue(_ condition: Bool, _ message: String) {
            if condition {
                passed += 1
            } else {
                print("❌ FAILED: \(message)")
                failed += 1
            }
        }

        // 1. Domain Tests
        print("  Testing Domain Models...")
        let tab = FireballTab(title: "DuckDuckGo")
        assertEqual(tab.title, "DuckDuckGo", "Tab title default")
        assertEqual(tab.tier, .today, "Tab tier default")
        let burnerTab = FireballTab(isBurner: true)
        assertEqual(burnerTab.tier, .burner, "Burner tab tier")

        let space = Space(name: "Work", tabs: [tab, burnerTab], activeTabId: burnerTab.id)
        assertEqual(space.tabs.count, 2, "Space tabs count")
        assertEqual(space.activeTab?.id, burnerTab.id, "Active tab resolution")

        // 2. Search Engine & Bangs Tests
        print("  Testing Search Engines & Bangs...")
        let ddg = SearchEngineDefaults.duckduckgo
        let ddgUrl = ddg.buildSearchUrl(query: "swiftui testing")
        assertEqual(ddgUrl.absoluteString, "https://duckduckgo.com/?q=swiftui%20testing", "DDG search URL")

        let ghUrl = BangParser.resolveQuery(input: "!gh fireball")
        assertEqual(ghUrl.absoluteString, "https://github.com/search?q=fireball", "!gh shortcut")
        let ytUrl = BangParser.resolveQuery(input: "!yt lofi")
        assertEqual(ytUrl.absoluteString, "https://www.youtube.com/results?search_query=lofi", "!yt shortcut")

        // 3. Shields Tests
        print("  Testing Shields & URL Cleaner...")
        let dirtyUrl = URL(string: "https://example.com/p?id=1&utm_source=tw&fbclid=123")!
        let cleanUrl = ShieldsEngine.cleanUrl(dirtyUrl)
        assertEqual(cleanUrl.absoluteString, "https://example.com/p?id=1", "Strip tracking params")
        let cosmeticCss = ShieldsEngine.generateCosmeticCss()
        assertTrue(cosmeticCss.contains("display: none !important;"), "Cosmetic CSS")

        // 4. Password Vault Tests
        print("  Testing Password Vault (AES-256-GCM)...")
        let vault = PasswordVault(masterSeed: "test-seed-12345".data(using: .utf8)!)
        do {
            let saved = try await vault.saveCredential(domain: "github.com", username: "lamndt", plainPassword: "SecretPassword123")
            assertEqual(saved.domain, "github.com", "Saved credential domain")
            let decrypted = try await vault.decryptCredential(saved)
            assertEqual(decrypted.plainPassword, "SecretPassword123", "Decrypted password")
            await vault.deleteCredential(id: saved.id)
            let all = await vault.getAllCredentials()
            assertEqual(all.count, 0, "Deleted credential")
        } catch {
            print("❌ Vault error: \(error)")
            failed += 1
        }

        // 5. Sync Tests
        print("  Testing Sync & BIP-39...")
        let phrase = SyncEngine.generateSyncPhrase(wordCount: 24)
        assertEqual(phrase.count, 24, "Sync phrase length")
        let code1 = SyncEngine.generatePairingCode(from: phrase)
        let code2 = SyncEngine.generatePairingCode(from: phrase)
        assertEqual(code1, code2, "Deterministic pairing code")

        // 6. Beam Protocol Tests
        print("  Testing Beam Protocol Codec...")
        do {
            let payload: [String: Any] = ["url": "https://apple.com", "trackers": 5]
            let packet = try BeamPacketCodec.encodeJsonPacket(type: .tabStateUpdate, payload: payload)
            assertTrue(packet.starts(with: "FBEAM".data(using: .utf8)!), "Beam magic header")
            let (msgType, decoded) = try BeamPacketCodec.decodePacket(packet)
            assertEqual(msgType, .tabStateUpdate, "Beam message type")
            assertEqual(decoded["url"] as? String, "https://apple.com", "Decoded URL")
        } catch {
            print("❌ Beam codec error: \(error)")
            failed += 1
        }

        print("\n==========================================")
        if failed == 0 {
            print("✅ All \(passed) FireballCore tests PASSED successfully!")
            exit(0)
        } else {
            print("❌ \(failed) tests failed out of \(passed + failed) total.")
            exit(1)
        }
    }
}
