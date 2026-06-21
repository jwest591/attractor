#!/usr/bin/env bash
# SessionStart hook for ClaudeCodeTmuxBackend.
# Writes the transcript path (from stdin JSON) to $ATTRACTOR_NODE_LOG_DIR/transcript.txt
# so the backend can discover it without scanning ~/.claude/projects/.
set -u

if [ -z "${ATTRACTOR_NODE_LOG_DIR:-}" ]; then
    echo "att-session-start: ATTRACTOR_NODE_LOG_DIR is not set" >&2
    exit 1
fi

INPUT="$(cat)"
transcript_path=$(printf '%s' "$INPUT" | jq -r '.transcript_path // ""')

if [ -z "$transcript_path" ]; then
    echo "att-session-start: transcript_path not found in hook payload" >&2
    exit 1
fi

target="$ATTRACTOR_NODE_LOG_DIR/transcript.txt"

if [ -f "$target" ]; then
    echo "att-session-start: $target already exists" >&2
    exit 1
fi

mkdir -p "$ATTRACTOR_NODE_LOG_DIR" || exit 1
# Write atomically: crash between create and write would leave an empty file that blocks re-runs
printf '%s\n' "$transcript_path" > "${target}.tmp"
mv "${target}.tmp" "$target"
