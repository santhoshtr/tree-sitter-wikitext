test: gen wasm
    tree-sitter playground
wasm: gen
    tree-sitter build --wasm
gen:
    tree-sitter generate
    tree-sitter build

see: gen
    tree-sitter parse examples/sample.wikitext | bat
see-links: gen
    tree-sitter parse examples/links.wikitext | bat
   
