# Tree-Sitter Wikitext Parser

This repository contains the implementation of a **Tree-Sitter** parser for **Wikitext**, a markup language used by MediaWiki.

## Overview

Tree-Sitter is a powerful parser generator tool and incremental parsing library. It is designed to build concrete syntax trees for source files and efficiently update them as the source changes. This project leverages Tree-Sitter to parse Wikitext, enabling structured analysis and manipulation of MediaWiki content.

## Features

- **Incremental Parsing**: Efficiently updates syntax trees as the source changes.
- **Language Agnostic**: Can be embedded in applications written in C, Python, Go, Rust, Node.js, and Swift.
- **Robust Parsing**: Handles syntax errors gracefully to provide useful results.
- **Custom Grammar**: Implements a grammar tailored for Wikitext.

## Repository Structure

- **`src/`**: Contains the core C implementation of the parser.
- **`bindings/`**: Language-specific bindings for Python, Go, Node.js, Rust, and Swift.
- **`grammar.js`**: Defines the grammar for Wikitext.
- **`queries/`**: Contains Tree-Sitter query files for extracting specific syntax patterns.
- **`tests/`**: Unit tests for validating the parser's functionality.

## Installation

### Prerequisites

- A C compiler (e.g., GCC or Clang)
- [Node.js](https://nodejs.org/) (for building the grammar)
- Python 3.6+ (optional, for Python bindings)

### Build Instructions

1. Clone the repository:

   ```bash
   git clone https://github.com/santhoshtr/tree-sitter-wikitext.git
   cd tree-sitter-wikitext
   ```

2. Build the parser:

   ```bash
   npm install
   ```

3. (Optional) Build language-specific bindings:
   - **Python**: Run `python setup.py build`.
   - **Rust**: Use `cargo build`.
   - **Go**: Use `go build`.

## Usage

### Embedding in Applications

The parser can be embedded in applications written in various languages. For example:

- **Python**: Use the `tree-sitter` Python module to load and use the parser.
- **Node.js**: Import the parser as a Node.js module.
- **Rust**: Use the `tree-sitter` crate to integrate the parser.

### Example: Parsing Wikitext in Rust

```rust
use tree_sitter::{Parser, Language};

fn main() {
    // Create a new parser
    let mut parser = tree_sitter::Parser::new();
    parser.set_language(&tree_sitter_wikitext::LANGUAGE.into()).expect("Error loading wikitext grammar");

    // Parse a Wikitext string
    let source_code = "== Heading ==\nThis is a paragraph.\n";
    let tree = parser.parse(source_code, None).unwrap();

    // Print the syntax tree
    println!("{}", tree.root_node().to_sexp());
}
```

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Submit a pull request with a detailed description of your changes.

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.
