
#include "tree_sitter/parser.h"

#include <stdbool.h>
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

// Helper function to check if a character sequence forms a valid HTML entity
static bool is_html_entity(TSLexer *lexer) {
  // Save current position
  uint32_t saved_position = lexer->get_column(lexer);

  // Skip the '&'
  advance(lexer);

  // Check for numeric entity &#...;
  if (lexer->lookahead == '#') {
    advance(lexer);

    // Check for hex entity &#x...;
    if (lexer->lookahead == 'x' || lexer->lookahead == 'X') {
      advance(lexer);
      int hex_digits = 0;
      while (lexer->lookahead &&
             ((lexer->lookahead >= '0' && lexer->lookahead <= '9') ||
              (lexer->lookahead >= 'a' && lexer->lookahead <= 'f') ||
              (lexer->lookahead >= 'A' && lexer->lookahead <= 'F'))) {
        advance(lexer);
        hex_digits++;
      }
      if (hex_digits > 0 && hex_digits <= 6 && lexer->lookahead == ';') {
        return true;
      }
    } else {
      // Check for decimal entity &#...;
      int decimal_digits = 0;
      while (lexer->lookahead && lexer->lookahead >= '0' &&
             lexer->lookahead <= '9') {
        advance(lexer);
        decimal_digits++;
      }
      if (decimal_digits > 0 && decimal_digits <= 5 &&
          lexer->lookahead == ';') {
        return true;
      }
    }
  } else {
    // Check for named entity &name;
    int name_chars = 0;
    while (lexer->lookahead &&
           ((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
            (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z'))) {
      advance(lexer);
      name_chars++;
    }
    if (name_chars > 0 && name_chars <= 30 && lexer->lookahead == ';') {
      return true;
    }
  }

  return false;
}

// Helper function to check if '[' has a matching ']'
static bool has_matching_brackets(TSLexer *lexer) {
  int bracket_count = 1;

  advance(lexer);
  while (lexer->lookahead) {
    if (lexer->lookahead == '[') {
      bracket_count++;
    } else if (lexer->lookahead == ']') {
      bracket_count--;
    } else if (lexer->lookahead == '\n') {
      // Brackets typically don't span multiple lines in wikitext
      break;
    }
    advance(lexer);
  }
  return bracket_count == 0;
}

// Helper function to check if consecutive '~' make up signature
//
// User signature: ~~~
// User signature with date: ~~~~
// Current date: ~~~~~
static bool is_signature(TSLexer *lexer) {
  int tilda_count = 1;
  advance(lexer); // Skip the opening '~'

  while (lexer->lookahead && tilda_count > 0) {
    if (lexer->lookahead == '~') {
      tilda_count++;
    } else {
      break;
    }
    advance(lexer);
  }

  return tilda_count == 3 || tilda_count == 4 || tilda_count == 5;
}
// Helper function to check if consecutive ' make up bold or italic
//
// '''Bold text'''
// ''Italic text''
// '''''Bold and italic text'''''
static bool is_bold_italic(TSLexer *lexer) {
  int tick_count = 1;
  advance(lexer); // Skip the opening '

  while (lexer->lookahead && tick_count > 0) {
    if (lexer->lookahead == '\'') {
      tick_count++;
    } else {
      break;
    }
    advance(lexer);
  }

  return tick_count == 2 || tick_count == 3 || tick_count == 5;
}

// Helper function to check if '{{' has a matching '}}'
static bool is_opening_template(TSLexer *lexer) {
  int double_brace_count = 1;
  int line_count = 0;
  advance(lexer); // Skip the first '{'

  if (lexer->lookahead != '{') {
    return false; // Not a double brace
  }
  return true;
}

static bool scan_inline_text_base(TSLexer *lexer) {
  // Characters that should not be included in inline text
  const char *exclusions = "\n!=;";

  bool found_text = false;

  while (lexer->lookahead) {
    // Check for special sequences that need matching
    if (lexer->lookahead == '[') {
      // Check if this is [[ (wikilink) or [ (external link)
      // This is [, check if it has matching ]
      TSLexer saved_lexer = *lexer;
      if (has_matching_brackets(lexer)) {
        // Has matching ], so this should be handled as wikilink
        *lexer = saved_lexer; // Restore position
        break;
      }

      // No matching bracket, treat as regular text
      *lexer = saved_lexer; // Restore position
      advance(lexer);
      lexer->mark_end(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '{') {
      // Check if this is {{
      TSLexer saved_lexer = *lexer;
      if (is_opening_template(lexer)) {
        // Has matching }}, so this should be handled as template
        *lexer = saved_lexer; // Restore position
        break;
      }

      // No matching braces, treat as regular text
      *lexer = saved_lexer; // Restore position
      advance(lexer);
      lexer->mark_end(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '~') {
      TSLexer saved_lexer = *lexer;
      if (is_signature(lexer)) {
        // This is an signature, should be handled separately
        *lexer = saved_lexer; // Restore position
        break;
      }

      // Not a signature, treat as regular text
      *lexer = saved_lexer; // Restore position
      advance(lexer);
      lexer->mark_end(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '&') {
      TSLexer saved_lexer = *lexer;
      // Check if this forms an HTML entity
      if (is_html_entity(lexer)) {
        *lexer = saved_lexer; // Restore position
        // This is an HTML entity, should be handled separately
        break;
      }

      // Not an HTML entity, treat as regular text
      *lexer = saved_lexer; // Restore position
      advance(lexer);
      lexer->mark_end(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '\'') {
      TSLexer saved_lexer = *lexer;
      // Check if this forms an bold or italic
      if (is_bold_italic(lexer)) {
        *lexer = saved_lexer; // Restore position
        // This is a bold or italic , should be handled separately
        break;
      }

      *lexer = saved_lexer; // Restore position
      advance(lexer);
      lexer->mark_end(lexer);
      found_text = true;
      continue;
    }

    // Skip * and # for now (will handle later with context tracking)
    if (lexer->lookahead == '*' || lexer->lookahead == '#' ||
        lexer->lookahead == '\n' || lexer->lookahead == '|' ||
        lexer->lookahead == '<') {
      // should be handled separately
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
