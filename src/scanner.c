
#include "tree_sitter/alloc.h"
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
    TEMPLATE_PARAM_VALUE_MARKER,
    TEMPLATE_PARAM_NAME_VALUE_MARKER,
    HTML_TAG_OPEN_MARKER,
    HTML_TAG_CLOSE_MARKER,
    HTML_SELF_CLOSING_TAG_MARKER,
    UNORDERED_LIST_MARKER,
    ORDERED_LIST_MARKER,
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
        "noinclude",
        // References
        "ref", NULL};

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
        return strcmp(attr_name, "name") == 0;
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

        // For link and mmeeta tags, validate required attributes
        if (strcmp(tag_name, "link") == 0) {
            // link tag must have itemprop and href
            static bool has_itemprop = false, has_href = false;
            if (strcmp(attr_name, "itemprop") == 0)
                has_itemprop = true;
            if (strcmp(attr_name, "href") == 0)
                has_href = true;
            // Note: This is a simplified check - in practice you'd need to
            // track all attributes throughout the parsing
        }

        if (strcmp(tag_name, "meta") == 0) {
            // meta tag must have itemprop and content
            static bool has_itemprop = false, has_content = false;
            if (strcmp(attr_name, "itemprop") == 0)
                has_itemprop = true;
            if (strcmp(attr_name, "content") == 0)
                has_content = true;
            // Note: This is a simplified check - in practice you'd need to
            // track all attributes throughout the parsing
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

static bool scan_inline_text_base(Scanner *scanner, TSLexer *lexer) {
    int text_run_length = 0;

    lexer->mark_end(lexer);
    while (lexer->lookahead) {
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
            advance(lexer);
            if (lexer->lookahead == '}') {
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

        uint32_t col_index = lexer->get_column(lexer);
        if (lexer->lookahead == '=' && col_index == 0) {
            // Check if this forms a MediaWiki header
            if (is_mediawiki_header(lexer)) {
                // This is a MediaWiki header, should be handled separately
                break;
            }
        }
        // Do not allow these chars if they are at beginning of text.
        if ((lexer->lookahead == '*' || lexer->lookahead == '#' ||
             lexer->lookahead == ';' || lexer->lookahead == ':') &&
            col_index == 0) {
            break;
        }
        /* if (lexer->lookahead == ':' && char_index > 0) {
             break;
        }
        */

        if (lexer->lookahead == '<') {
            /* if (consume_string("<!--", lexer)) { */
            /*     break; */
            /* } */
            break;
        }
        // FIXME:This should be properly handled - tables and html
        if (lexer->lookahead == '|' || lexer->lookahead == '\n') {
            // should be handled separately
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
static bool is_template_param_name_value_pair(TSLexer *lexer) {
    while (lexer->lookahead) {
        if (lexer->lookahead == '=') {
            return true;
        }
        if (lexer->lookahead == '|' || lexer->lookahead == '[' ||
            lexer->lookahead == '{' || lexer->lookahead == '\n') {
            break;
        }
        advance(lexer);
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

bool tree_sitter_wikitext_external_scanner_scan(void *payload, TSLexer *lexer,
                                                const bool *valid_symbols) {
    Scanner *scanner = (Scanner *)payload;
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
            bool found_link = false;
            // This is zero width token.
            lexer->mark_end(lexer);
            if (valid_symbols[WIKI_LINK_TOKEN] &&
                is_wiki_link_open_token(lexer)) {
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
    case '*':
    case '#':
        if (valid_symbols[UNORDERED_LIST_MARKER] ||
            valid_symbols[ORDERED_LIST_MARKER]) {
            int level = 0;
            uint32_t col_index = lexer->get_column(lexer);
            if (col_index > 0) {
                return false;
            }
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
            if (scanner->list_level <= level) {
                scanner->list_level = level;
            } else {
                return true;
            }
            return true;
        }
        break;
    case '<':
        if (valid_symbols[COMMENT] && scan_comment(lexer)) {
            lexer->result_symbol = COMMENT;
            return true;
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
