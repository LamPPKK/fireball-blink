import Foundation

public struct SavedCredential: Identifiable, Codable, Sendable, Equatable {
    public let id: String
    public var domain: String
    public var username: String
    public var encryptedPasswordBase64: String
    public var nonceBase64: String
    public var createdAt: Date
    public var updatedAt: Date

    public init(
        id: String = UUID().uuidString,
        domain: String,
        username: String,
        encryptedPasswordBase64: String,
        nonceBase64: String,
        createdAt: Date = Date(),
        updatedAt: Date = Date()
    ) {
        self.id = id
        self.domain = domain
        self.username = username
        self.encryptedPasswordBase64 = encryptedPasswordBase64
        self.nonceBase64 = nonceBase64
        self.createdAt = createdAt
        self.updatedAt = updatedAt
    }
}

public struct DecryptedCredential: Identifiable, Sendable, Equatable {
    public let id: String
    public let domain: String
    public let username: String
    public let plainPassword: String

    public init(id: String, domain: String, username: String, plainPassword: String) {
        self.id = id
        self.domain = domain
        self.username = username
        self.plainPassword = plainPassword
    }
}
