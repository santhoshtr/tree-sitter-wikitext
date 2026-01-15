/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

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
    $._template_param_value_marker,
    $._template_param_name_value_marker,
    $._html_tag_open_marker,
    $._html_tag_close_marker,
    $._html_self_closing_tag_marker,
    $.unordered_list_marker,
    $.ordered_list_marker,
  ],
  extras: (_) => ["\r", /\s/],
  conflicts: ($) => [[$.nowiki_tag_block, $.nowiki_inline_element]],
  precedences: ($) => [
    // Precedence for ''''' (bold italic) vs ''' (bold) and '' (italic)
    ["bold_italic_explicit", $.bold, $.italic],
    [$._block_not_section, $.html_tag],
    [$._inline_content, $.html_tag],
  ],
  rules: {
    source_file: ($) =>
      seq(
        alias(prec.right(repeat($._block_not_section)), $.section),
        repeat($.section),
      ),
    // BLOCK STRUCTURE

    // All blocks. Every block contains a trailing newline.
    _block: ($) => choice($._block_not_section, $.section),
    _block_not_section: ($) =>
      choice(
        //$.heading,
        $.horizontal_rule,
        $._list,
        $.preformatted_block,
        $.table,
        $.paragraph, // Must be low precedence
        $._blank_line, // Consumes blank lines separating blocks
        $.syntaxhighlight,
      ),
    section: ($) =>
      choice(
        $._section1,
        $._section2,
        $._section3,
        $._section4,
        $._section5,
        $._section6,
      ),
    _section1: ($) =>
      prec.right(
        seq(
          $.heading1,
          repeat(
            choice(
              alias(
                choice(
                  $._section6,
                  $._section5,
                  $._section4,
                  $._section3,
                  $._section2,
                ),
                $.section,
              ),
              $._block_not_section,
            ),
          ),
        ),
      ),
    _section2: ($) =>
      prec.right(
        seq(
          $.heading2,
          repeat(
            choice(
              alias(
                choice($._section6, $._section5, $._section4, $._section3),
                $.section,
              ),
              $._block_not_section,
            ),
          ),
        ),
      ),
    _section3: ($) =>
      prec.right(
        seq(
          $.heading3,
          repeat(
            choice(
              alias(choice($._section6, $._section5, $._section4), $.section),
              $._block_not_section,
            ),
          ),
        ),
      ),
    _section4: ($) =>
      prec.right(
        seq(
          $.heading4,
          repeat(
            choice(
              alias(choice($._section6, $._section5), $.section),
              $._block_not_section,
            ),
          ),
        ),
      ),
    _section5: ($) =>
      prec.right(
        seq(
          $.heading5,
          repeat(choice(alias($._section6, $.section), $._block_not_section)),
        ),
      ),
    _section6: ($) => prec.right(seq($.heading6, repeat($._block_not_section))),

    _blank_line: ($) => /\n\s*\n?/,
    _pipe: ($) => token.immediate("|"),
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
        $.parser_function, // Must come before template
        $.template,
        $.magic_word,
        $.signature,
        $.redirect,
        $.html_tag,
        $.entity,
        $.nowiki_inline_element, // <nowiki>content</nowiki> or <nowiki />
        // Everything else
        $.text,
      ),
    // Text is a sequence of non-special characters or characters that don't form other tokens
    // This is a fallback and should have low precedence
    text: ($) => prec(-2, $._inline_text_base),

    // More specific text tokens for contexts where fewer delimiters apply
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
          $.html_tag,
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
        $.html_tag,
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

    file_caption: ($) => repeat1(choice($._inline_content, $._newline)),
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
        $.html_tag,
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
      seq(
        "{{",
        field("name", $.template_name),
        repeat($.template_argument),
        "}}",
      ),

    template_name: ($) =>
      seq(
        alias($._text_no_pipes_braces_colon_hash_equals, $.template_name_part),
        // Templates optionally have a colon (namespace) but nothing after it before |
        // OR hash but nothing after it before |
        // If there IS content after : or #, it's a parser function (handled separately)
        optional(choice(
          alias(token.immediate(prec(1, ":")), $.template_name_colon),
          alias(token.immediate(prec(1, "#")), $.template_name_hash),
        )),
      ),

    // ==== Parser Functions ====
    // Parser functions like {{PLURAL:value|opts}} and {{#if:condition|then}}
    // Must come before template in choices to have priority
    parser_function: ($) =>
      choice(
        $.parser_function_colon,
        $.parser_function_hash,
      ),

    // Colon-based parser functions: {{PLURAL:$1|is|are}}
    parser_function_colon: ($) =>
      seq(
        "{{",
        alias($._text_no_pipes_braces_colon_hash_equals, $.parser_function_name),
        alias(token.immediate(prec(2, ":")), $.function_delimiter),
        repeat(  // Changed from repeat1 to allow empty first parameter
          choice(
            alias($._text_no_pipes_braces, $.param_text),
            $.parser_function, // Nested parser functions
            $.template, // Nested templates
            $.wikilink, // Nested links
          ),
        ),
        repeat($.template_argument),
        "}}",
      ),

    // Hash-based parser functions: {{#if:condition|then|else}}
    parser_function_hash: ($) =>
      seq(
        "{{",
        alias(token.immediate(prec(2, "#")), $.function_prefix),
        alias($._text_no_pipes_braces_colon_hash_equals, $.parser_function_name),
        alias(token.immediate(prec(2, ":")), $.function_delimiter),
        repeat(  // Changed from repeat1 to allow empty first parameter
          choice(
            alias($._text_no_pipes_braces, $.param_text),
            $.parser_function, // Nested parser functions
            $.template, // Nested templates
            $.wikilink, // Nested links
          ),
        ),
        repeat($.template_argument),
        "}}",
      ),

    _text_no_pipes_braces: ($) =>
      token(prec(1, /[^\|{}]+/)),

    _text_no_pipes_braces_colon_hash_equals: ($) =>
      token(prec(1, /[^\|{}:#=]+/)), // Allow spaces in name. But not in the beginning.

    template_argument: ($) =>
      seq(
        $._pipe,
        choice(
          seq($._template_param_value_marker, $.template_param_value),
          seq(
            $._template_param_name_value_marker,
            $.template_param_name,
            "=",
            $.template_param_value,
          ),
        ),
      ),
    template_param_name: ($) => $._text_no_pipes_braces_equals,

    _text_no_pipes_braces_equals: ($) => token(prec(1, /[^\|{}=]+/)),

    template_param_value: ($) =>
      repeat1(
        choice(
          $._inline_content,
          $._list, // Nested list
          $._blank_line,
        ),
      ),
    // ==== Lists ====
    _list: ($) => choice($.unordered_list, $.ordered_list, $.definition_list),

    // Content of a list item can be complex, including nested lists or paragraphs
    list_item_content: ($) =>
      prec.left(
        repeat1(
          choice(
            $._inline_content, // Inline content directly
            $._list, // Nested list
          ),
        ),
      ),

    unordered_list: ($) => prec.right(repeat1($.unordered_list_item)),
    unordered_list_item: ($) =>
      prec.right(
        seq(
          alias($.unordered_list_marker, $.list_marker),
          field("content", $.list_item_content),
          $._newline, // Items are typically one per line
        ),
      ),
    ordered_list: ($) => prec.right(repeat1($.ordered_list_item)),
    ordered_list_item: ($) =>
      prec.right(
        seq(
          alias($.ordered_list_marker, $.list_marker),
          field("content", $.list_item_content),
          $._newline,
        ),
      ),
    definition_list: ($) =>
      prec.right(repeat1(choice($.definition_list_item, $.definition_data))),
    definition_list_item: ($) =>
      prec.right(
        seq(
          alias(";", $.list_marker),
          field("term", $.list_item_content),
          optional(
            seq(
              alias(":", $.list_marker),
              field("data", $.list_item_content),
              optional($._newline),
            ),
          ),
          $._newline,
        ),
      ),
    definition_data: ($) =>
      prec.right(
        seq(
          alias(":", $.list_marker),
          field("data", $.list_item_content),
          $._newline,
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
        optional(repeat1(choice($.table_attribute, $.template, "|"))),
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
    _html_attributes_pattern_no_gt: ($) => repeat1(seq(" ", $.html_attribute)),

    _html_content: ($) =>
      choice(
        $._inline_content, // Allows MediaWiki markup inside HTML tags (often the case)
        $._blank_line,
      ),

    html_tag: ($) => choice($._self_closing_tag, $._html_generic_tag),
    _self_closing_tag: ($) =>
      seq(
        "<",
        $._html_self_closing_tag_marker,
        field("name", $.html_tag_name),
        field("attributes", optional($._html_attributes_pattern_no_gt)),
        choice(">", "/>"),
      ),

    _html_generic_tag: ($) =>
      seq(
        "<",
        $._html_tag_open_marker,
        field("name", $.html_tag_name),
        field("attributes", optional($._html_attributes_pattern_no_gt)),
        choice(
          seq(
            ">",
            field("content", repeat($._html_content)),
            "</",
            $._html_tag_close_marker,
            $.html_tag_name,
            ">",
          ),
          // Some block tags might be self-closing in wikitext context if not properly handled by MediaWiki parser
          // but typically they are not. We allow it for robustness.
          "/>",
        ),
      ),

    code_language: ($) => /[a-zA-Z0-9_-]+/, // e.g., python, javascript

    syntaxhighlight: ($) =>
      seq(
        "<syntaxhighlight",
        optional(seq("lang", "=", '"', $.code_language, '"')),
        repeat($.html_attribute),
        ">",
        repeat(alias(token(/.+/), $.code)),
        "</syntaxhighlight>",
      ),

    // An entity can be named, numeric (decimal), or numeric (hexacecimal). The
    // longest entity name is 29 characters long, and the HTML spec says that
    // no more will ever be added.
    entity: (_) => /&(#([xX][0-9a-fA-F]{1,6}|[0-9]{1,5})|[A-Za-z]{1,30});?/,
  },
});
