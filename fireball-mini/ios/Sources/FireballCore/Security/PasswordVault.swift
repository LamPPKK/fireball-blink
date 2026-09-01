import Foundation
import CryptoKit

public actor PasswordVault {
    private var credentials: [SavedCredential] = []
    private let symmetricKey: SymmetricKey

    public init(masterSeed: Data? = nil) {
        if let seed = masterSeed {
            let digest = SHA256.hash(data: seed)
            self.symmetricKey = SymmetricKey(data: digest)
        } else {
            self.symmetricKey = SymmetricKey(size: .bits256)
        }
    }

    public func getAllCredentials() -> [SavedCredential] {
        return credentials
    }

    public func findCredentials(for domain: String) -> [SavedCredential] {
        return credentials.filter { $0.domain.localizedCaseInsensitiveContains(domain) }
    }

    public func saveCredential(domain: String, username: String, plainPassword: String) throws -> SavedCredential {
        let passwordData = Data(plainPassword.utf8)
        let sealedBox = try AES.GCM.seal(passwordData, using: symmetricKey)
        
        let combined = sealedBox.ciphertext + sealedBox.tag
        let nonceData = Data(sealedBox.nonce)

        let saved = SavedCredential(
            domain: domain,
            username: username,
            encryptedPasswordBase64: combined.base64EncodedString(),
            nonceBase64: nonceData.base64EncodedString()
        )

        credentials.removeAll { $0.domain == domain && $0.username == username }
        credentials.append(saved)
        return saved
    }

    public func decryptCredential(_ credential: SavedCredential) throws -> DecryptedCredential {
        guard let combinedData = Data(base64Encoded: credential.encryptedPasswordBase64),
              let nonceData = Data(base64Encoded: credential.nonceBase64),
              combinedData.count >= 16 else {
            throw VaultError.invalidData
        }

        let nonce = try AES.GCM.Nonce(data: nonceData)
        let tag = combinedData.suffix(16)
        let ciphertext = combinedData.prefix(combinedData.count - 16)

        let sealedBox = try AES.GCM.SealedBox(nonce: nonce, ciphertext: ciphertext, tag: tag)
        let decryptedData = try AES.GCM.open(sealedBox, using: symmetricKey)

        guard let plainPassword = String(data: decryptedData, encoding: .utf8) else {
            throw VaultError.decryptionFailed
        }

        return DecryptedCredential(
            id: credential.id,
            domain: credential.domain,
            username: credential.username,
            plainPassword: plainPassword
        )
    }

    public func deleteCredential(id: String) {
        credentials.removeAll { $0.id == id }
    }
}

public enum VaultError: Error {
    case invalidData
    case decryptionFailed
}
