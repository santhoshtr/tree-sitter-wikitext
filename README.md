# Tree-Sitter Wikitext Parser

[![PyPI Version](https://img.shields.io/pypi/v/tree-sitter-wikitext.svg)](https://pypi.org/project/tree-sitter-wikitext/)
[![npm Version](https://img.shields.io/npm/v/tree-sitter-wikitext.svg)](https://www.npmjs.com/package/tree-sitter-wikitext)
[![crates.io Version](https://img.shields.io/crates/v/tree-sitter-wikitext.svg)](https://crates.io/crates/tree-sitter-wikitext)


This repository contains the implementation of a **Tree-Sitter** parser for **Wikitext**, a markup language used by MediaWiki.

Try the parse in the [playground](https://tree-sitter-wikitext.toolforge.org/)

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

### Example: Parsing Wikitext in Python

First, install the required dependencies:

```bash
pip install tree-sitter
```

Then use the parser in your Python code:

```python
from tree_sitter import Language, Parser, Query, QueryCursor
import tree_sitter_wikitext as tswikitext

# Create a language object
WIKITEXT_LANGUAGE = Language(tswikitext.language())

# Create a parser
parser = Parser(WIKITEXT_LANGUAGE)


# Parse some wikitext
source_code = b"""
== Introduction ==
This is a '''bold''' text with ''italic'' formatting.

* List item 1
* List item 2

[[Link to another page]]
"""

tree = parser.parse(source_code)

# Print the syntax tree
print(tree.root_node)

# Walk through the tree
def walk_tree(node, depth=0):
    indent = "  " * depth
    print(f"{indent}{node.type}: {node.text.decode('utf-8')[:50]}")
    for child in node.children:
        walk_tree(child, depth + 1)

walk_tree(tree.root_node)

# Query for specific nodes (e.g., all headings)
query = Query(WIKITEXT_LANGUAGE,
    """
(heading2) @heading
""")

query_cursor = QueryCursor(query)
captures = query_cursor.captures(tree.root_node)
for  capture_name in captures:
    print(f"Found {capture_name}: {captures[capture_name][0].text.decode('utf-8').strip()}")```

### Example: Parsing Wikitext in Node.js

First, install the parser:

```bash
npm install tree-sitter tree-sitter-wikitext
```

Then use it in your Node.js application:

```javascript
const Parser = require('tree-sitter');
const Wikitext = require('tree-sitter-wikitext');

// Create a parser
const parser = new Parser();
parser.setLanguage(Wikitext);

// Parse some wikitext
const sourceCode = `
== Introduction ==
This is a '''bold''' text with ''italic'' formatting.

* List item 1
* List item 2

[[Link to another page]]
`;

const tree = parser.parse(sourceCode);

// Print the syntax tree
console.log(tree.rootNode.toString());

// Walk through the tree
function walkTree(node, depth = 0) {
    const indent = "  ".repeat(depth);
    console.log(`${indent}${node.type}: ${node.text.substring(0, 50)}`);

    for (const child of node.children) {
        walkTree(child, depth + 1);
    }
}

walkTree(tree.rootNode);

// Query for specific nodes
const query = Wikitext.query(`
(heading) @heading
(bold) @bold
(italic) @italic
(link) @link
`);

const captures = query.captures(tree.rootNode);
captures.forEach(capture => {
    console.log(`Found ${capture.name}: ${capture.node.text.trim()}`);
});

// Find all headings
function findHeadings(node) {
    const headings = [];

    if (node.type === 'heading') {
        headings.push({
            level: node.children.filter(c => c.type === 'heading_marker')[0]?.text.length || 2,
            text: node.text.replace(/^=+\s*|\s*=+$/g, '').trim()
        });
    }

    for (const child of node.children) {
        headings.push(...findHeadings(child));
    }

    return headings;
}

const headings = findHeadings(tree.rootNode);
console.log('Headings found:', headings);
```

### Advanced Usage in Node.js

For more advanced use cases, you can create incremental parsers and handle large documents:

```javascript
const Parser = require('tree-sitter');
const Wikitext = require('tree-sitter-wikitext');

class WikitextProcessor {
    constructor() {
        this.parser = new Parser();
        this.parser.setLanguage(Wikitext);
    }

    parseDocument(content) {
        return this.parser.parse(content);
    }

    updateDocument(oldTree, content, startIndex, oldEndIndex, newEndIndex) {
        // For incremental parsing
        oldTree.edit({
            startIndex,
            oldEndIndex,
            newEndIndex,
            startPosition: { row: 0, column: startIndex },
            oldEndPosition: { row: 0, column: oldEndIndex },
            newEndPosition: { row: 0, column: newEndIndex }
        });

        return this.parser.parse(content, oldTree);
    }

    extractMetadata(tree) {
        const metadata = {
            headings: [],
            links: [],
            templates: [],
            categories: []
        };

        // Implementation would depend on your specific grammar rules
        // This is a simplified example
        function traverse(node) {
            switch (node.type) {
                case 'heading':
                    metadata.headings.push(node.text.trim());
                    break;
                case 'link':
                    metadata.links.push(node.text.trim());
                    break;
                // Add more cases based on your grammar
            }

            for (const child of node.children) {
                traverse(child);
            }
        }

        traverse(tree.rootNode);
        return metadata;
    }
}

// Usage
const processor = new WikitextProcessor();
const tree = processor.parseDocument(sourceCode);
const metadata = processor.extractMetadata(tree);
console.log('Document metadata:', metadata);
```

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

### Using with Neovim

Checkout the repo, add the following configuration to `init.lua` of your nvim installation.

```lua
--- Refer https://github.com/nvim-treesitter/nvim-treesitter
local parser_config = require("nvim-treesitter.parsers").get_parser_configs()
parser_config.wikitext = {
  install_info = {
    url = "~/path/to/tree-sitter-wikitext", -- local path or git repo
    files = { "src/parser.c" }, -- note that some parsers also require src/scanner.c or src/scanner.cc
    -- optional entries:
    branch = "main", -- default branch in case of git repo if different from master
    generate_requires_npm = false, -- if stand-alone parser without npm dependencies
    requires_generate_from_grammar = false, -- if folder contains pre-generated src/parser.c
  },
  filetype = "wikitext", -- if filetype does not match the parser name
}

vim.filetype.add({
  pattern = {
    [".*/*.wikitext"] = "wikitext",
  },
})
```

Link the queries folder of `tree-sitter-wikitext` to `queries/wikitext` folder of nvim

```bash
cd ~/.config/nvim
mkdir -p queries
ln -s path/to/tree-sitter-wikitext/queries queries/wikitext
```

Re-open nvim. Open any file with `.wikitext` extension. You should see syntax highlighting. You can also inspect the tree-sitter tree using `:InspectTree` command

To run queries against a buffer, run `:EditQuery wikitext`. A scratch buffer will be opened. Write your Tree-Sitter query there, in normal node, move cursor over the capture names. You will see the corresponding text in the buffer get highlighted.

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork the repository.
2. Create a new branch for your feature or bug fix.
3. Submit a pull request with a detailed description of your changes.

## License

This project is licensed under the MIT License. See the `LICENSE.md` file for details.
