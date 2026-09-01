import XCTest
@testable import FireballCore

final class PasswordVaultTests: XCTestCase {
    func testCredentialEncryptionAndDecryptionRoundTrip() async throws {
        let vault = PasswordVault(masterSeed: "fireball-secure-seed-12345".data(using: .utf8)!)
        
        let saved = try await vault.saveCredential(
            domain: "github.com",
            username: "lamndt",
            plainPassword: "SuperSecretPassword123!"
        )

        XCTAssertEqual(saved.domain, "github.com")
        XCTAssertEqual(saved.username, "lamndt")
        XCTAssertFalse(saved.encryptedPasswordBase64.isEmpty)
        XCTAssertFalse(saved.encryptedPasswordBase64.contains("SuperSecretPassword123!"))

        let decrypted = try await vault.decryptCredential(saved)
        XCTAssertEqual(decrypted.domain, "github.com")
        XCTAssertEqual(decrypted.username, "lamndt")
        XCTAssertEqual(decrypted.plainPassword, "SuperSecretPassword123!")
    }

    func testCredentialDeletion() async throws {
        let vault = PasswordVault()
        let saved = try await vault.saveCredential(
            domain: "reddit.com",
            username: "user_test",
            plainPassword: "PassWord999"
        )

        let initialList = await vault.getAllCredentials()
        XCTAssertEqual(initialList.count, 1)

        await vault.deleteCredential(id: saved.id)
        let afterList = await vault.getAllCredentials()
        XCTAssertEqual(afterList.count, 0)
    }
}
