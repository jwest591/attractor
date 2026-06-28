#!/usr/bin/env bash
# next-story.sh -- Return the next incomplete story in a given epic.
#
# Usage: next-story.sh <epic_number> <sprint-status-file>
#
# Scans development_status for keys of the form "<epic>-<N>" or
# "<epic>-<N>-<rest>" (e.g. "1-3-zustand-stores" for epic 1), in document
# order, and prints the key of the first entry whose status is not "done".
#
# Epic-level keys ("epic-1") and retrospective keys ("epic-1-retrospective")
# are skipped.
#
# Exit codes:
#   0  story key printed to stdout
#   1  no incomplete story found (all done or epic has no stories)
#   2  bad arguments

set -u

EPIC="${1:-}"
SPRINT_FILE="${2:-}"

if [ -z "$EPIC" ] || [ -z "$SPRINT_FILE" ]; then
    echo "usage: $0 <epic_number> <sprint-status-file>" >&2
    exit 2
fi

if [ ! -f "$SPRINT_FILE" ]; then
    echo "error: file not found: $SPRINT_FILE" >&2
    exit 2
fi

# Validate epic is numeric
if ! printf '%s' "$EPIC" | grep -qE '^[0-9]+$'; then
    echo "error: epic_number must be numeric, got: $EPIC" >&2
    exit 2
fi

# Find first story key in this epic whose status is not "done".
# Story keys match: <epic>-<digit(s)> or <epic>-<digit(s)>-<anything>
# Epic-level keys like "epic-1" and "epic-1-retrospective" are excluded by
# requiring the prefix to start with a digit.
result=$(awk -v epic="$EPIC" '
    {
        line = $0
        gsub(/^[[:space:]]+/, "", line)
        if (line == "" || substr(line, 1, 1) == "#") next

        colon = index(line, ":")
        if (colon == 0) next
        key = substr(line, 1, colon - 1)
        val = substr(line, colon + 1)

        gsub(/[[:space:]]/, "", key)
        gsub(/^[[:space:]]+/, "", val)
        sub(/[[:space:]]*#.*$/, "", val)
        gsub(/[[:space:]]+$/, "", val)

        # Match "<epic>-<digits>" or "<epic>-<digits>-<rest>"
        # The character after the epic prefix must be a digit (rules out
        # "epic-N" which would start with a letter before the dash).
        prefix = epic "-"
        if (index(key, prefix) != 1) next

        rest = substr(key, length(prefix) + 1)
        # rest must start with a digit
        if (rest !~ /^[0-9]/) next

        if (val != "done") {
            print key
            exit
        }
    }
' "$SPRINT_FILE")

if [ -z "$result" ]; then
    exit 1
fi

jq -n --arg story "$result" '{"story":$story}'
