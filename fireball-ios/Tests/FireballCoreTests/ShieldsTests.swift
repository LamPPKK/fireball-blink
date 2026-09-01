import XCTest
@testable import FireballCore

final class ShieldsTests: XCTestCase {
    func testTrackingParametersStripping() {
        let dirtyUrl = URL(string: "https://example.com/product?id=123&utm_source=twitter&utm_medium=social&fbclid=IwAR3XYZ&gclid=test")!
        let cleanUrl = ShieldsEngine.cleanUrl(dirtyUrl)

        XCTAssertEqual(cleanUrl.absoluteString, "https://example.com/product?id=123")
    }

    func testCleanUrlUntouched() {
        let cleanUrl = URL(string: "https://example.com/search?q=swift&page=2")!
        let result = ShieldsEngine.cleanUrl(cleanUrl)
        XCTAssertEqual(result.absoluteString, "https://example.com/search?q=swift&page=2")
    }

    func testCosmeticCssGeneration() {
        let css = ShieldsEngine.generateCosmeticCss()
        XCTAssertTrue(css.contains(".ad-banner"))
        XCTAssertTrue(css.contains("display: none !important;"))
    }
}
