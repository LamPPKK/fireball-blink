// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "FireballIOS",
    platforms: [
        .iOS(.v16),
        .macOS(.v13)
    ],
    products: [
        .library(
            name: "FireballCore",
            targets: ["FireballCore"]
        ),
        .library(
            name: "FireballUI",
            targets: ["FireballUI"]
        ),
        .executable(
            name: "FireballTestRunner",
            targets: ["FireballTestRunner"]
        )
    ],
    dependencies: [],
    targets: [
        .target(
            name: "FireballCore",
            dependencies: [],
            path: "Sources/FireballCore"
        ),
        .target(
            name: "FireballUI",
            dependencies: ["FireballCore"],
            path: "Sources/FireballUI"
        ),
        .executableTarget(
            name: "FireballTestRunner",
            dependencies: ["FireballCore"],
            path: "Sources/FireballTestRunner"
        )
    ]
)
