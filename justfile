test:
    tree-sitter generate
    tree-sitter build --wasm
    tree-sitter playground

gen:
    tree-sitter generate

see: gen
    tree-sitter parse examples/sample.wikitext | batcat
