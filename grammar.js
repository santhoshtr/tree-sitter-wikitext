/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

const HTML_TAGS_VOID = [
  "area",
  "base",
  "br",
  "col",
  "embed",
  "hr",
  "img",
  "input",
  "keygen",
  "link",
  "meta",
  "param",
  "source",
  "track",
  "wbr",
  "nowiki", // Treat <nowiki /> as void
];

const HTML_TAGS_BLOCK = [
  "address",
  "article",
  "aside",
  "blockquote",
  "canvas",
  "dd",
  "div",
  "dl",
  "dt",
  "fieldset",
  "figcaption",
  "figure",
  "footer",
  "form",
  "h1",
  "h2",
  "h3",
  "h4",
  "h5",
  "h6",
  "header",
  "hgroup",
  "hr",
  "li",
  "main",
  "nav",
  "noscript",
  "ol",
  "output",
  "p",
  "pre",
  "section",
  "table",
  "tfoot",
  "ul",
  "video",
  // MediaWiki specific common block tags
  "categorytree",
  "gallery",
  "imagemap",
  "includeonly",
  "indicator",
  "inputbox",
  "noinclude",
  "onlyinclude",
  "poem",
  "references",
  "syntaxhighlight",
  "source",
  "timeline",
];

const HTML_TAGS_INLINE = [
  "a",
  "abbr",
  "acronym",
  "b",
  "bdo",
  "big",
  "br",
  "button",
  "cite",
  "code",
  "del",
  "dfn",
  "em",
  "i",
  "img",
  "input",
  "ins",
  "kbd",
  "label",
  "map",
  "object",
  "q",
  "ruby",
  "s",
  "samp",
  "small",
  "span",
  "strike",
  "strong",
  "sub",
  "sup",
  "textarea",
  "time",
  "tt",
  "u",
  "var",
  // MediaWiki specific common inline tags
  "charinsert",
  "math",
  "ref",
  "score",
];

// Combine and make unique, preferring block if duplicated
const KNOWN_HTML_TAG_NAMES = [
  ...new Set([...HTML_TAGS_BLOCK, ...HTML_TAGS_INLINE, ...HTML_TAGS_VOID]),
];

// Helper for text that should not greedily consume terminators
const text_not_ending_with = (terminators) => {
  // Construct a regex that matches any character NOT in terminators,
  // or any character in terminators NOT followed by another character in terminators (if applicable)
  // This is a simplification; complex cases might need more sophisticated lookaheads.
  // For now, a simple negated character class is often sufficient if terminators are single characters.
  const pattern = `[^${terminators.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}\\n]+`;
  return token(pattern);
};

