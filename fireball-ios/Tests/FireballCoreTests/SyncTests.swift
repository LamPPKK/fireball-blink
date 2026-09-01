import XCTest
@testable import FireballCore

final class SyncTests: XCTestCase {
    func testSyncPhraseGeneration() {
        let phrase = SyncEngine.generateSyncPhrase(wordCount: 24)
        XCTAssertEqual(phrase.count, 24)
        for word in phrase {
            XCTAssertTrue(SyncEngine.sampleWordlist.contains(word))
        }
    }

    func testDeterministicKeyAndPairingCode() {
        let phrase = ["abandon", "ability", "able", "about", "above", "absent"]
        let key1 = SyncEngine.deriveSyncKey(from: phrase)
        let key2 = SyncEngine.deriveSyncKey(from: phrase)

        let code1 = SyncEngine.generatePairingCode(from: phrase)
        let code2 = SyncEngine.generatePairingCode(from: phrase)

        XCTAssertEqual(code1, code2)
        XCTAssertEqual(code1.count, 6)
    }
}
