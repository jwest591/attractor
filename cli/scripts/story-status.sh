#!/usr/bin/env bash
# story-status.sh -- Extract story status from a sprint status YAML file.
#
# Usage: story-status.sh <story-id> <sprint-status-file>
#
# Accepts both short and long form story ids:
#   short: "1-3"
#   long:  "1-3-zustand-stores"
#
# Both forms match any YAML key that equals the short prefix or starts with
# the short prefix followed by "-" (e.g. "1-3" and "1-3-zustand-stores" both
# match "1-3", "1-3-zustand-stores", and "1-3-engine-correctness-hardening").
# If the id appears multiple times, returns the first status that is not
# "done", or "done" if all are "done".
#
# Exit codes:
#   0  status printed to stdout
#   1  story id not found
#   2  bad arguments

set -u

STORY_ID="${1:-}"
SPRINT_FILE="${2:-}"

if [ -z "$STORY_ID" ] || [ -z "$SPRINT_FILE" ]; then
    echo "usage: $0 <story-id> <sprint-status-file>" >&2
    exit 2
fi

if [ ! -f "$SPRINT_FILE" ]; then
    echo "error: file not found: $SPRINT_FILE" >&2
    exit 2
fi

# Normalize to short form: extract the leading "<epic>-<story>" numeric prefix.
# "1-3-zustand-stores" -> "1-3";  "1-3" -> "1-3" (unchanged).
MATCH_ID=$(printf '%s' "$STORY_ID" | sed 's/^\([0-9][0-9]*-[0-9][0-9]*\).*/\1/')

# Extract statuses for all keys that equal match-id or start with match-id-.
# Lines look like: "  key: value  # optional comment"
statuses=$(awk -v id="$MATCH_ID" '
    {
        line = $0
        # Skip blank lines and comment-only lines
        gsub(/^[[:space:]]+/, "", line)
        if (line == "" || substr(line, 1, 1) == "#") next

        # Split on first colon
        colon = index(line, ":")
        if (colon == 0) next
        key = substr(line, 1, colon - 1)
        val = substr(line, colon + 1)

        # Normalise key (no whitespace expected, but be safe)
        gsub(/[[:space:]]/, "", key)

        # Trim value: leading whitespace, trailing comment, trailing whitespace
        gsub(/^[[:space:]]+/, "", val)
        sub(/[[:space:]]*#.*$/, "", val)
        gsub(/[[:space:]]+$/, "", val)

        # Match: exact key or key is a longer name prefixed with id-
        # (e.g. id="7-4" matches "7-4" and "7-4-engine-..." but not "7-40-...")
        if (key == id || index(key, id "-") == 1) {
            print val
        }
    }
' "$SPRINT_FILE")

if [ -z "$statuses" ]; then
    echo "not found: $STORY_ID (matched as: $MATCH_ID)" >&2
    exit 1
fi

# Return the first non-done status, or "done" if every status is done.
first_non_done=""
all_done=true
while IFS= read -r s; do
    [ -z "$s" ] && continue
    if [ "$s" != "done" ]; then
        all_done=false
        [ -z "$first_non_done" ] && first_non_done="$s"
    fi
done <<< "$statuses"

if $all_done; then
    echo "done"
else
    echo "$first_non_done"
fi
