;; Query to find all templates in Wikitext

;; Match basic templates
(template
  (template_name))

;; Match templates with parameters
(template
  (template_name)
  (template_parameters
    (parameter)))

;; Match nested templates
(template
  (template_name)
  (template_parameters
    (parameter
      (template))))
