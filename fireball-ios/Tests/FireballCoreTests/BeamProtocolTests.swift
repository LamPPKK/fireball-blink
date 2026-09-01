import XCTest
@testable import FireballCore

final class BeamProtocolTests: XCTestCase {
    func testBeamJsonPacketEncodingAndDecoding() throws {
        let payload: [String: Any] = [
            "tab_id": "tab-ios-99",
            "url": "https://apple.com",
            "title": "Apple",
            "is_secure": true
        ]

        let packet = try BeamPacketCodec.encodeJsonPacket(type: .tabStateUpdate, payload: payload)
        XCTAssertTrue(packet.starts(with: "FBEAM".data(using: .utf8)!))

        let (decodedType, decodedPayload) = try BeamPacketCodec.decodePacket(packet)
        XCTAssertEqual(decodedType, .tabStateUpdate)
        XCTAssertEqual(decodedPayload["tab_id"] as? String, "tab-ios-99")
        XCTAssertEqual(decodedPayload["url"] as? String, "https://apple.com")
        XCTAssertEqual(decodedPayload["is_secure"] as? Bool, true)
    }

    func testNormalizedTouchClamping() {
        let touch1 = NormalizedTouch(id: 0, normX: 0.5, normY: 0.5, pressure: 1.0)
        XCTAssertEqual(touch1.normX, 0.5)
        XCTAssertEqual(touch1.normY, 0.5)

        let touchOverflow = NormalizedTouch(id: 1, normX: 1.8, normY: -0.4, pressure: 0.5)
        XCTAssertEqual(touchOverflow.normX, 1.0)
        XCTAssertEqual(touchOverflow.normY, 0.0)
    }
}
