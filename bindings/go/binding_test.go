package tree_sitter_wikitext_test

import (
	"testing"

	tree_sitter "github.com/tree-sitter/go-tree-sitter"
	tree_sitter_wikitext "github.com/santhoshtr/tree-sitter-wikitext/bindings/go"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_wikitext.Language())
	if language == nil {
		t.Errorf("Error loading Wikitext grammar")
	}
}
