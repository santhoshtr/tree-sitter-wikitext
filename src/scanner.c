
#include "tree_sitter/parser.h"

#include <stdio.h>
#include <wctype.h>

enum TokenType {
  COMMENT,
  INLINE_TEXT_BASE,
};

void *tree_sitter_wikitext_external_scanner_create() { return NULL; }

void tree_sitter_wikitext_external_scanner_destroy(void *p) {}

unsigned tree_sitter_wikitext_external_scanner_serialize(void *payload,
                                                         char *buffer) {
  return 0;
}

void tree_sitter_wikitext_external_scanner_deserialize(void *p, const char *b,
                                                       unsigned n) {}

static inline void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

static inline void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

static bool scan_raw_text(TSLexer *lexer) { return false; }

static bool scan_comment(TSLexer *lexer) {
  const char *comment_start = "<!--";

  if (lexer->lookahead == '<') {
    for (unsigned i = 0; i < 4; i++) {
      if (lexer->lookahead != comment_start[i]) {
        return false;
      }
      advance(lexer);
    }
  } else {
    return false;
  }
  unsigned dashes = 0;
  while (lexer->lookahead) {
    switch (lexer->lookahead) {
    case '-':
      ++dashes;
      break;
    case '>':
      if (dashes >= 2) {
        advance(lexer);
        lexer->mark_end(lexer);
        lexer->result_symbol = COMMENT;
        return true;
      }
    default:
      dashes = 0;
    }
    advance(lexer);
  }
  return false;
}

static bool scan_inline_text_base(TSLexer *lexer) {
  // Characters that should not be included in inline text
  const char *exclusions = "\n[]{}'!=*|#~&;";

  bool found_text = false;

  while (lexer->lookahead) {
    // Check if current character is in the exclusions
    bool is_excluded = false;
    for (const char *p = exclusions; *p; p++) {
      if (lexer->lookahead == *p) {
        is_excluded = true;
        break;
      }
    }

    if (is_excluded) {
      break;
    }

    advance(lexer);
    lexer->mark_end(lexer);
    found_text = true;
  }

  if (found_text) {
    lexer->result_symbol = INLINE_TEXT_BASE;
    return true;
  }

  return false;
}

bool tree_sitter_wikitext_external_scanner_scan(void *payload, TSLexer *lexer,
                                                const bool *valid_symbols) {
  if (valid_symbols[COMMENT] && scan_comment(lexer)) {
    return true;
  }

  if (valid_symbols[INLINE_TEXT_BASE] && scan_inline_text_base(lexer)) {
    return true;
  };

  return false;
}
