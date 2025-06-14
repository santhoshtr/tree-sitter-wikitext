;; Query to find all links in Wikitext

;; Match internal links
(wikilink
  (wikilink_page))

;; Match internal links with labels
(wikilink
  (wikilink_page)
  (page_name_segment))

;; Match external links (URLs only)
(external_link
  (url))

;; Match external links with labels
(external_link
  (url)
  (page_name_segment))
