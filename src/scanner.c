
#include "tree_sitter/parser.h"

#include <stdio.h>
#include <wctype.h>

enum TokenType {
  COMMENT,
  // TEXT
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

bool tree_sitter_wikitext_external_scanner_scan(void *payload, TSLexer *lexer,
                                                const bool *valid_symbols) {
  if (valid_symbols[COMMENT]) {
    return scan_comment(lexer);
  }

  return false;
}
