;; Highlighting rules for Wikitext

;; Highlight headings
(heading1
  (heading_marker) @punctuation.special
  (text) @markup.heading.1
  (heading_marker) @punctuation.special
)
(heading2
  (heading_marker) @punctuation.special
  (text) @markup.heading.2
  (heading_marker) @punctuation.special
)
(heading3
  (heading_marker) @punctuation.special
  (text) @markup.heading.3
  (heading_marker) @punctuation.special
)
(heading4
  (heading_marker) @punctuation.special
  (text) @markup.heading.4
  (heading_marker) @punctuation.special
)
(heading5
  (heading_marker) @punctuation.special
  (text) @markup.heading.5
  (heading_marker) @punctuation.special
)

(heading6
  (heading_marker) @punctuation.special
  (text) @markup.heading.6
  (heading_marker) @punctuation.special
)

(wikilink
  (wikilink_page) @markup.link.url
  (page_name_segment)? @markup.link.label
)
(external_link
  (url) @markup.link.url
  (page_name_segment)? @markup.link.label
)
(template
  (template_name) @module
  (template_argument
  (template_param_name) @tag.attribute
  )
)

(comment) @comment

[
  "[["
  "]]"
  "{{"
  "}}"
  "{|"
  "|}"
  "["
  "]"
  "<"
  ">"
  "</"
  "/>"
] @punctuation.bracket

[
  "|"
  "|-"
  "|+"
  "!"
  "!!"
  "||"
] @punctuation.delimiter

(table_cell_block
  (content) @text
)
(table_cell_inline
  (content) @text
)
(table_header_block
  (content) @text.special
)
(table_header_inline
  (content) @text.special
)
(table_cell_inline
  (content) @text
)

(html_attribute
  (html_attribute_name) @attribute
  (html_attribute_value) @string
)

(paragraph
  (italic) @markup.italic
  (bold) @markup.strong
)

