;; Query to find all templates in Wikitext

;; Match basic templates
(template
  (template_name
    (template_name_part))) @template.name

;; Match templates with unnamed parameters
(template
  (template_name
    (template_name_part))
  (template_argument
    (template_param_value
      (text)))) @template.parameter

;; Match templates with named parameters
(template
  (template_name
    (template_name_part))
  (template_argument
    (template_param_value
      (text))
    (template_param_value
      (text)))) @template.parameter

;; Match nested templates
(template
  (template_name
    (template_name_part))
  (template_argument
    (template_param_value
      (template
        (template_name
          (template_name_part))
        (template_argument
          (template_param_value
            (text))))))) @template.nested
