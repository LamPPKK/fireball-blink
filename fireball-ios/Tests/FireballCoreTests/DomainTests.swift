import XCTest
@testable import FireballCore

final class DomainTests: XCTestCase {
    func testTabCreationAndDefaults() {
        let tab = FireballTab(
            url: URL(string: "https://duckduckgo.com")!,
            title: "DuckDuckGo"
        )
        XCTAssertEqual(tab.title, "DuckDuckGo")
        XCTAssertEqual(tab.tier, .today)
        XCTAssertFalse(tab.isBurner)
        XCTAssertFalse(tab.isPinned)
        XCTAssertFalse(tab.isFavorite)
    }

    func testBurnerTabForcesBurnerTier() {
        let burnerTab = FireballTab(
            url: URL(string: "https://github.com")!,
            title: "GitHub",
            isBurner: true
        )
        XCTAssertEqual(burnerTab.tier, .burner)
        XCTAssertTrue(burnerTab.isBurner)
    }

    func testSpaceAndActiveTabResolution() {
        let tab1 = FireballTab(title: "Tab 1")
        let tab2 = FireballTab(title: "Tab 2")
        let space = Space(name: "Work", tabs: [tab1, tab2], activeTabId: tab2.id)

        XCTAssertEqual(space.name, "Work")
        XCTAssertEqual(space.tabs.count, 2)
        XCTAssertEqual(space.activeTab?.id, tab2.id)
    }

    func testProfileSpacesLifecycle() {
        let space1 = Space(name: "Personal")
        let space2 = Space(name: "Burner Space", isBurner: true)
        let profile = Profile(name: "Default Profile", spaces: [space1, space2])

        XCTAssertEqual(profile.spaces.count, 2)
        XCTAssertEqual(profile.activeSpace?.name, "Personal")
    }
}
