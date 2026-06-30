
#include "tree_sitter/alloc.h"
#include "tree_sitter/parser.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "namespace_names.h"

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
    TEMPLATE_PARAM_VALUE_MARKER,
    TEMPLATE_PARAM_NAME_VALUE_MARKER,
    HTML_TAG_OPEN_MARKER,
    HTML_TAG_CLOSE_MARKER,
    HTML_SELF_CLOSING_TAG_MARKER,
    UNORDERED_LIST_MARKER,
    ORDERED_LIST_MARKER,
    TABLE_CELL_ATTRIBUTE_MARKER,
    TABLE_CELL_PLAIN_MARKER,
};

typedef struct {
    uint8_t self_closing_html_tag;
    uint8_t list_level;
} Scanner;

static inline void reset_state(Scanner *scanner) {
    scanner->self_closing_html_tag = 0;
    scanner->list_level = 0;
}

void *tree_sitter_wikitext_external_scanner_create() {
    Scanner *scanner = ts_calloc(1, sizeof(Scanner));
    return scanner;
}

void tree_sitter_wikitext_external_scanner_destroy(void *payload) {
    Scanner *scanner = (Scanner *)payload;
    ts_free(scanner);
}

unsigned tree_sitter_wikitext_external_scanner_serialize(void *payload,
                                                         char *buffer) {
    Scanner *scanner = (Scanner *)payload;
    buffer[0] = (char)scanner->self_closing_html_tag;
    buffer[1] = (char)scanner->list_level;
    return 2;
}

void tree_sitter_wikitext_external_scanner_deserialize(void *payload,
                                                       const char *buffer,
                                                       unsigned length) {
    Scanner *scanner = (Scanner *)payload;
    if (length == 0)
        return;
    scanner->self_closing_html_tag = buffer[0];
    scanner->list_level = buffer[1];
}

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

