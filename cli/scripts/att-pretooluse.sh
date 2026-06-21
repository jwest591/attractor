#!/usr/bin/env bash
# PreToolUse hook for ClaudeCodeTmuxBackend.
# Implements selective-allow context-ceiling detection:
#   - percent < threshold            -> allow (echo {})
#   - percent >= threshold:
#     - tool targets handoff.md      -> allow (let Claude write handoff)
#     - handoff.md absent            -> write stub atomically, deny with ceiling message
#     - handoff.md present           -> {"continue": false} to halt session
set -u

NDL="${ATTRACTOR_NODE_LOG_DIR:-}"
CRITICAL="${ATTRACTOR_CONTEXT_CRITICAL:-85}"

INPUT="$(cat)"
tool_name=$(printf '%s' "$INPUT" | jq -r '.tool_name // ""' 2>/dev/null)
file_path=$(printf '%s' "$INPUT" | jq -r '.tool_input.file_path // ""' 2>/dev/null)

# No log dir or no usage file: allow
if [ -z "$NDL" ] || [ ! -f "$NDL/ctx-usage.json" ]; then
    echo '{}'
    exit 0
fi

pct=$(jq -r '.percent // 0' "$NDL/ctx-usage.json" 2>/dev/null || echo 0)
pct=${pct:-0}
[ "$pct" = "null" ] && pct=0
pct=${pct%%.*}  # truncate decimal part so bash integer comparison works (e.g. 84.7 -> 84)
case "$pct" in ''|*[!0-9]*) pct=0 ;; esac

if [ "$pct" -lt "$CRITICAL" ]; then
    echo '{}'
    exit 0
fi

# Context ceiling reached
handoff="$NDL/handoff.md"

# Normalize file_path to absolute so relative paths from Claude compare correctly
case "$file_path" in
    /*) ;;
    *) [ -n "$file_path" ] && file_path="$(pwd)/$file_path" ;;
esac

# Allow Write, Edit, or MultiEdit targeting handoff.md (let Claude write its handoff content)
if { [ "$tool_name" = "Write" ] || [ "$tool_name" = "Edit" ] || [ "$tool_name" = "MultiEdit" ]; } && \
   [ "$file_path" = "$handoff" ]; then
    echo '{}'
    exit 0
fi

# If handoff.md already exists: halt the session
if [ -f "$handoff" ]; then
    printf '{"continue":false}\n'
    exit 0
fi

# handoff.md does not exist yet: write an empty stub atomically so subsequent hook
# firings know the ceiling has been signalled, then deny the current tool call.
printf '' > "$NDL/handoff.tmp"
mv "$NDL/handoff.tmp" "$handoff"

reason="Context ceiling (${pct}%). Write your handoff summary to $handoff then end your turn."
jq -n --arg r "$reason" '{
  hookSpecificOutput: {
    hookEventName: "PreToolUse",
    permissionDecision: "deny",
    permissionDecisionReason: $r
  }
}'
