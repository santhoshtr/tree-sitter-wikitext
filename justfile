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
