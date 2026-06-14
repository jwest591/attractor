#!/usr/bin/env bash
# SessionStart hook for ClaudeCodeTmuxBackend.
# Writes the JSONL transcript path to /tmp/att-<session_title>-transcript.txt
# so the backend can discover it without scanning ~/.claude/projects/.
set -u
INPUT="$(cat)"
transcript_path=$(printf '%s' "$INPUT" | jq -r '.transcript_path // ""')
session_title=$(printf '%s'  "$INPUT" | jq -r '.session_title  // ""')
if [ -n "$session_title" ] && [ -n "$transcript_path" ]; then
    printf '%s\n' "$transcript_path" > "/tmp/att-${session_title}-transcript.txt"
fi
exit 0
