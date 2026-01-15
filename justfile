default:
    @just --choose
test: gen
    tree-sitter test
playground: gen wasm
    tree-sitter playground

wasm: gen
    tree-sitter build --wasm
gen:
    tree-sitter generate
    tree-sitter build

see: gen
    tree-sitter parse --cst examples/sample.wikitext | bat
see-links: gen
    tree-sitter parse --cst examples/links.wikitext | bat
see-tables: gen
    tree-sitter parse --cst examples/tables.wikitext | bat
see-templates: gen
    tree-sitter parse --cst examples/templates.wikitext -d| bat
see-comments: gen
    tree-sitter parse --cst examples/comment.wikitext -d| bat
see-lists: gen
    tree-sitter parse --cst examples/lists.wikitext -d| bat
see-html: gen
    tree-sitter parse --cst examples/html.wikitext -d| bat
test-bindings:
    # Ref https://github.com/tree-sitter/parser-test-action/
    cargo build --all-features
    cargo test -q --all-features --no-fail-fast
    npm install
    node --test bindings/node/*_test.js
    uv pip install -e ".[core]"
    uv run -m unittest discover -v -s bindings/python/tests
    cd bindings/go && go mod tidy && go test -v && cd ../..
    which swift > /dev/null && (swift build --build-tests && swift test --skip-build -q) || echo "Swift not available, skipping Swift tests"

