
#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

enum TokenType {
  COMMENT,
  INLINE_TEXT_BASE,
  WIKI_LINK_TOKEN,
  MEDIA_LINK_TOKEN,
  FILE_SIZE_TOKEN,
  FILE_ALIGNMENT_TOKEN,
  FILE_FORMAT_TOKEN,
  FILE_LINK_TOKEN,
  FILE_ALT_TOKEN,
  FILE_CAPTION_TOKEN,
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

static bool scan_file_size(TSLexer *lexer) {
  int digits = 0;
  while (lexer->lookahead >= '0' && lexer->lookahead <= '9') {
    advance(lexer);
    digits++;
  }

  if (digits > 0 && consume_string("px", lexer)) {
    consume_string("px", lexer);
    lexer->result_symbol = FILE_SIZE_TOKEN;
    return true;
  }
  return false;
}

static bool scan_file_alignment(TSLexer *lexer) {
  if (consume_string("left", lexer) || consume_string("right", lexer) ||
      consume_string("center", lexer) || consume_string("none", lexer)) {
    lexer->result_symbol = FILE_ALIGNMENT_TOKEN;
    return true;
  }
  return false;
}

static bool scan_file_format(TSLexer *lexer) {
  if (consume_string("thumb", lexer) || consume_string("thumbnail", lexer) ||
      consume_string("frameless", lexer) || consume_string("framed", lexer) ||
      consume_string("frame", lexer)) {
    lexer->result_symbol = FILE_FORMAT_TOKEN;
    return true;
  }
  return false;
}

static bool scan_file_link(TSLexer *lexer) {
  if (consume_string("link=", lexer)) {
    // Consume until | or ]]
    while (lexer->lookahead && lexer->lookahead != '|' &&
           !(lexer->lookahead == ']' && lexer->lookahead == ']')) {
      advance(lexer);
    }
    lexer->result_symbol = FILE_LINK_TOKEN;
    return true;
  }
  return false;
}

static bool scan_file_alt(TSLexer *lexer) {
  if (consume_string("alt=", lexer)) {
    // Consume until | or ]]
    while (lexer->lookahead && lexer->lookahead != '|' &&
           !(lexer->lookahead == ']' && lexer->lookahead == ']')) {
      advance(lexer);
    }
    lexer->result_symbol = FILE_ALT_TOKEN;
    return true;
  }
  return false;
}

static bool scan_file_caption(TSLexer *lexer) {
  // this is zero width
  lexer->mark_end(lexer);
  // file caption is last item in media
  lexer->result_symbol = FILE_CAPTION_TOKEN;
  return true;
}

static bool scan_comment(TSLexer *lexer) {
  if (!consume_string("<!--", lexer)) {
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
    if (bracket_count == 0) {
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
  int text_run_length = 0;
  int char_index = -1;

  lexer->mark_end(lexer);
  while (lexer->lookahead) {
    char_index++;
    // Check for special sequences that need matching
    if (lexer->lookahead == '[') {
      // Check if this is [[ (wikilink) or [ (external link)
      // This is [, check if it has matching ]
      // if (has_matching_brackets(lexer)) {
      // Has matching ], so this should be handled as wikilink
      break;
      //}
    }
    if (lexer->lookahead == ']') {
      break;
    }

    if (lexer->lookahead == '{') {
      // Check if this is {{
      if (is_opening_template(lexer)) {
        // Has matching }}, so this should be handled as template
        break;
      }
    }

    if (lexer->lookahead == '}') {
      if (consume_string("}}", lexer)) {
        break;
      }
    }

    if (lexer->lookahead == '~') {
      if (is_signature(lexer)) {
        // This is an signature, should be handled separately
        break;
      }
    }

    if (lexer->lookahead == '&') {
      // Check if this forms an HTML entity
      if (is_html_entity(lexer)) {
        // This is an HTML entity, should be handled separately
        break;
      }
    }

    if (lexer->lookahead == '\'') {
      // Check if this forms an bold or italic
      if (is_bold_italic(lexer)) {
        // This is a bold or italic , should be handled separately
        break;
      }
    }

    if (lexer->lookahead == '=' && char_index == 0) {
      // Check if this forms a MediaWiki header
      if (is_mediawiki_header(lexer)) {
        // This is a MediaWiki header, should be handled separately
        break;
      }
    }
    // Do not allow these chars if they are at beginning of text.
    if ((lexer->lookahead == '*' || lexer->lookahead == '#') &&
        char_index == 0) {
      // should be handled separately
      break;
    }
    if (lexer->lookahead == '<') {
      if (consume_string("<!--", lexer)) {
        break;
      }
    }
    // FIXME:This should be properly handled - tables and html
    if (lexer->lookahead == '|' || lexer->lookahead == '\n') {
      // should be handled separately
      break;
    }
    advance(lexer);
    text_run_length++;
    lexer->mark_end(lexer);
  }
  if (text_run_length > 0) {
    lexer->result_symbol = INLINE_TEXT_BASE;
    return true;
  }

  return false;
}
// Scan for link opening patterns
static bool is_media_link_token(TSLexer *lexer) {
  // NOTE: At this point '[[' check passed
  // Check for "File:" or "Image:" or "Media:"
  if (lexer->lookahead == 'F') {
    if (consume_string("File", lexer)) {
      advance(lexer); // Skip ':'
      return true;
    }
    return false;
  }

  if (lexer->lookahead == 'I') {
    // Check for "Image:"
    if (consume_string("Image", lexer)) {
      advance(lexer); // Skip ':'
      return true;
    }
    return false;
  }

  if (lexer->lookahead == 'M' || lexer->lookahead == 'm') {
    // Check for "Media:"
    if (consume_string("Media", lexer)) {
      advance(lexer); // Skip ':'
      return true;
    }
    return false;
  }

  return false;
}

// Check if the lexer at current position can scan a wikilink_opening.
static bool is_wiki_link_open_token(TSLexer *lexer) {
  if (lexer->lookahead == '[') {
    advance(lexer);

    if (lexer->lookahead == '[') {
      return true;
    }
  }

  return false;
}

// Add this function before tree_sitter_wikitext_external_scanner_scan
static void dump_valid_symbols(const bool *valid_symbols) {
  printf("Valid symbols: ");
  if (valid_symbols[COMMENT]) {
    printf("COMMENT, ");
  }
  if (valid_symbols[INLINE_TEXT_BASE]) {
    printf("INLINE_TEXT_BASE, ");
  }
  if (valid_symbols[WIKI_LINK_TOKEN]) {
    printf("WIKI_LINK_TOKEN, ");
  }
  if (valid_symbols[MEDIA_LINK_TOKEN]) {
    printf("MEDIA_LINK_TOKEN, ");
  }
  if (valid_symbols[FILE_LINK_TOKEN]) {
    printf("FILE_LINK_TOKEN, ");
  }
  if (valid_symbols[FILE_ALIGNMENT_TOKEN]) {
    printf("FILE_ALIGNMENT_TOKEN,");
  }
  if (valid_symbols[FILE_ALT_TOKEN]) {
    printf("FILE_ALT_TOKEN,");
  }
  if (valid_symbols[FILE_SIZE_TOKEN]) {
    printf("FILE_SIZE_TOKEN,");
  }
  if (valid_symbols[FILE_CAPTION_TOKEN]) {
    printf("FILE_CAPTION_TOKEN,");
  }
  printf("\n");
}

bool tree_sitter_wikitext_external_scanner_scan(void *payload, TSLexer *lexer,
                                                const bool *valid_symbols) {
  /* dump_valid_symbols(valid_symbols); */
  // Handle file options in priority order (most specific first)
  if (valid_symbols[FILE_SIZE_TOKEN] && scan_file_size(lexer)) {
    return true;
  }

  if (valid_symbols[FILE_ALIGNMENT_TOKEN] && scan_file_alignment(lexer)) {
    return true;
  }

  if (valid_symbols[FILE_FORMAT_TOKEN] && scan_file_format(lexer)) {
    return true;
  }

  if (valid_symbols[FILE_LINK_TOKEN] && scan_file_link(lexer)) {
    return true;
  }

  if (valid_symbols[FILE_ALT_TOKEN] && scan_file_alt(lexer)) {
    return true;
  }

  // Caption should be last (least specific)
  if (valid_symbols[FILE_CAPTION_TOKEN] && scan_file_caption(lexer)) {
    return true;
  }
  switch (lexer->lookahead) {
  case '[':
    if (valid_symbols[MEDIA_LINK_TOKEN] || valid_symbols[WIKI_LINK_TOKEN]) {
      bool found_link = false;
      // This is zero width token.
      lexer->mark_end(lexer);
      if (valid_symbols[WIKI_LINK_TOKEN] && is_wiki_link_open_token(lexer)) {
        lexer->result_symbol = WIKI_LINK_TOKEN;
        found_link = true;
      }
      advance(lexer);
      if (found_link && valid_symbols[MEDIA_LINK_TOKEN] &&
          is_media_link_token(lexer)) {
        lexer->result_symbol = MEDIA_LINK_TOKEN;
        return true;
      }
      if (found_link) {
        return true;
      };
    }
    break;
  case '<':
    if (valid_symbols[COMMENT] && scan_comment(lexer)) {
      return true;
    }
    break;
  default:
    if (valid_symbols[INLINE_TEXT_BASE] && scan_inline_text_base(lexer)) {
      return true;
    };
  }
  //  dump_valid_symbols(valid_symbols);
  return false;
}