static bool is_allowed_self_closing_html_tag(const char *tag_name) {
    static const char *self_closing_tags[] = {"br", "hr", "meta", NULL};

    for (int i = 0; self_closing_tags[i] != NULL; i++) {
        if (strcmp(tag_name, self_closing_tags[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_allowed_html_tag(const char *tag_name) {
    // List of allowed HTML tags from MediaWiki documentation
    // https://www.mediawiki.org/wiki/Help:HTML_in_wikitext#
    static const char *allowed_tags[] = {
        "abbr", "b", "bdi", "bdo", "big", "blockquote", "br", "caption", "cite",
        "code", "col", "colgroup", "data", "dd", "del", "dfn", "div", "dl",
        "dt", "em", "h1", "h2", "h3", "h4", "h5", "h6", "hr", "i", "ins", "kbd",
        "li", "link", "mark", "ol", "p", "pre", "q", "rp", "rt", "ruby", "s",
        "samp", "small", "span", "strong", "sub", "sup", "table", "td", "th",
        "time", "tr", "u", "ul", "var", "wbr",
        // Deprecated but still allowed tags
        "center", "font", "rb", "rtc", "strike", "tt",
        // self_closing_tags
        "br", "meta", "hr",
        // wiki specific markup
        "noinclude", "onlyinclude", "includeonly",
        // Extension tag whose body is wikitext with significant line breaks; the
        // generic tag path handles its multi-line inline content.
        "poem",
        // References
        "ref", "references", NULL};

    for (int i = 0; allowed_tags[i] != NULL; i++) {
        if (strcmp(tag_name, allowed_tags[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_common_attribute(const char *attr_name) {
    // Common attributes allowed on most tags
    static const char *common_attrs[] = {
        // HTML attributes
        "id", "class", "style", "lang", "dir", "title", "tabindex",
        // WAI-ARIA attributes
        "aria-describedby", "aria-flowto", "aria-hidden", "aria-label",
        "aria-labelledby", "aria-level", "aria-owns", "role",
        // RDFa attributes
        "about", "property", "resource", "datatype", "typeof",
        // Microdata attributes
        "itemid", "itemprop", "itemref", "itemscope", "itemtype", NULL};

    for (int i = 0; common_attrs[i] != NULL; i++) {
        if (strcmp(attr_name, common_attrs[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_allowed_tag_specific_attribute(const char *tag_name,
                                              const char *attr_name) {
    // Tag-specific attributes based on MediaWiki documentation

    if (strcmp(tag_name, "blockquote") == 0 || strcmp(tag_name, "q") == 0) {
        return strcmp(attr_name, "cite") == 0;
    }

    if (strcmp(tag_name, "br") == 0) {
        return strcmp(attr_name, "clear") == 0;
    }

    if (strcmp(tag_name, "caption") == 0 || strcmp(tag_name, "div") == 0 ||
        strcmp(tag_name, "p") == 0) {
        return strcmp(attr_name, "align") == 0;
    }

    if (strcmp(tag_name, "col") == 0 || strcmp(tag_name, "colgroup") == 0) {
        return strcmp(attr_name, "span") == 0;
    }

    if (strcmp(tag_name, "data") == 0) {
        return strcmp(attr_name, "value") == 0;
    }

    if (strcmp(tag_name, "del") == 0 || strcmp(tag_name, "ins") == 0) {
        return strcmp(attr_name, "cite") == 0 ||
               strcmp(attr_name, "datetime") == 0;
    }

    if (strcmp(tag_name, "font") == 0) {
        return strcmp(attr_name, "color") == 0 ||
               strcmp(attr_name, "size") == 0 ||
               strcmp(attr_name, "face") == 0;
    }

    if (strcmp(tag_name, "h1") == 0 || strcmp(tag_name, "h2") == 0 ||
        strcmp(tag_name, "h3") == 0 || strcmp(tag_name, "h4") == 0 ||
        strcmp(tag_name, "h5") == 0 || strcmp(tag_name, "h6") == 0) {
        return strcmp(attr_name, "align") == 0;
    }

    if (strcmp(tag_name, "hr") == 0 || strcmp(tag_name, "pre") == 0) {
        return strcmp(attr_name, "width") == 0;
    }

    if (strcmp(tag_name, "li") == 0) {
        return strcmp(attr_name, "type") == 0 ||
               strcmp(attr_name, "value") == 0;
    }

    if (strcmp(tag_name, "link") == 0) {
        return strcmp(attr_name, "href") == 0 || strcmp(attr_name, "rel") == 0;
    }

    if (strcmp(tag_name, "meta") == 0) {
        return strcmp(attr_name, "content") == 0;
    }

    if (strcmp(tag_name, "ol") == 0) {
        return strcmp(attr_name, "type") == 0 ||
               strcmp(attr_name, "start") == 0 ||
               strcmp(attr_name, "reversed") == 0;
    }

    if (strcmp(tag_name, "table") == 0) {
        return strcmp(attr_name, "summary") == 0 ||
               strcmp(attr_name, "width") == 0 ||
               strcmp(attr_name, "border") == 0 ||
               strcmp(attr_name, "frame") == 0 ||
               strcmp(attr_name, "rules") == 0 ||
               strcmp(attr_name, "cellspacing") == 0 ||
               strcmp(attr_name, "cellpadding") == 0 ||
               strcmp(attr_name, "align") == 0 ||
               strcmp(attr_name, "bgcolor") == 0;
    }

    if (strcmp(tag_name, "td") == 0 || strcmp(tag_name, "th") == 0) {
        return strcmp(attr_name, "abbr") == 0 ||
               strcmp(attr_name, "axis") == 0 ||
               strcmp(attr_name, "headers") == 0 ||
               strcmp(attr_name, "scope") == 0 ||
               strcmp(attr_name, "rowspan") == 0 ||
               strcmp(attr_name, "colspan") == 0 ||
               strcmp(attr_name, "nowrap") == 0 ||
               strcmp(attr_name, "width") == 0 ||
               strcmp(attr_name, "height") == 0 ||
               strcmp(attr_name, "bgcolor") == 0 ||
               strcmp(attr_name, "align") == 0 ||
               strcmp(attr_name, "valign") == 0;
    }

    if (strcmp(tag_name, "time") == 0) {
        return strcmp(attr_name, "datetime") == 0;
    }

    if (strcmp(tag_name, "tr") == 0) {
        return strcmp(attr_name, "bgcolor") == 0 ||
               strcmp(attr_name, "align") == 0 ||
               strcmp(attr_name, "valign") == 0;
    }

    if (strcmp(tag_name, "ul") == 0) {
        return strcmp(attr_name, "type") == 0;
    }
    if (strcmp(tag_name, "ref") == 0) {
        // Cite extension attributes for <ref>
        return strcmp(attr_name, "name") == 0 ||
               strcmp(attr_name, "group") == 0 ||
               strcmp(attr_name, "follow") == 0 ||
               strcmp(attr_name, "dir") == 0 ||
               strcmp(attr_name, "extends") == 0;
    }

    if (strcmp(tag_name, "references") == 0) {
        // Cite extension attributes for <references>
        return strcmp(attr_name, "group") == 0 ||
               strcmp(attr_name, "responsive") == 0;
    }

    return false;
}

static bool is_valid_attribute_name(const char *attr_name) {
    if (!attr_name || attr_name[0] == '\0') {
        return false;
    }

    // Attribute names must start with a letter
    if (!((attr_name[0] >= 'a' && attr_name[0] <= 'z') ||
          (attr_name[0] >= 'A' && attr_name[0] <= 'Z'))) {
        return false;
    }

    // Check remaining characters
    for (int i = 1; attr_name[i] != '\0'; i++) {
        char c = attr_name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ':')) {
            return false;
        }
    }

    return true;
}

static bool parse_and_validate_attributes(TSLexer *lexer,
                                           const char *tag_name) {
    while (lexer->lookahead && lexer->lookahead != '>' &&
           lexer->lookahead != '/') {
        // Skip whitespace
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
               lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            advance(lexer);
        }

        if (lexer->lookahead == '>' || lexer->lookahead == '/') {
            break;
        }

        // Parse attribute name
        char attr_name[64] = {0};
        int name_len = 0;

        while (name_len < 63 && lexer->lookahead &&
               ((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
                (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z') ||
                (lexer->lookahead >= '0' && lexer->lookahead <= '9') ||
                lexer->lookahead == '-' || lexer->lookahead == '_' ||
                lexer->lookahead == ':')) {

            // Convert to lowercase for comparison
            if (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z') {
                attr_name[name_len] = lexer->lookahead + ('a' - 'A');
            } else {
                attr_name[name_len] = lexer->lookahead;
            }
            name_len++;
            advance(lexer);
        }

        if (name_len == 0) {
            return false; // Invalid attribute name
        }

        attr_name[name_len] = '\0';

        // Validate attribute name
        if (!is_valid_attribute_name(attr_name)) {
            return false;
        }

        // Check if attribute is allowed for this tag
        if (!is_common_attribute(attr_name) &&
            !is_allowed_tag_specific_attribute(tag_name, attr_name)) {
            return false;
        }

        // Skip whitespace after attribute name
        while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
               lexer->lookahead == '\n' || lexer->lookahead == '\r') {
            advance(lexer);
        }

        // Check for attribute value
        if (lexer->lookahead == '=') {
            advance(lexer); // Skip '='

            // Skip whitespace after '='
            while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
                   lexer->lookahead == '\n' || lexer->lookahead == '\r') {
                advance(lexer);
            }

            // Parse attribute value
            if (lexer->lookahead == '"' || lexer->lookahead == '\'') {
                char quote = lexer->lookahead;
                advance(lexer); // Skip opening quote

                // Skip to closing quote
                while (lexer->lookahead && lexer->lookahead != quote) {
                    advance(lexer);
                }

                if (lexer->lookahead == quote) {
                    advance(lexer); // Skip closing quote
                } else {
                    return false; // Unclosed quoted value
                }
            } else {
                // Unquoted value - read until whitespace or special char
                while (lexer->lookahead && lexer->lookahead != ' ' &&
                       lexer->lookahead != '\t' && lexer->lookahead != '\n' &&
                       lexer->lookahead != '\r' && lexer->lookahead != '>' &&
                       lexer->lookahead != '/') {
                    advance(lexer);
                }
            }
        }
    }

    return true;
}

static bool is_valid_html_tag(Scanner *scanner, TSLexer *lexer,
                              bool is_closing) {
    lexer->mark_end(lexer);
    // Skip whitespace (though not typical in HTML)
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t') {
        advance(lexer);
    }
    // Extract tag name
    char tag_name[32] = {0}; // Maximum reasonable tag name length
    int tag_len = 0;

    // Tag name must start with a letter
    if (!((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
          (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z'))) {
        return false;
    }

    // Read tag name (letters, digits, hyphens)
    while (tag_len < 31 && lexer->lookahead &&
           ((lexer->lookahead >= 'a' && lexer->lookahead <= 'z') ||
            (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z') ||
            (lexer->lookahead >= '0' && lexer->lookahead <= '9') ||
            lexer->lookahead == '-')) {

        // Convert to lowercase for comparison
        if (lexer->lookahead >= 'A' && lexer->lookahead <= 'Z') {
            tag_name[tag_len] = lexer->lookahead + ('a' - 'A');
        } else {
            tag_name[tag_len] = lexer->lookahead;
        }
        tag_len++;
        advance(lexer);
    }

    if (tag_len == 0) {
        return false;
    }

    tag_name[tag_len] = '\0';
    // Check if tag is in allowed list
    if (!is_allowed_html_tag(tag_name)) {
        return false;
    }

    if (is_allowed_self_closing_html_tag(tag_name)) {
        // set it in scanner
        scanner->self_closing_html_tag = 1;
    } else {

        scanner->self_closing_html_tag = 0;
    }

    // Skip whitespace
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' ||
           lexer->lookahead == '\n' || lexer->lookahead == '\r') {
        advance(lexer);
    }

    // For opening tags, validate attributes
    if (!is_closing) {
        if (!parse_and_validate_attributes(lexer, tag_name)) {
            return false;
        }
    }

    // Check for self-closing tag
    if (lexer->lookahead == '/') {
        advance(lexer);
    }

    if (lexer->lookahead == '>') {
        return true;
    }

    return false;
}

// Scan a single file/media link option, i.e. the text between two '|'
// separators inside [[File:...|...]].
//
// All six option tokens are valid at the same position, so we classify the
// option in a single pass rather than trying one matcher after another. This
// matters because the lexer cannot rewind: the previous approach advanced the
// lexer while attempting each option and never reset on failure, so a failed
// match corrupted the position for the matchers tried afterwards (e.g. "left"
// consuming the 'l' of "link=", "thumb" matching the prefix of "thumbnail", or
// the digit run of a malformed size being left behind).
//
// The caption is the zero-width fallback: we mark_end at the option start
// before any lookahead, so when nothing else matches the caption token stays
// empty and the grammar parses the caption content itself. On a definite match
// we advance over the option text and mark_end again at its end.
static bool scan_file_option(TSLexer *lexer, const bool *valid_symbols) {
    // Zero-width position for the caption fallback.
    lexer->mark_end(lexer);

    // Size: <digits> [ ('x'|'X') <digits> ] "px"
    if (valid_symbols[FILE_SIZE_TOKEN] && lexer->lookahead >= '0' &&
        lexer->lookahead <= '9') {
        while (lexer->lookahead >= '0' && lexer->lookahead <= '9') {
            advance(lexer);
        }
        if (lexer->lookahead == 'x' || lexer->lookahead == 'X') {
            advance(lexer);
            while (lexer->lookahead >= '0' && lexer->lookahead <= '9') {
                advance(lexer);
            }
        }
        if (lexer->lookahead == 'p') {
            advance(lexer);
            if (lexer->lookahead == 'x') {
                advance(lexer);
                lexer->mark_end(lexer);
                lexer->result_symbol = FILE_SIZE_TOKEN;
                return true;
            }
        }
        // Not a valid size; fall back to treating the option as a caption.
        if (valid_symbols[FILE_CAPTION_TOKEN]) {
            lexer->result_symbol = FILE_CAPTION_TOKEN;
            return true;
        }
        return false;
    }

    // Read a leading run of lowercase letters: an alignment/format keyword, or
    // the name part of "link=" / "alt=".
    char keyword[16] = {0};
    int len = 0;
    while (len < 15 && lexer->lookahead >= 'a' && lexer->lookahead <= 'z') {
        keyword[len++] = (char)lexer->lookahead;
        advance(lexer);
    }
    keyword[len] = '\0';

    if (len > 0) {
        if (lexer->lookahead == '=') {
            if (valid_symbols[FILE_LINK_TOKEN] && strcmp(keyword, "link") == 0) {
                advance(lexer); // '='
                while (lexer->lookahead && lexer->lookahead != '|' &&
                       lexer->lookahead != ']') {
                    advance(lexer);
                }
                lexer->mark_end(lexer);
                lexer->result_symbol = FILE_LINK_TOKEN;
                return true;
            }
            if (valid_symbols[FILE_ALT_TOKEN] && strcmp(keyword, "alt") == 0) {
                advance(lexer); // '='
                while (lexer->lookahead && lexer->lookahead != '|' &&
                       lexer->lookahead != ']') {
                    // Skip a nested wikilink "[[ … ]]" wholesale so its internal
                    // '|' and ']]' do not end the (otherwise plain-text) alt value,
                    // e.g. alt=a [[Resurrection of Jesus|risen]] b.
                    if (lexer->lookahead == '[') {
                        advance(lexer);
                        if (lexer->lookahead == '[') {
                            advance(lexer);
                            while (lexer->lookahead && lexer->lookahead != ']') {
                                advance(lexer);
                            }
                            if (lexer->lookahead == ']') {
                                advance(lexer);
                            }
                            if (lexer->lookahead == ']') {
                                advance(lexer);
                            }
                        }
                        continue;
                    }
                    advance(lexer);
                }
                lexer->mark_end(lexer);
                lexer->result_symbol = FILE_ALT_TOKEN;
                return true;
            }
        }

        if (valid_symbols[FILE_ALIGNMENT_TOKEN] &&
            (strcmp(keyword, "left") == 0 || strcmp(keyword, "right") == 0 ||
             strcmp(keyword, "center") == 0 || strcmp(keyword, "none") == 0)) {
            lexer->mark_end(lexer);
            lexer->result_symbol = FILE_ALIGNMENT_TOKEN;
            return true;
        }

        if (valid_symbols[FILE_FORMAT_TOKEN] &&
            (strcmp(keyword, "thumb") == 0 ||
             strcmp(keyword, "thumbnail") == 0 ||
             strcmp(keyword, "frame") == 0 || strcmp(keyword, "framed") == 0 ||
             strcmp(keyword, "frameless") == 0)) {
            lexer->mark_end(lexer);
            lexer->result_symbol = FILE_FORMAT_TOKEN;
            return true;
        }
    }

    // Anything else is the caption (zero-width marker; the grammar parses the
    // caption content that follows).
    if (valid_symbols[FILE_CAPTION_TOKEN]) {
        lexer->result_symbol = FILE_CAPTION_TOKEN;
        return true;
    }
    return false;
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
            dashes = 0;
            break;
        default:
            dashes = 0;
        }
        advance(lexer);
    }
    return false;
}

// Helper function to check if a character sequence forms a valid HTML entity
static bool is_html_entity(TSLexer *lexer) {
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
    advance(lexer); // Skip the first '{'

    if (lexer->lookahead != '{') {
        return false; // Not a double brace
    }
    return true;
}
// Decide whether the rest of a line forms a MediaWiki header, given that the
// `initial_equals` leading '=' have already been consumed by the caller.
// MediaWiki headers: = Header =, == Header ==, === Header ===, etc.
//
// This advances the lexer past the line body but never calls mark_end, so the
// caller keeps full control of the token boundary. The body scan deliberately
// consumes any characters (including '[', '{', '<') because a heading may
// legitimately contain links/templates (e.g. == [[Link]] ==); soundness of the
// caller relies on it discarding these advances when this returns true.
static bool is_heading_remainder(TSLexer *lexer, int initial_equals) {
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

// Starting at the character after a '[', try to match the URL scheme of an
// external link ("http://" or "https://"). Advances over the matched prefix and
// returns true only on a full match. Every scheme character is also an ordinary
// text character, so on a partial (failed) match the caller can simply fold the
// consumed characters into the surrounding text run.
static bool match_url_scheme(TSLexer *lexer) {
    const char *scheme = "http";
    for (const char *p = scheme; *p; p++) {
        if (lexer->lookahead != *p) {
            return false;
        }
        advance(lexer);
    }
    if (lexer->lookahead == 's') {
        advance(lexer);
    }
    if (lexer->lookahead != ':') {
        return false;
    }
    advance(lexer);
    if (lexer->lookahead != '/') {
        return false;
    }
    advance(lexer);
    return lexer->lookahead == '/';
}

static bool scan_inline_text_base(Scanner *scanner, TSLexer *lexer) {
    int text_run_length = 0;

    lexer->mark_end(lexer);
    while (lexer->lookahead) {
        // Check for special sequences that need matching
        if (lexer->lookahead == '[') {
            // '[' begins a link only as "[[" (wikilink/media link) or
            // "[http(s)://" (external link); MediaWiki treats any other '[' as
            // a literal character (e.g. "[or]" in prose). Advancing to peek is
            // safe: on the markup branch mark_end has not moved past '[', so
            // the over-advance is discarded on return.
            advance(lexer);
            if (lexer->lookahead == '[' || match_url_scheme(lexer)) {
                break;
            }
            text_run_length++;
            lexer->mark_end(lexer);
            continue;
        }
        if (lexer->lookahead == ']') {
            // "]]" can close a wikilink/media link (or a file caption), so end
            // the run there. A lone ']' is a literal character.
            advance(lexer);
            if (lexer->lookahead == ']') {
                break;
            }
            text_run_length++;
            lexer->mark_end(lexer);
            continue;
        }

        if (lexer->lookahead == '{') {
            // Check if this is {{
            if (is_opening_template(lexer)) {
                // Has opening {}, so this should be handled as template
                break;
            }
        }

        if (lexer->lookahead == '}') {
            advance(lexer);
            if (lexer->lookahead == '}') {
                break;
            }
        }

        if (lexer->lookahead == '~') {
            if (is_signature(lexer)) {
                // This is a signature, should be handled separately
                break;
            }
            // is_signature advanced over the whole tilde run while probing and it
            // was not a valid 3/4/5 signature (e.g. a long "~~~~~~" run, or a lone
            // '~'). Fold the consumed tildes into the text run; without this they
            // are left unconsumable when the run ends the line (mirrors the '' and
            // '<'/'[' cases).
            text_run_length++;
            lexer->mark_end(lexer);
            continue;
        }

        if (lexer->lookahead == '&') {
            // Check if this forms an HTML entity
            if (is_html_entity(lexer)) {
                // This is an HTML entity, should be handled separately
                break;
            }
            // is_html_entity advanced over the probed characters and it was not a
            // valid entity (e.g. the "&H" in "B&H"). Fold them into the text run;
            // without this they are left unconsumable when a delimiter follows
            // (e.g. "B&H}}"). Mirrors the ''/'<'/'['/'~' cases.
            text_run_length++;
            lexer->mark_end(lexer);
            continue;
        }

        if (lexer->lookahead == '\'') {
            // Check if this forms bold or italic
            if (is_bold_italic(lexer)) {
                // This is a bold or italic marker, should be handled separately
                break;
            }
            // is_bold_italic advanced over the quote run while probing and it
            // was not a valid marker (a lone apostrophe, e.g. "companion'|" at
            // the end of a template parameter). Fold the consumed quote(s) into
            // the text run; mark_end here so they are emitted rather than left
            // unconsumable.
            text_run_length++;
            lexer->mark_end(lexer);
            continue;
        }

        uint32_t col_index = lexer->get_column(lexer);
        if (lexer->lookahead == '=' && col_index == 0) {
            // Consume the leading '=' run; this is the most we can emit as text
            // if the line is not a heading. mark_end pins the token here so the
            // header check below (which advances past the body to find a
            // matching trailing '=') cannot fold the body into the token.
            int equals = 0;
            while (lexer->lookahead == '=') {
                advance(lexer);
                equals++;
            }
            lexer->mark_end(lexer);
            if (is_heading_remainder(lexer, equals)) {
                // Valid heading: emit no inline text so the grammar lexes the
                // heading markers. Returning false discards the advances above
                // and resets the lexer to the start of the line.
                return false;
            }
            // Not a heading: emit just the '=' run. The body's constructs
            // (links, templates, ...) are recognised on the following scans,
            // with their break checks intact.
            lexer->result_symbol = INLINE_TEXT_BASE;
            return true;
        }
        // Do not allow these chars if they are at beginning of text.
        if ((lexer->lookahead == '*' || lexer->lookahead == '#' ||
             lexer->lookahead == ';' || lexer->lookahead == ':' ||
             lexer->lookahead == '!') &&
            col_index == 0) {
            break;
        }

        if (lexer->lookahead == '<') {
            // A '<' begins markup only when followed by a letter (tag), '/'
            // (closing tag) or '!' (comment) — this mirrors MediaWiki's
            // tokenizer. Any other '<' is a literal character (e.g. the
            // "40:21<4062" in a DOI), so keep it in the text run. We advance to
            // peek the next character; on the markup branch mark_end has not
            // moved past '<', so the over-advance is discarded on return.
            advance(lexer);
            int32_t after = lexer->lookahead;
            if (after == '/' || after == '!' ||
                (after >= 'a' && after <= 'z') ||
                (after >= 'A' && after <= 'Z')) {
                break;
            }
            text_run_length++;
            lexer->mark_end(lexer);
            continue;
        }
        // Tables and pipes should be handled separately
        if (lexer->lookahead == '|' || lexer->lookahead == '\n') {
            break;
        }
        if (col_index == 0) {
            scanner->list_level = 0;
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
// Encode a code point as UTF-8 into out (must hold >= 4 bytes); return length.
static inline unsigned utf8_encode(int32_t c, char *out) {
    if (c < 0x80) {
        out[0] = (char)c;
        return 1;
    }
    if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
        return 2;
    }
    if (c < 0x10000) {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (c >> 18));
    out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
    out[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

// Scan for link opening patterns
static bool is_media_link_token(TSLexer *lexer) {
    // NOTE: At this point '[[' check passed. Accumulate the namespace prefix up
    // to ':' and look it up in the generated MEDIA_NAMESPACES table, which holds
    // every File/Media namespace name and alias across MediaWiki's languages.
    // Normalization here must match scripts/gen-namespaces.py: '_' -> ' ' and an
    // ASCII first letter upper-cased (MediaWiki is first-letter insensitive).
    char buf[128];
    unsigned len = 0;
    while (true) {
        int32_t c = lexer->lookahead;
        if (c == ':') {
            break;
        }
        // A namespace prefix never contains these; bail before scanning further.
        if (c == 0 || c == '\n' || c == '[' || c == ']' || c == '|' ||
            c == '{' || c == '}' || c == '#' || c == '<') {
            return false;
        }
        if (c == '_') {
            c = ' ';
        }
        if (len + 4 > sizeof(buf)) {
            return false;
        }
        len += utf8_encode(c, buf + len);
        advance(lexer);
    }
    if (len == 0) {
        return false;
    }
    buf[len] = '\0';
    if (buf[0] >= 'a' && buf[0] <= 'z') {
        buf[0] -= 'a' - 'A';
    }

    unsigned lo = 0, hi = MEDIA_NAMESPACES_COUNT;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        int cmp = strcmp(buf, MEDIA_NAMESPACES[mid]);
        if (cmp == 0) {
            advance(lexer); // Skip ':'
            return true;
        }
        if (cmp < 0) {
            hi = mid;
        } else {
            lo = mid + 1;
        }
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
static bool is_template_param_name_value_pair(TSLexer *lexer) {
    while (lexer->lookahead) {
        if (lexer->lookahead == '=') {
            return true;
        }
        if (lexer->lookahead == '|' || lexer->lookahead == '[' ||
            lexer->lookahead == '{' || lexer->lookahead == '}' ||
            lexer->lookahead == '\n') {
            // '}' closes the template, so an '=' beyond it belongs to an outer
            // scope; without this a positional argument such as the "2" in
            // "{{chem|O|2}}" is misread as a parameter name when an '=' appears
            // later in the surrounding text.
            break;
        }
        advance(lexer);
    }
    return false;
}

// A table cell may carry HTML attributes before its content, separated by a single
// `|`, e.g. `! style="text-align:right;"|Total`. Looking ahead from the start of a
// cell's content, report whether such an attribute run is present: an `=` (the
// attribute syntax) appears before a single `|` separator, and we do not first hit
// a newline (end of cell), a template/link (`{`/`[`, whose `|` is internal), or a
// `||`/`!!` (the inline cell/header separator — the cell has no attributes).
static bool has_cell_attributes(TSLexer *lexer) {
    bool seen_equals = false;
    while (lexer->lookahead) {
        int32_t c = lexer->lookahead;
        if (c == '\n' || c == '{' || c == '[' || c == '}') {
            return false;
        }
        if (c == '=') {
            seen_equals = true;
        }
        if (c == '|') {
            advance(lexer);
            if (lexer->lookahead == '|') {
                return false; // `||` is the inline cell separator, not an attr sep
            }
            return seen_equals;
        }
        if (c == '!') {
            advance(lexer);
            if (lexer->lookahead == '!') {
                return false; // `!!` is the inline header separator
            }
            continue;
        }
        advance(lexer);
    }
    return false;
}

// Debug utility function to print valid symbols (currently unused)
// Kept for debugging purposes but not called in production code
// Uncomment the call in tree_sitter_wikitext_external_scanner_scan to use
#ifdef DEBUG_SCANNER
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
    if (valid_symbols[TEMPLATE_PARAM_VALUE_MARKER]) {
        printf("TEMPLATE_PARAM_VALUE_MARKER");
    }
    if (valid_symbols[TEMPLATE_PARAM_NAME_VALUE_MARKER]) {
        printf("TEMPLATE_PARAM_VALUE_MARKER");
    }
    if (valid_symbols[HTML_TAG_OPEN_MARKER]) {
        printf("HTML_CONTENT_MARKER");
    }
    if (valid_symbols[HTML_SELF_CLOSING_TAG_MARKER]) {
        printf("HTML_SELF_CLOSING_TAG_MARKER");
    }
    if (valid_symbols[ORDERED_LIST_MARKER]) {
        printf("ORDERED_LIST_MARKER");
    }
    if (valid_symbols[UNORDERED_LIST_MARKER]) {
        printf("UNORDERED_LIST_MARKER");
    }
    printf("\n");
}
#endif

bool tree_sitter_wikitext_external_scanner_scan(void *payload, TSLexer *lexer,
                                                const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
    /* dump_valid_symbols(valid_symbols); */
    // Handle file/media link options in a single pass (see scan_file_option).
    if ((valid_symbols[FILE_SIZE_TOKEN] || valid_symbols[FILE_ALIGNMENT_TOKEN] ||
         valid_symbols[FILE_FORMAT_TOKEN] || valid_symbols[FILE_LINK_TOKEN] ||
         valid_symbols[FILE_ALT_TOKEN] || valid_symbols[FILE_CAPTION_TOKEN]) &&
        scan_file_option(lexer, valid_symbols)) {
        return true;
    }
    if (valid_symbols[TEMPLATE_PARAM_VALUE_MARKER] &&
        valid_symbols[TEMPLATE_PARAM_NAME_VALUE_MARKER]) {

        lexer->mark_end(lexer);
        if (is_template_param_name_value_pair(lexer)) {
            lexer->result_symbol = TEMPLATE_PARAM_NAME_VALUE_MARKER;
            return true;
        } else {
            lexer->result_symbol = TEMPLATE_PARAM_VALUE_MARKER;
            return true;
        }
    }
    // At the start of a table cell's content, emit a zero-width marker saying
    // whether an HTML attribute run (`name=value … |`) precedes the content. Always
    // emitting one of the two markers (never falling through) keeps the lexer
    // position clean despite has_cell_attributes advancing to look ahead, and
    // commits the parser to a branch before the greedy inline-text token can swallow
    // the attributes as content. Mirrors the template-param marker pair.
    if (valid_symbols[TABLE_CELL_ATTRIBUTE_MARKER] &&
        valid_symbols[TABLE_CELL_PLAIN_MARKER]) {
        lexer->mark_end(lexer);
        if (has_cell_attributes(lexer)) {
            lexer->result_symbol = TABLE_CELL_ATTRIBUTE_MARKER;
        } else {
            lexer->result_symbol = TABLE_CELL_PLAIN_MARKER;
        }
        return true;
    }

    if (valid_symbols[HTML_TAG_OPEN_MARKER] ||
        valid_symbols[HTML_SELF_CLOSING_TAG_MARKER]) {
        reset_state(scanner);
        if (is_valid_html_tag(scanner, lexer, false)) {
            if (scanner->self_closing_html_tag == 1) {
                lexer->result_symbol = HTML_SELF_CLOSING_TAG_MARKER;
            } else {
                lexer->result_symbol = HTML_TAG_OPEN_MARKER;
            }
            reset_state(scanner);
            return true;
        }
    }
    if (valid_symbols[HTML_TAG_CLOSE_MARKER] &&
        is_valid_html_tag(scanner, lexer, true)) {
        lexer->result_symbol = HTML_TAG_CLOSE_MARKER;
        return true;
    }

    switch (lexer->lookahead) {
    case '[':
        if (valid_symbols[MEDIA_LINK_TOKEN] || valid_symbols[WIKI_LINK_TOKEN]) {
            // Zero-width link-opening marker (mark_end pins it at the '['). The
            // probes below advance only to look ahead.
            lexer->mark_end(lexer);
            if (valid_symbols[WIKI_LINK_TOKEN] &&
                is_wiki_link_open_token(lexer)) {
                lexer->result_symbol = WIKI_LINK_TOKEN;
                advance(lexer); // consume the second '[' before the media probe
                if (valid_symbols[MEDIA_LINK_TOKEN] &&
                    is_media_link_token(lexer)) {
                    lexer->result_symbol = MEDIA_LINK_TOKEN;
                }
                return true;
            }
            // Not "[[": is_wiki_link_open_token consumed the single '['. If an
            // external-link scheme follows ("[http(s)://"), decline so the grammar
            // lexes the external_link (the advances here are discarded on return).
            if (match_url_scheme(lexer)) {
                return false;
            }
        }
        // A literal '[' (not "[[" or "[http://", e.g. the "[a b] c" in a citation
        // quote): the '[' is an ordinary character. Consume the run as inline text
        // rather than leave it to error. The token spans from the '[' (the scan
        // start) regardless of how far the probes above advanced.
        if (valid_symbols[INLINE_TEXT_BASE] &&
            scan_inline_text_base(scanner, lexer)) {
            lexer->result_symbol = INLINE_TEXT_BASE;
            return true;
        }
        break;
    case '*':
    case '#':
        if ((valid_symbols[UNORDERED_LIST_MARKER] ||
             valid_symbols[ORDERED_LIST_MARKER]) &&
            lexer->get_column(lexer) == 0) {
            int level = 0;
            while (lexer->lookahead == '*' || lexer->lookahead == '#') {
                if (lexer->lookahead == '*') {
                    lexer->result_symbol = UNORDERED_LIST_MARKER;
                }
                if (lexer->lookahead == '#') {
                    lexer->result_symbol = ORDERED_LIST_MARKER;
                }
                level++;
                advance(lexer);
                lexer->mark_end(lexer);
            }
            scanner->list_level = level;
            return true;
        }
        // A '*'/'#' that is not a list marker here — not at column 0, or lists are
        // not valid in this context (e.g. a literal '*' inside <sup>...</sup>) — is
        // ordinary inline text. Without this fallback the character is left
        // unconsumed and the parse errors.
        if (valid_symbols[INLINE_TEXT_BASE] &&
            scan_inline_text_base(scanner, lexer)) {
            return true;
        }
        break;
    case '<':
        if (valid_symbols[COMMENT]) {
            if (scan_comment(lexer)) {
                lexer->result_symbol = COMMENT;
                return true;
            }
            // scan_comment consumed the leading '<' before failing on input
            // that is not "<!--", so the lexer now sits on the character after
            // it. Fall through to classify that character.
        } else if (valid_symbols[INLINE_TEXT_BASE]) {
            advance(lexer); // consume the leading '<' ourselves
        } else {
            break;
        }
        // A letter or '/' after '<' is a (possibly invalid) tag the grammar
        // must handle, so decline. Otherwise the '<' is a literal character (a
        // stray '<' starting a text run, e.g. a DOI value); emit it — and any
        // text that follows — as inline text so parsing does not stall.
        if (valid_symbols[INLINE_TEXT_BASE]) {
            int32_t after = lexer->lookahead;
            if (after != '/' && !(after >= 'a' && after <= 'z') &&
                !(after >= 'A' && after <= 'Z')) {
                lexer->mark_end(lexer);
                scan_inline_text_base(scanner, lexer);
                lexer->result_symbol = INLINE_TEXT_BASE;
                return true;
            }
        }
        break;
    default:
        if (valid_symbols[INLINE_TEXT_BASE] &&
            scan_inline_text_base(scanner, lexer)) {
            return true;
        };
    }
    //  dump_valid_symbols(valid_symbols);
    return false;
}
