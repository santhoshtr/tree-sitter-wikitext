/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
	name: "wikitext",

	extras: ($) => [$._comment, "\r"],

	rules: {
		source_file: ($) => repeat($._node),

		_node: ($) =>
			prec.right(
				choice(
					prec(3, $.heading),
					$._internal_link,
					$._external_link,
					$._list,
					$.indent,
					$.table,
					$._htmltag,
					$.bold,
					$.italic,
					$.redirect,
					$.newline,
					// everything else is text!
					$._textrange,
				),
			),

		// -----------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------- REDIRECT
		// -----------------------------------------------------------------------------------------

		redirect: ($) =>
			seq(
				choice("#REDIRECT", "#redirect", "#Redirect"),
				optional(/\s+/),
				$.wikilink,
			),

		// -----------------------------------------------------------------------------------------
		// --------------------------------------------------------------------------------- COMMENT
		// -----------------------------------------------------------------------------------------

		_comment: (_) => token(seq("<!--", /[^-]+/, "-->")),

		// the repeat1 generates a node for each character but allows
		// rules to be identified at each new character
		_textrange: ($) => prec.right(alias(repeat1($._text), $.text)),

		text: (_) =>
			prec(
				-1, // precedence might not be needed here
				/[^\n]/,
			),

		_text: (_) =>
			prec(
				-1, // precedence might not be needed here
				/[^\n]/,
			),

		newline: (_) => prec(-1, "\n"),

		// -----------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------------- HEADINGS
		// -----------------------------------------------------------------------------------------

		heading: ($) => choice($.h1, $.h2, $.h3, $.h4, $.h5, $.h6),
		h1: ($) => seq("\n=", $._textrange, "="),
		h2: ($) => seq("\n==", $._textrange, "=="),
		h3: ($) => seq("\n===", $._textrange, "==="),
		h4: ($) => seq("\n====", $._textrange, "===="),
		h5: ($) => seq("\n=====", $._textrange, "====="),
		h6: ($) => seq("\n======", $._textrange, "======"),

		// -----------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------- INTERNAL LINKS
		// -----------------------------------------------------------------------------------------

		_internal_link: ($) => choice($.wikilink, $.category, $.file, $.image),

		// categories are specially formatted wikilinks
		category: ($) =>
			seq(
				/\[\[\s*(c|C)ategory\s*:/,
				alias($._categorytarget, $.category),
				optional(seq("|", optional(field("label", $.wikilabel)))),
				"]]",
			),

		// NOTE: the file and image links are difficult to implement fully in treesitter
		// -> the separation of attributes and label happen in Python...

		// file links, a special wikilink format, are mostly for images and can contain attributes
		file: ($) =>
			seq(
				/\[\[\s*(f|F)ile\s*:/,
				alias($.wikitarget, $.file),
				optional(alias($._attributed_label, $.attr_label)),
				"]]",
			),

		// image links are a special type of wikilink that can contain style attributes
		image: ($) =>
			seq(
				/\[\[\s*(i|I)mage\s*:/,
				alias($.wikitarget, $.file),
				optional(alias($._attributed_label, $.attr_label)),
				"]]",
			),

		_attributed_label: ($) => seq("|", $._textrange),

		_attributes: ($) => repeat1(seq($.attribute, "|")),

		_label: ($) => alias(repeat1($._node), $.label),

		// wikilinks are internal links
		wikilink: ($) =>
			seq(
				"[[",
				alias($._categorytarget, $.target),
				optional(seq("|", /\s*/, alias(repeat1($._node), $.label))),
				"]]",
			),

		wikitarget: ($) => $._wikilinktargettext,

		wikilabel: ($) => $._wikilinktext,

		attribute: ($) => $._wikilinktext,

		_wikilinktargettext: (_) => /[^\|\n\[\]]+/,
		_categorytarget: (_) => /[^\|\n\[\]]+/,

		_wikilinktext: (_) => /[^\|\n\[\]]+/,

		// -----------------------------------------------------------------------------------------
		// -------------------------------------------------------------------------- EXTERNAL LINKS
		// -----------------------------------------------------------------------------------------

		_external_link: ($) => $.external_link,

		external_link: ($) =>
			seq(
				"[",
				alias($.external_link_target, $.target),
				optional(alias(repeat1($._node), $.label)),
				"]",
			),

		external_link_target: (_) => /[^\|\n\[\]\s]+/,

		// -----------------------------------------------------------------------------------------
		// ----------------------------------------------------------------------------------- LISTS
		// -----------------------------------------------------------------------------------------

		_list: ($) => choice($.bullet_list, $.number_list),

		bullet_list: ($) => prec.right(repeat1($._bullet_item)),

		number_list: ($) => prec.right(repeat1($._numbered_item)),

		_bullet_item: ($) => choice($.bl1, $.bl2, $.bl3),
		_numbered_item: ($) => choice($.nl1, $.nl2, $.nl3),

		bl1: ($) => seq("*", alias(repeat1($._node), $.content), "\n"),

		bl2: ($) => seq("**", alias(repeat1($._node), $.content), "\n"),

		bl3: ($) => seq("***", alias(repeat1($._node), $.content), "\n"),

		nl1: ($) => seq("#", alias(repeat1($._node), $.content), "\n"),

		nl2: ($) => seq("##", alias(repeat1($._node), $.content), "\n"),

		nl3: ($) => seq("###", alias(repeat1($._node), $.content), "\n"),

		// -----------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------- INDENT
		// -----------------------------------------------------------------------------------------

		indent: ($) => seq("\n:", alias(repeat1($._node), $.content), "\n"),

		// -----------------------------------------------------------------------------------------
		// ---------------------------------------------------------------------------------- TABLES
		// -----------------------------------------------------------------------------------------

		table: ($) =>
			seq(
				"{|",
				optional(repeat1($.attribute)),
				"\n",
				optional($.tablecaption),
				optional(alias(repeat1($.tableheader), $.colheaders)),
				optional(repeat1($.tablecell)),
				repeat($.tablerow),
				"|}",
			),

		tablecaption: ($) => seq("|+", alias(repeat1($._node), $.content), "\n"),

		tableheader: ($) => seq("!", alias(repeat1($._node), $.content), "\n"),

		tablecell: ($) =>
			seq(
				"|",
				optional(repeat1(seq(alias($._textrange, $.attribute), "|"))),
				alias(repeat1($._node), $.content),
				"\n",
			),

		tablerow: ($) =>
			seq(
				"|-",
				optional(alias($._textrange, $.attribute)),
				"\n",
				optional(repeat1($.tableheader)),
				repeat($.tablecell),
			),

		// -----------------------------------------------------------------------------------------
		// ------------------------------------------------------------------------------- HTML TAGS
		// -----------------------------------------------------------------------------------------

		_htmltag: ($) => choice($.htmlopen, $.htmlclose, $.htmlopenclose),

		htmlopen: ($) =>
			seq(
				"<",
				field("name", $.tagname),
				optional(field("htmltagcontents", $._textrange)),
				">",
			),

		htmlclose: ($) => seq("</", field("name", $.tagname), ">"),

		htmlopenclose: ($) => seq("<", field("name", $.tagname), "/>"),

		tagname: (_) => /[a-zA-Z][\w-]*/,

		// -----------------------------------------------------------------------------------------
		// ------------------------------------------------------------------------------ TYPOGRAPHY
		// -----------------------------------------------------------------------------------------

		bold: ($) =>
			prec.left(
				1,
				seq("'''", alias(repeat1($._node), $.content), choice("'''", "’’’")),
			),

		italic: ($) =>
			prec.left(
				2,
				seq("''", alias(repeat1($._node), $.content), choice("''", "’’")),
			),
	},
});