module.exports = grammar({
  name: "wikitext",
  externals: ($) => [
    $.comment,
    $._inline_text_base,
    $._wiki_link_token,
    $._media_link_token,
    $.file_size_token,
    $.file_alignment_token,
    $.file_format_token,
    $.file_link_token,
    $.file_alt_token,
    $._file_caption_token,
  ],
  extras: (_) => ["\r", /\s/],
  conflicts: ($) => [
    [$.paragraph, $._html_content],
    [$._block_level_element, $._html_content],
    [$.nowiki_tag_block, $.nowiki_inline_element],
  ],
  precedences: ($) => [
    // Precedence for ''''' (bold italic) vs ''' (bold) and '' (italic)
    ["bold_italic_explicit", $.bold, $.italic],
    [$._block_level_element, $.html_tag],
    [$._inline_content, $.html_tag],
  ],
  rules: {
    source_file: ($) => repeat($._block_level_element),
    _block_level_element: ($) =>
      choice(
        $.heading,
        $.horizontal_rule,
        $.list,
        $.preformatted_block,
        $.table,
        $.html_block_tag, // HTML known to be block-level
        $.html_tag, // Generic HTML tag (could be block or inline, parsed as block here)
        $.paragraph, // Must be low precedence
        $._blank_line, // Consumes blank lines separating blocks
      ),
    _blank_line: ($) => /\n\s*\n/,

    // ==== Text and Inline Content ====
    _inline_content: ($) =>
      choice(
        $.comment,
        $.bold_italic, // Must come before bold and italic
        $.bold,
        $.italic,
        $.wikilink,
        $.medialink,
        $.external_link,
        $.template,
        $.magic_word,
        $.signature,
        $.redirect,
        $.html_inline_tag, // HTML known to be inline
        $.html_void_tag, // E.g. <br />, <hr />
        $.entity,
        $.nowiki_inline_element, // <nowiki>content</nowiki> or <nowiki />
        // Everything else
        $.text,
      ),
    // Text is a sequence of non-special characters or characters that don't form other tokens
    // This is a fallback and should have low precedence
    text: ($) => prec(-2, $._inline_text_base),

    // More specific text tokens for contexts where fewer delimiters apply
    _text_no_brackets_pipes: ($) => text_not_ending_with("\\[\\]|{}"),
    _text_no_pipes_braces: ($) => text_not_ending_with("|{}"),
    _text_no_square_brackets: ($) => text_not_ending_with("\\[\\]"),
    _text_no_curly_braces: ($) => text_not_ending_with("{}"),
    _text_no_equals: ($) => text_not_ending_with("="),
    _text_no_bar: ($) => text_not_ending_with("|"),
    _text_no_newline: ($) => token(/[^\n]+/),

    single_quote_text: ($) =>
      token(
        "'", // Single quote
      ),
    // REDIRECT

    redirect: ($) =>
      seq(
        choice("#REDIRECT", "#redirect", "#Redirect"),
        optional(/\s+/),
        $.wikilink,
      ),

    // ==== Horizontal Rule ====
    horizontal_rule: ($) =>
      token(
        prec(
          4,
          // Match 4 or more hyphens
          // Followed by zero or more spaces or tabs (horizontal whitespace)
          // Followed by a newline sequence (CRLF, LF, or CR)
          /----[ \t]*(\r\n|\n|\r)/,
        ),
      ),
    // ==== Headings ====
    heading: ($) =>
      choice(
        $.heading1,
        $.heading2,
        $.heading3,
        $.heading4,
        $.heading5,
        $.heading6,
      ),
    _heading_content: ($) =>
      repeat1(
        choice(
          alias($._text_no_equals_newline, $.text), // Text that isn't an equals sign or newline
          $.wikilink,
          $.external_link,
          $.template,
          $.bold,
          $.italic,
          $.bold_italic,
          $.html_inline_tag,
          $.html_void_tag,
          $.nowiki_inline_element,
          // Cannot contain other headings or block elements easily
        ),
      ),
    heading1: ($) =>
      seq(
        alias(/={1}/, $.heading_marker),
        field("title", $._heading_content),
        alias(/={1}/, $.heading_marker),
        /\s*\n/,
      ),
    heading2: ($) =>
      seq(
        alias(/={2}/, $.heading_marker),
        field("title", $._heading_content),
        alias(/={2}/, $.heading_marker),
        /\s*\n/,
      ),
    heading3: ($) =>
      seq(
        alias(/={3}/, $.heading_marker),
        field("title", $._heading_content),
        alias(/={3}/, $.heading_marker),
        /\s*\n/,
      ),
    heading4: ($) =>
      seq(
        alias(/={4}/, $.heading_marker),
        field("title", $._heading_content),
        alias(/={4}/, $.heading_marker),
        /\s*\n/,
      ),
    heading5: ($) =>
      seq(
        alias(/={5}/, $.heading_marker),
        field("title", $._heading_content),
        alias(/={5}/, $.heading_marker),
        /\s*\n/,
      ),
    heading6: ($) =>
      seq(
        alias(/={6}/, $.heading_marker),
        field("title", $._heading_content),
        alias(/={6}/, $.heading_marker),
        optional(/\s*\n/),
      ),
    _text_no_equals_newline: ($) => token(prec(1, /[^=\n]+/)),
    // ==== Paragraphs ====
    // A paragraph is a sequence of inline content, not starting with a block marker,
    // and terminated by a blank line or another block element.
    // This is tricky. It often has low precedence.
    paragraph: ($) =>
      prec(-4, seq(repeat1($._inline_content), choice($._newline, $._eof))),
    _newline: ($) => "\n",
    _eof: ($) => token(prec(-10, "\0")), // special token for end of file, defined by tree-sitter

    // ==== Links ====
    wikilink: ($) =>
      seq(
        $._wiki_link_token,
        "[[",
        field("target", $.wikilink_page),
        optional(seq("|", field("display", $._wikilink_display_content))),
        "]]",
      ),
    wikilink_page: ($) =>
      repeat1(
        choice(
          alias($._text_no_brackets_pipes_colon_hash, $.page_name_segment), // Colon for namespace, hash for section
          $.template, // Templates can be in link targets/pages
          alias(token.immediate(prec(1, ":")), $.namespace_separator), // for interwiki/category
          alias(token.immediate(prec(1, "#")), $.section_separator),
        ),
      ),
    _text_no_brackets_pipes_colon_hash: ($) =>
      token(prec(1, /[^\[\]\|\{\}:#\n][^\[\]\|\{\}\n]*/)),

    _text_no_brackets: ($) => token(prec(1, /[^\[\]\{\}]+/)),
    _wikilink_display_content: ($) =>
      choice(
        alias($._text_no_brackets, $.page_name_segment), // Colon for namespace, hash for section
        $.template,
        $.bold,
        $.italic,
        $.bold_italic,
        $.html_inline_tag,
        $.html_void_tag,
        $.nowiki_inline_element,
        // No nested links easily in display text without complex escape logic
      ),

    medialink: ($) =>
      seq(
        $._media_link_token,
        "[[",
        field("filename", $.filename),
        repeat(seq("|", $._file_option)),
        "]]",
      ),

    filename: ($) => token(prec(1, /[^\|\[\]]+/)),

    _file_option: ($) =>
      choice(
        $.file_size_token,
        $.file_alignment_token,
        $.file_format_token,
        $.file_link_token,
        $.file_alt_token,
        seq($._file_caption_token, $.file_caption),
      ),

    file_caption: ($) => repeat1(choice($.text, $.wikilink, $._newline)),
    external_link: ($) =>
      choice(
        seq(
          // Unnamed external link
          "[",
          field("url", $.url),
          "]",
        ),
        seq(
          // Named external link
          "[",
          field("url", $.url),
          /\s+/, // Must have space
          field("display", repeat1($._external_link_display_content)),
          "]",
        ),
        field("url_bare", $.url_bare), // Bare URLs are also links
      ),
    url: ($) => token(/https?:\/\/[^ \]\n]+/), // Basic URL regex
    url_bare: ($) => token(prec(-1, /https?:\/\/[^\s\[\]\{\}<>'"]+/)), // Bare URLs should not be inside other constructs
    _external_link_display_content: ($) =>
      choice(
        alias($._text_no_brackets, $.page_name_segment), // Colon for namespace, hash for section
        $.template,
        $.bold,
        $.italic,
        $.bold_italic,
        $.html_inline_tag,
        $.html_void_tag,
        $.nowiki_inline_element,
      ),

    // ==== Preformatted Text & Nowiki ====
    preformatted_block: ($) =>
      choice($.pre_tag_block, $.nowiki_tag_block, $.preformatted_line_block),
    pre_tag_block: ($) =>
      seq(
        alias($._pre_open_tag, $.html_tag_open),
        field("content", optional(alias($._pre_content, $.raw_text))),
        alias($._pre_close_tag, $.html_tag_close),
      ),
    _pre_open_tag: ($) =>
      seq(
        "<",
        token.immediate("pre"),
        optional($._html_attributes_pattern_no_gt),
        ">",
      ),
    _pre_close_tag: ($) => token(seq("</", token.immediate("pre"), ">")),
    _pre_content: ($) =>
      token(
        prec.right(/(?:[^<]|<[^/]|<\/[^p]|<\/p[^r]|<\/pr[^e]|<\/pre[^>])+/),
      ),
    nowiki_tag_block: ($) =>
      seq(
        alias($._nowiki_open_tag, $.html_tag_open),
        field("content", optional(alias($._nowiki_content, $.raw_text))),
        alias($._nowiki_close_tag, $.html_tag_close),
      ),
    _nowiki_open_tag: ($) =>
      seq(
        "<",
        token.immediate("nowiki"),
        optional($._html_attributes_pattern_no_gt),
        ">",
      ),
    _nowiki_close_tag: ($) => token(seq("</", token.immediate("nowiki"), ">")),
    _nowiki_content: ($) =>
      token(
        prec.right(
          /(?:[^<]|<[^/]|<\/[^n]|<\/n[^o]|<\/no[^w]|<\/now[^i]|<\/nowi[^k]|<\/nowik[^i]|<\/nowiki[^>])+/,
        ),
      ),
    nowiki_inline_element: ($) =>
      choice(
        seq(
          // <nowiki>text</nowiki>
          alias($._nowiki_open_tag, $.html_tag_open),
          field("content", optional(alias($._nowiki_content, $.raw_text))),
          alias($._nowiki_close_tag, $.html_tag_close),
        ),
        alias($._nowiki_void_tag, $.html_void_tag), // <nowiki />
      ),
    _nowiki_void_tag: ($) =>
      token(seq("<", token.immediate("nowiki"), /\s*\/>/)),

    preformatted_line_block: ($) =>
      prec.left(
        1, // Higher precedence than paragraph
        repeat1($.preformatted_line),
      ),
    preformatted_line: ($) =>
      seq(
        token.immediate(" "), // Must start with a space at the beginning of a line (handled by parser logic via `extras`)
        field("content", $._text_no_newline), // Consume rest of the line
        $._newline,
      ),

    // ==== Templates ====
    template: ($) =>
      seq("{{", $.template_name, repeat($.template_argument), "}}"),

    template_name: ($) =>
      seq(
        // Text for template name, avoid | and }}. But space is allowed.
        // Can also contain other templates if they are part of the name (complex case)
        alias($._text_no_pipes_braces_colon_hash_equals, $.template_name_part), // Colon for magic words like {{PAGENAMEE}}, hash for parser functions, equals for parameter default
        optional(alias(token.immediate(prec(1, ":")), $.template_name_colon)),
        optional(alias(token.immediate(prec(1, "#")), $.template_name_hash)),
      ),
    _text_no_pipes_braces_colon_hash_equals: ($) =>
      token(prec(1, /[^\|{}:#=]+/)), // Allow spaces in name. But not in the beginning.

    template_argument: ($) =>
      seq(
        "|",
        optional($.template_param_value),
        optional(seq("=", $.template_param_value)),
      ),
    template_param_name: ($) => $._text_no_pipes_braces_equals,

    _text_no_pipes_braces_equals: ($) => token(prec(1, /[^\|{}=]+/)),

    template_param_value: ($) =>
      repeat1(
        choice(
          // Text for param value, avoid | and }}
          // This is where most inline content can appear within a template
          $.wikilink,
          $.external_link,
          $.template, // Nested template in param value
          $.bold,
          $.italic,
          $.bold_italic,
          $.html_tag, // HTML tags can be in param values
          $.magic_word,
          $.signature,
          $.nowiki_inline_element,
          $.text,
          // token(prec(1, /[^=\[\]{}\|]+/)),
          // General text content within the parameter value.
          // This token will match sequences of characters that are NOT |, {, }, or newline.
          // It has lower precedence to allow specific tokens/rules to match first.
          //alias($._param_value_text_content, $.text),

          // If a single |, {, or } is encountered that wasn't part of the above
          // constructs (e.g., not `{{..}}`, not the `|` that separates arguments for $.template_argument),
          // then parse it as a literal character.
          // These have slightly higher precedence than the general text to be picked
          // when a single one of these characters is found.
          alias(token(prec(0, "|")), $.pipe_literal), // Literal pipe character within a value
          alias(token(prec(0, "{")), $.lbrace_literal), // Literal open brace within a value
          alias(token(prec(0, "}")), $.rbrace_literal), // Literal close brace within a value
        ),
      ),
    // ==== Lists ====
    list: ($) =>
      prec.left(
        repeat1(
          choice(
            $.unordered_list_item,
            $.ordered_list_item,
            $.definition_term,
            $.definition_data,
          ),
        ),
      ),

    _list_item_prefix_unordered: ($) => /\*+/,
    _list_item_prefix_ordered: ($) => /#+/,
    _list_item_prefix_definition_term: ($) => /;+/,
    _list_item_prefix_definition_data: ($) => /:+/,

    // Content of a list item can be complex, including nested lists or paragraphs
    list_item_content: ($) =>
      prec.left(
        repeat1(
          choice(
            $._inline_content, // Inline content directly
            $.list, // Nested list
            // A paragraph node could also be here if list items can span multiple lines forming paragraphs
          ),
        ),
      ),

    unordered_list_item: ($) =>
      prec.left(
        seq(
          alias($._list_item_prefix_unordered, $.list_marker),
          field("content", $.list_item_content),
          optional($._newline), // Items are typically one per line
        ),
      ),
    ordered_list_item: ($) =>
      prec.left(
        seq(
          alias($._list_item_prefix_ordered, $.list_marker),
          field("content", $.list_item_content),
          optional($._newline),
        ),
      ),
    definition_term: ($) =>
      prec.left(
        seq(
          alias($._list_item_prefix_definition_term, $.list_marker),
          field("term", $.list_item_content),
          optional(
            seq(
              alias($._list_item_prefix_definition_data, $.list_marker), // term can be followed by data on same line
              field("data", $.list_item_content),
            ),
          ),
          optional($._newline),
        ),
      ),
    definition_data: ($) =>
      prec.left(
        seq(
          alias($._list_item_prefix_definition_data, $.list_marker),
          field("data", $.list_item_content),
          optional($._newline),
        ),
      ),

    // ==== Formatting ====
    bold_italic: ($) =>
      prec.left(
        "bold_italic_explicit",
        seq("'''''", field("content", repeat($._inline_content)), "'''''"),
      ),
    bold: ($) =>
      prec.left(
        1,
        seq(
          // dynamic precedence to help with nesting
          "'''",
          field("content", repeat($._inline_content)),
          "'''",
        ),
      ),
    italic: ($) =>
      prec.left(
        0,
        seq(
          // dynamic precedence
          "''",
          field("content", repeat($._inline_content)),
          "''",
        ),
      ),

    // Nowiki
    nowiki: ($) => seq("<nowiki>", /[^<]+/, "</nowiki>"),

    // ==== Tables ====
    // This is a simplified table grammar. Real MediaWiki tables are very complex.
    table: ($) =>
      seq(
        "{|",
        optional(repeat1($.table_attribute)),
        "\n",
        optional($.tablecaption),
        optional(alias(repeat1($.table_header), $.colheaders)),
        optional(repeat1($.table_cell)),
        repeat($.table_row),
        "|}",
      ),
    table_attribute: ($) =>
      seq(
        field("name", $.html_attribute_name),
        "=",
        field("value", $.html_attribute_value),
      ),

    // This token matches any sequence of characters that are NOT the primary
    // table structure delimiters (\n, |, !, }).
    // It serves as a fallback for non-HTML attributes within the table declaration.
    _table_attribute_text: ($) => token(prec(-1, /[^\n\|!}]+/)),
    tablecaption: ($) => seq("|+", alias($._table_node, $.content), "\n"),

    table_header_block: ($) =>
      seq(
        "!",
        // optional(seq(repeat1($.table_attribute), "|")),
        alias($._table_node, $.content),
        optional($._newline),
      ),
    table_header_inline: ($) =>
      seq(
        "!!",
        // optional(seq(repeat1($.table_attribute), "|")),
        alias($._table_node, $.content),
      ),

    table_header: ($) =>
      choice(
        $.table_header_inline, // !! separated
        $.table_header_block, // ! separated
      ),

    table_cell_block: ($) =>
      seq(
        "|",
        // optional(seq(repeat1($.table_attribute), "|")),
        alias($._table_node, $.content),
        optional($._newline),
      ),
    table_cell_inline: ($) =>
      seq(
        "||",
        // optional(seq(repeat1($.table_attribute), "|")),
        alias($._table_node, $.content),
      ),

    table_cell: ($) =>
      choice(
        $.table_cell_inline, // || separated
        $.table_cell_block, // | separated
      ),
    _table_node: ($) => repeat1(choice($._inline_content)),
    table_row: ($) =>
      seq(
        "|-",
        optional(repeat1($.table_attribute)),
        "\n",
        optional(repeat1($.table_header)),
        repeat($.table_cell),
      ),
    // -----------------------------------------------------------------------------------------
    // ------------------------------------------------------------------------------ SIGNATURES
    // -----------------------------------------------------------------------------------------

    signature: ($) =>
      choice(
        alias("~~~", $.user_signature),
        alias("~~~~", $.user_signature_with_date),
        alias("~~~~~", $.current_date),
      ),
    // Magic words
    magic_word: ($) => token(/__([A-Z_]+)__/),
    // ==== HTML ====
    // Basic HTML parsing. This is not a full HTML parser.
    html_tag_name: ($) => $._word_chars,
    _word_chars: ($) => token(/[a-zA-Z_][a-zA-Z0-9_]*/),

    html_attribute_name: ($) => token(/[a-zA-Z_:][-a-zA-Z0-9_:.]*/),
    html_attribute_value: ($) =>
      choice(
        seq('"', field("value_text", optional(token(prec(1, /[^"]*/)))), '"'),
        seq("'", field("value_text", optional(token(prec(1, /[^']*/)))), "'"),
        field("value_text", token(prec(1, /[^\s"'=<>`]+/))), // Unquoted
      ),
    html_attribute: ($) =>
      seq(
        field("name", $.html_attribute_name),
        optional(seq("=", field("value", $.html_attribute_value))),
      ),
    _html_attributes_pattern_no_gt: ($) =>
      repeat1(choice($.html_attribute, /\s+/)),

    _html_content: ($) =>
      choice(
        $.html_tag,
        $._inline_content, // Allows MediaWiki markup inside HTML tags (often the case)
        $._block_level_element, // Allows block MediaWiki markup inside HTML block tags
        alias($._html_text, $.text),
      ),

    _html_text: ($) => token(prec(-1, /[^<]+/)),

    html_tag: ($) =>
      prec.dynamic(
        0,
        choice(
          // Generic HTML tag, try to match specific types first
          $.html_block_tag,
          $.html_inline_tag,
          $.html_void_tag,
          $._html_generic_tag, // Fallback
        ),
      ),

    _html_generic_tag: ($) =>
      seq(
        "<",
        field("name", $.html_tag_name),
        field("attributes", optional($._html_attributes_pattern_no_gt)),
        choice(
          seq(
            ">",
            field("content", repeat($._html_content)),
            "</",
            alias($.html_tag_name, $.tag_name_closing),
            ">",
          ), // Matched closing tag
          "/>", // Self-closing
        ),
      ),

    html_block_tag: ($) =>
      seq(
        "<",
        field(
          "name",
          alias(
            token(prec(1, new RegExp(HTML_TAGS_BLOCK.join("|"), "i"))),
            $.html_tag_name,
          ),
        ),
        field("attributes", optional($._html_attributes_pattern_no_gt)),
        choice(
          seq(
            ">",
            field("content", repeat($._html_content)),
            "</",
            alias($.html_tag_name, $.tag_name_closing),
            ">",
          ),
          // Some block tags might be self-closing in wikitext context if not properly handled by MediaWiki parser
          // but typically they are not. We allow it for robustness.
          "/>",
        ),
      ),

    html_inline_tag: ($) =>
      seq(
        "<",
        field(
          "name",
          alias(
            token(prec(1, new RegExp(HTML_TAGS_INLINE.join("|"), "i"))),
            $.html_tag_name,
          ),
        ),
        field("attributes", optional($._html_attributes_pattern_no_gt)),
        choice(
          seq(
            ">",
            field("content", repeat($._html_content)),
            "</",
            alias($.html_tag_name, $.tag_name_closing),
            ">",
          ),
          "/>",
        ),
      ),

    html_void_tag: ($) =>
      seq(
        "<",
        field(
          "name",
          alias(
            token(prec(1, new RegExp(HTML_TAGS_VOID.join("|"), "i"))),
            $.html_tag_name,
          ),
        ),
        field("attributes", optional($._html_attributes_pattern_no_gt)),
        choice(">", "/>"), // Void tags can be <tag> or <tag />
      ),

    // An entity can be named, numeric (decimal), or numeric (hexacecimal). The
    // longest entity name is 29 characters long, and the HTML spec says that
    // no more will ever be added.
    entity: (_) => /&(#([xX][0-9a-fA-F]{1,6}|[0-9]{1,5})|[A-Za-z]{1,30});?/,
  },
});
