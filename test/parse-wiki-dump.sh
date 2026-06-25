#!/usr/bin/env bash
#
# Stress-test the grammar against a real Wikipedia dump.
#
# Usage: test/parse-wiki-dump.sh [wiki]
#   wiki   the dump database name, e.g. tlywiki, mlwiki (default: tlywiki).
#          Pick a small wiki to keep the download small.
#
# It streams https://dumps.wikimedia.org/<wiki>/latest/<wiki>-latest-pages-articles.xml.bz2,
# parses every main-namespace article with `tree-sitter parse`, and prints the
# title of any article whose parse tree contains an ERROR node.
#
set -euo pipefail

wiki="${1:-tlywiki}"
url="https://dumps.wikimedia.org/${wiki}/latest/${wiki}-latest-pages-articles.xml.bz2"

# Run tree-sitter from the grammar root so it picks up this repo's parser.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# A sentinel byte that never occurs in wikitext, used to delimit page records
# in the stream below.
SEP="$(printf '\x01')"

echo "Streaming ${wiki} dump..." >&2

total=0
failed=0

# Pipeline:
#   curl  -> stream the compressed dump
#   bzcat -> decompress on the fly
#   xq    -> the PyPI `yq` package's XML transcoder (jq for XML). --xml-item-depth=2
#            descends to each <page> and streams it, so the multi-GB dump is never
#            held in memory. For each main-namespace (ns=0) page it emits one
#            SEP-terminated record:   <title>\n<wikitext><SEP>
# `read -d SEP` consumes one record at a time and pipes just that article's
# wikitext to `tree-sitter parse`, so nothing is written to disk regardless of
# dump size. SEP terminates every record (so the last one isn't lost); jq's
# trailing newline becomes a leading newline on the next record, stripped below.
while IFS= read -r -d "$SEP" record; do
  record="${record#$'\n'}"          # drop jq's inter-record newline
  [ -z "$record" ] && continue
  total=$((total + 1))
  title="${record%%$'\n'*}"   # first line
  text="${record#*$'\n'}"     # everything after it
  # Capture rather than pipe into the `if`: with -q, tree-sitter prints a single
  # (ERROR ...) line only on failure, and `|| true` keeps set -e/pipefail from
  # aborting on its non-zero exit.
  out="$(printf '%s' "$text" | tree-sitter parse -q /dev/stdin 2>/dev/null)" || true
  case "$out" in
    *ERROR*)
      failed=$((failed + 1))
      echo "PARSE ERROR: $title"
      ;;
  esac
  if [ $((total % 1000)) -eq 0 ]; then
    echo "  ...${total} parsed, ${failed} failed" >&2
  fi
done < <(
  curl -sSL --retry 3 -A "tree-sitter-wikitext dump tester (https://github.com/wikimedia/tree-sitter-wikitext)" "$url" \
    | bzcat \
    | uvx --from yq xq --xml-item-depth=2 -r --arg sep "$SEP" \
        'select(.ns == "0") | .title + "\n" + (.revision.text["#text"]? // .revision.text? // "") + $sep'
)

echo "Done. ${failed} of ${total} articles in ${wiki} failed to parse." >&2
