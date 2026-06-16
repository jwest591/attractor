#!/usr/bin/env bash
# SessionStart hook for ClaudeCodeTmuxBackend.
# Writes the JSONL transcript path to .attractor/att-<session_title>-transcript.txt
# so the backend can discover it without scanning ~/.claude/projects/.
#
# Claude Code fires this hook before it finalises its project root, so the
# hook-supplied transcript_path may point to the wrong directory (e.g. when a
# .claude/ dir exists in a parent of CLAUDE_PROJECT_DIR).  We work around this
# by searching recently-modified JSONL files for one whose customTitle matches
# the session title, falling back to the hook-provided path if none is found
# within the hook timeout.
set -u
INPUT="$(cat)"
transcript_path=$(printf '%s' "$INPUT" | jq -r '.transcript_path // ""')
session_title=$(printf '%s'  "$INPUT" | jq -r '.session_title  // ""')
if [ -n "$session_title" ] && [ -n "$transcript_path" ]; then
    mkdir -p "${CLAUDE_PROJECT_DIR:-.}/.attractor" || exit 1
    MARKER="${CLAUDE_PROJECT_DIR:-.}/.attractor/att-${session_title}-transcript.txt"
    PROJECTS="${HOME}/.claude/projects"
    NEEDLE="\"customTitle\":\"${session_title}\""

    # Poll up to 2.5 s for the JSONL that actually has our session title.
    # -mmin -1 limits the grep to files modified in the last 60 s.
    ACTUAL=""
    for _ in 1 2 3 4 5; do
        sleep 0.5
        ACTUAL=$(find "$PROJECTS" -name "*.jsonl" -mmin -1 \
            -exec grep -l "$NEEDLE" {} \; 2>/dev/null | \
            xargs -r stat -c "%Y %n" 2>/dev/null | sort -rn | head -1 | cut -d' ' -f2-)
        [ -n "$ACTUAL" ] && break
    done

    printf '%s\n' "${ACTUAL:-${transcript_path}}" > "$MARKER"
fi
exit 0
