import Foundation
import CryptoKit

public enum SyncEngine {
    public static let sampleWordlist: [String] = [
        "abandon", "ability", "able", "about", "above", "absent", "absorb", "abstract",
        "absurd", "abuse", "access", "accident", "account", "accuse", "achieve", "acid",
        "acoustic", "acquire", "across", "act", "action", "actor", "actress", "actual",
        "adapt", "add", "addict", "address", "adjust", "admit", "adult", "advance",
        "advice", "aerobic", "affair", "afford", "afraid", "again", "age", "agent",
        "agree", "ahead", "aim", "air", "airport", "aisle", "alarm", "album",
        "alcohol", "alert", "alien", "all", "alley", "allow", "almost", "alone",
        "alpha", "already", "also", "alter", "always", "amateur", "amazing", "among"
    ]

    public static func generateSyncPhrase(wordCount: Int = 24) -> [String] {
        var words: [String] = []
        for _ in 0..<wordCount {
            let randomIndex = Int.random(in: 0..<sampleWordlist.count)
            words.append(sampleWordlist[randomIndex])
        }
        return words
    }

    public static func deriveSyncKey(from phrase: [String]) -> SymmetricKey {
        let joinedPhrase = phrase.joined(separator: " ")
        let inputData = Data(joinedPhrase.utf8)
        let hash = SHA256.hash(data: inputData)
        return SymmetricKey(data: hash)
    }

    public static func generatePairingCode(from phrase: [String]) -> String {
        let key = deriveSyncKey(from: phrase)
        let data = key.withUnsafeBytes { Data($0) }
        let hash = SHA256.hash(data: data)
        let hexString = hash.compactMap { String(format: "%02x", $0) }.joined()
        return String(hexString.prefix(6)).uppercased()
    }
}
