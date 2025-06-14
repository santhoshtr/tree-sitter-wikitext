;; Highlighting rules for Wikitext

;; Highlight internal links
(wikilink
  (wikilink_page)) @text.uri

(wikilink
  (page_name_segment)) @text.reference

;; Highlight external links
(external_link
  (url)) @text.uri

(external_link
  (page_name_segment)) @text.reference

;; Highlight bold and italic text
(bold) @text.strong
(italic) @text.emphasis

;; Highlight headings
(heading
  (heading_marker)) @punctuation.special
(heading
  (text)) @text.title

;; Highlight templates
(template
  (template_name)) @text.literal

;; Highlight comments
(comment) @comment

;; Highlight lists
(unordered_list_item
  (list_marker)) @punctuation.special
(ordered_list_item
  (list_marker)) @punctuation.special

;; Highlight table syntax
(table
  (table_row
    (table_cell))) @text.literal
