import XCTest
@testable import FireballCore

final class SearchEngineTests: XCTestCase {
    func testDefaultDuckDuckGoSearch() {
        let engine = SearchEngineDefaults.duckduckgo
        let searchUrl = engine.buildSearchUrl(query: "swiftui testing")
        XCTAssertEqual(searchUrl.absoluteString, "https://duckduckgo.com/?q=swiftui%20testing")
    }

    func testBangParserDirectUrls() {
        let httpsUrl = BangParser.resolveQuery(input: "https://apple.com")
        XCTAssertEqual(httpsUrl.absoluteString, "https://apple.com")

        let domainGuess = BangParser.resolveQuery(input: "github.com")
        XCTAssertEqual(domainGuess.absoluteString, "https://github.com")
    }

    func testBangShortcuts() {
        let ghUrl = BangParser.resolveQuery(input: "!gh fireball")
        XCTAssertEqual(ghUrl.absoluteString, "https://github.com/search?q=fireball")

        let ytUrl = BangParser.resolveQuery(input: "!yt lofi")
        XCTAssertEqual(ytUrl.absoluteString, "https://www.youtube.com/results?search_query=lofi")

        let wikiUrl = BangParser.resolveQuery(input: "!w quantum computing")
        XCTAssertEqual(wikiUrl.absoluteString, "https://en.wikipedia.org/wiki/Special:Search?search=quantum%20computing")
    }
}
