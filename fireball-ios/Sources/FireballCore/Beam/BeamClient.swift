import Foundation

public enum BeamMessageType: UInt8, Sendable {
    case pairingRequest = 0x01
    case pairingChallenge = 0x02
    case pairingVerify = 0x03
    case pairingSuccess = 0x04
    case sessionStart = 0x05
    case sessionAck = 0x06
    case sessionHeartbeat = 0x07
    case sessionClose = 0x08

    case frameVideoH264 = 0x10
    case frameAudioOpus = 0x11
    case tabStateUpdate = 0x12
    case navigationCommitted = 0x13
    case fullscreenChange = 0x14
    case securityInfo = 0x15

    case inputTouchStart = 0x30
    case inputTouchMove = 0x31
    case inputTouchEnd = 0x32
    case inputTouchCancel = 0x33
    case inputKeyEvent = 0x34
    case inputScroll = 0x35
    case navigateUrl = 0x36
    case tabCommand = 0x37
}

public struct NormalizedTouch: Sendable, Codable, Equatable {
    public let id: Int
    public let normX: Float  // 0.0 to 1.0
    public let normY: Float  // 0.0 to 1.0
    public let pressure: Float

    public init(id: Int = 0, normX: Float, normY: Float, pressure: Float = 1.0) {
        self.id = id
        self.normX = max(0.0, min(1.0, normX))
        self.normY = max(0.0, min(1.0, normY))
        self.pressure = pressure
    }
}

public enum BeamClientState: Sendable, Equatable {
    case disconnected
    case pairing(step: String)
    case connected(serverName: String)
    case streaming(fps: Int)
    case error(message: String)
}

public final class BeamPacketCodec: @unchecked Sendable {
    public static let magic = "FBEAM".data(using: .utf8)!
    public static let version: UInt8 = 1

    public static func encodeJsonPacket(type: BeamMessageType, payload: [String: Any]) throws -> Data {
        let jsonData = try JSONSerialization.data(withJSONObject: payload, options: [])
        var packet = Data()
        packet.append(magic)
        packet.append(version)
        packet.append(type.rawValue)
        var lengthBigEndian = UInt32(jsonData.count).bigEndian
        packet.append(Data(bytes: &lengthBigEndian, count: 4))
        packet.append(jsonData)
        return packet
    }

    public static func decodePacket(_ data: Data) throws -> (BeamMessageType, [String: Any]) {
        guard data.count >= 11 else {
            throw BeamError.packetTooShort
        }

        let magicData = data.prefix(5)
        guard magicData == magic else {
            throw BeamError.invalidMagic
        }

        let versionByte = data[5]
        guard versionByte == version else {
            throw BeamError.unsupportedVersion
        }

        guard let msgType = BeamMessageType(rawValue: data[6]) else {
            throw BeamError.unknownMessageType
        }

        let lengthData = data.subdata(in: 7..<11)
        let length = lengthData.withUnsafeBytes { $0.load(as: UInt32.self).bigEndian }

        let payloadData = data.subdata(in: 11..<data.count)
        guard payloadData.count == Int(length) else {
            throw BeamError.payloadTruncated
        }

        guard let jsonObject = try JSONSerialization.jsonObject(with: payloadData) as? [String: Any] else {
            throw BeamError.invalidJson
        }

        return (msgType, jsonObject)
    }
}

public enum BeamError: Error {
    case packetTooShort
    case invalidMagic
    case unsupportedVersion
    case unknownMessageType
    case payloadTruncated
    case invalidJson
}
