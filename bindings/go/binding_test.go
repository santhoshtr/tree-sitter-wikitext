package tree_sitter_wikitext_test

import (
	"testing"

	tree_sitter "github.com/smacker/go-tree-sitter"
	"github.com/tree-sitter/tree-sitter-wikitext"
)

func TestCanLoadGrammar(t *testing.T) {
	language := tree_sitter.NewLanguage(tree_sitter_wikitext.Language())
	if language == nil {
		t.Errorf("Error loading Wikitext grammar")
	}
}
