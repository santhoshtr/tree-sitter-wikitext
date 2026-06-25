#!/usr/bin/env bash
#
# Stress-test the grammar against a real Wikipedia dump.
#
# Usage: test/parse-wiki-dump.sh [wiki] [--chunk N]
#   wiki        the dump database name, e.g. tlywiki, mlwiki (default: tlywiki).
#   --chunk N   parse N articles per `tree-sitter parse` invocation instead of
#               one process per article. Much faster on large wikis; uses at
#               most N small temp files at a time. Default 1 (pure streaming,
#               zero temp files). Try --chunk 2000 for big dumps.
#
# It streams https://dumps.wikimedia.org/<wiki>/latest/<wiki>-latest-pages-articles.xml.bz2,
# parses every main-namespace article with `tree-sitter parse`, and prints the
# title of any article whose parse tree contains an ERROR node.
#
set -euo pipefail

wiki="tlywiki"
chunk=1
while [ $# -gt 0 ]; do
  case "$1" in
    --chunk|-c) chunk="$2"; shift 2 ;;
    --chunk=*)  chunk="${1#*=}"; shift ;;
    -*)         echo "Unknown option: $1" >&2; exit 2 ;;
    *)          wiki="$1"; shift ;;
  esac
done
url="https://dumps.wikimedia.org/${wiki}/latest/${wiki}-latest-pages-articles.xml.bz2"

# Run tree-sitter from the grammar root so it picks up this repo's parser.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# A sentinel byte that never occurs in wikitext, used to delimit page records.
SEP="$(printf '\x01')"

total=0
failed=0

# --- batched mode state (only used when chunk > 1) -------------------------
chunk_dir=""
declare -a titles=()        # titles[k] -> title of $chunk_dir/k.wikitext
buffered=0
if [ "$chunk" -gt 1 ]; then
  chunk_dir="$(mktemp -d)"
  trap 'rm -rf "$chunk_dir"' EXIT
fi

# Parse the current batch in a single tree-sitter invocation, report any
# ERROR-bearing articles by title, then reset the batch. With -q, tree-sitter
# prints one line (starting with the file path) only for files that fail.
flush() {
  [ "$buffered" -eq 0 ] && return 0
  local paths errfiles path idx k
  paths="$chunk_dir/paths.txt"
  : > "$paths"
  for ((k = 0; k < buffered; k++)); do
    printf '%s/%d.wikitext\n' "$chunk_dir" "$k" >> "$paths"
  done
  # </dev/null: keep tree-sitter from draining the producer pipe on the loop's stdin.
  # tree-sitter pads the filename column with spaces before a tab, and our temp
  # paths contain no spaces, so cut each line at its first whitespace.
  errfiles="$(tree-sitter parse -q --paths "$paths" </dev/null 2>/dev/null | grep -F 'ERROR' | sed -E 's/[[:space:]].*$//')" || true
  if [ -n "$errfiles" ]; then
    while IFS= read -r path; do
      idx="$(basename "$path" .wikitext)"
      failed=$((failed + 1))
      echo "PARSE ERROR: ${titles[idx]}"
    done <<< "$errfiles"
  fi
  buffered=0
  titles=()
}

echo "Streaming ${wiki} dump (chunk=${chunk})..." >&2

# Pipeline:
#   curl  -> stream the compressed dump
#   bzcat -> decompress on the fly
#   xq    -> the PyPI `yq` package's XML transcoder (jq for XML). --xml-item-depth=2
#            descends to each <page> and streams it, so the multi-GB dump is never
#            held in memory. For each main-namespace (ns=0) page it emits one
#            SEP-terminated record:   <title>\n<wikitext><SEP>
# SEP terminates every record (so the last one isn't lost); jq's trailing
# newline becomes a leading newline on the next record, stripped below.
while IFS= read -r -d "$SEP" record; do
  record="${record#$'\n'}"          # drop jq's inter-record newline
  [ -z "$record" ] && continue
  total=$((total + 1))
  title="${record%%$'\n'*}"   # first line
  text="${record#*$'\n'}"     # everything after it

  if [ "$chunk" -le 1 ]; then
    # Streaming mode: pipe just this article to tree-sitter, never touch disk.
    # `|| true` keeps set -e/pipefail from aborting on its non-zero exit.
    out="$(printf '%s' "$text" | tree-sitter parse -q /dev/stdin 2>/dev/null)" || true
    case "$out" in
      *ERROR*) failed=$((failed + 1)); echo "PARSE ERROR: $title" ;;
    esac
  else
    # Batched mode: buffer this article into a reused temp file; flush at chunk size.
    printf '%s' "$text" > "$chunk_dir/$buffered.wikitext"
    titles[buffered]="$title"
    buffered=$((buffered + 1))
    [ "$buffered" -ge "$chunk" ] && flush
  fi

  if [ $((total % 1000)) -eq 0 ]; then
    echo "  ...${total} parsed, ${failed} failed" >&2
  fi
done < <(
  curl -sSL --retry 3 -A "tree-sitter-wikitext dump tester (https://github.com/wikimedia/tree-sitter-wikitext)" "$url" \
    | bzcat \
    | uvx --from yq xq --xml-item-depth=2 -r --arg sep "$SEP" \
        'select(.ns == "0") | .title + "\n" + (.revision.text["#text"]? // .revision.text? // "") + $sep'
)

[ "$chunk" -gt 1 ] && flush

echo "Done. ${failed} of ${total} articles in ${wiki} failed to parse." >&2
