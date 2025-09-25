// swift-tools-version:5.3
import Foundation
import PackageDescription

var sources = ["src/parser.c","src/scanner.c"]

let package = Package(
    name: "TreeSitterWikitext",
    products: [
        .library(name: "TreeSitterWikitext", targets: ["TreeSitterWikitext"]),
    ],
    dependencies: [
        .package(name: "SwiftTreeSitter", url: "https://github.com/tree-sitter/swift-tree-sitter", from: "0.9.0"),
    ],
    targets: [
        .target(name: "TreeSitterWikitext",
            path: ".",
            sources: sources,
            exclude: [
                "Cargo.toml",
                "Makefile",
                "binding.gyp",
                "bindings/c",
                "bindings/go",
                "bindings/node",
                "bindings/python",
                "bindings/rust",
                "prebuilds",
                "grammar.js",
                "package.json",
                "package-lock.json",
                "pyproject.toml",
                "setup.py",
                "test",
                "examples",
                ".editorconfig",
                ".github",
                ".gitignore",
                ".gitattributes",
                ".gitmodules",
            ],
            resources: [
                .copy("queries")
            ],
            publicHeadersPath: "bindings/swift",
            cSettings: [.headerSearchPath("src")]
        )
        .testTarget(
            name: "TreeSitterWikitextTests",
            dependencies: [
                "SwiftTreeSitter",
                "TreeSitterWikitext",
            ],
            path: "bindings/swift/TreeSitterWikitextTests"
        )
    ],
    cLanguageStandard: .c11
)
