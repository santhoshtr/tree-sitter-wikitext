
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

static inline bool consume_char(char c, TSLexer *lexer) {
  if (lexer->lookahead != c) {
    return false;
  }

  advance(lexer);
  return true;
}

static inline bool consume_string(char *sequence, TSLexer *lexer) {
  unsigned length = strlen(sequence);
  for (unsigned i = 0; i < length; i++) {
    if (lexer->lookahead != sequence[i]) {
      return false;
    }
    advance(lexer);
  }
  advance(lexer);
  return true;
}

static inline uint8_t consume_and_count_char(char c, TSLexer *lexer) {
  uint8_t count = 0;
  while (lexer->lookahead == c) {
    ++count;
    advance(lexer);
  }
  return count;
}

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

static bool is_opening_template(TSLexer *lexer) {
  int double_brace_count = 1;
  int line_count = 0;
  advance(lexer); // Skip the first '{'

  if (lexer->lookahead != '{') {
    return false; // Not a double brace
  }
  return true;
}
static bool is_closing_template(TSLexer *lexer) {
  int double_brace_count = 1;
  int line_count = 0;
  advance(lexer); // Skip the first '{'

  if (lexer->lookahead != '}') {
    return false; // Not a double brace
  }
  return true;
}

// Helper function to check if '=' forms a MediaWiki header
// MediaWiki headers: = Header =, == Header ==, === Header ===, etc.
static bool is_mediawiki_header(TSLexer *lexer) {
  // Count initial '=' characters
  int initial_equals = 1;
  advance(lexer); // Skip the first '='

  while (lexer->lookahead == '=') {
    initial_equals++;
    advance(lexer);
  }

  // Must have at least one non-'=' character (the header text)
  if (lexer->lookahead == '\n' || lexer->lookahead == '\0') {
    return false;
  }

  // Skip whitespace after initial '='
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    advance(lexer);
  }

  // Must have some header text
  bool has_text = false;
  while (lexer->lookahead && lexer->lookahead != '\n' &&
         lexer->lookahead != '=') {
    if (lexer->lookahead != ' ' && lexer->lookahead != '\t') {
      has_text = true;
    }
    advance(lexer);
  }

  if (!has_text) {
    return false;
  }

  // Skip trailing whitespace
  while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
    advance(lexer);
  }

  // Count trailing '=' characters
  int trailing_equals = 0;
  while (lexer->lookahead == '=') {
    trailing_equals++;
    advance(lexer);
  }

  // MediaWiki headers typically have matching '=' counts at start and end
  // But they can also have just opening '=' characters
  // Valid if we have trailing equals that match, or if we reach end of line
  if (trailing_equals > 0 && trailing_equals == initial_equals) {
    return true;
  }

  // Also valid if we just have opening '=' and reach end of line
  if (trailing_equals == 0 &&
      (lexer->lookahead == '\n' || lexer->lookahead == '\0')) {
    return true;
  }

  return false;
}

static bool scan_inline_text_base(TSLexer *lexer) {
  bool found_text = false;
  int char_index = -1;
  while (lexer->lookahead) {
    char_index++;
    lexer->mark_end(lexer);
    // Check for special sequences that need matching
    if (lexer->lookahead == '[') {
      // Check if this is [[ (wikilink) or [ (external link)
      // This is [, check if it has matching ]
      if (has_matching_brackets(lexer)) {
        // Has matching ], so this should be handled as wikilink
        break;
      }

      // No matching bracket, treat as regular text
      advance(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '{') {
      // Check if this is {{
      if (is_opening_template(lexer)) {
        // Has matching }}, so this should be handled as template
        break;
      }

      // No matching braces, treat as regular text
      advance(lexer);
      found_text = true;
      continue;
    }
    if (lexer->lookahead == '}') {
      // Check if this is {{
      if (is_closing_template(lexer)) {
        // Has matching }}, so this should be handled as template
        break;
      }

      // No matching braces, treat as regular text
      advance(lexer);
      found_text = true;
      continue;
    }
    if (lexer->lookahead == '~') {
      if (is_signature(lexer)) {
        // This is an signature, should be handled separately
        break;
      }

      // Not a signature, treat as regular text
      advance(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '&') {
      // Check if this forms an HTML entity
      if (is_html_entity(lexer)) {
        // This is an HTML entity, should be handled separately
        break;
      }

      // Not an HTML entity, treat as regular text
      advance(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '\'') {
      // Check if this forms an bold or italic
      if (is_bold_italic(lexer)) {
        // This is a bold or italic , should be handled separately
        break;
      }

      advance(lexer);
      found_text = true;
      continue;
    }

    if (lexer->lookahead == '=' && char_index == 0) {
      // Check if this forms a MediaWiki header
      if (is_mediawiki_header(lexer)) {
        // This is a MediaWiki header, should be handled separately
        break;
      }

      // Not a MediaWiki header, treat as regular text
      advance(lexer);
      found_text = true;
      continue;
    }
    // Do not allow these chars if they are at beginning of text.
    if ((lexer->lookahead == '*' || lexer->lookahead == '#') &&
        char_index == 0) {
      // should be handled separately
      break;
    }

    // FIXME:This should be properly handled - tables and html
    if (lexer->lookahead == '|' || lexer->lookahead == '<' ||
        lexer->lookahead == '\n') {
      // should be handled separately
      break;
    }

    advance(lexer);
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
