#!/usr/bin/env bash
# context-ceiling.sh — PreToolUse hook that denies tool calls once context
# usage crosses the critical threshold. Reads the shared usage file written
# by ctx-usage.sh (via status-line.sh in interactive mode, or the headless
# stream-json pipe).
#
# Behavior:
#   - usage file missing OR percent < critical  -> allow (echo {})
#   - percent >= critical                       -> deny with permissionDecision
#
# Bypass: set CLAUDE_BYPASS_CEILING=1 to skip the check (use this for the
# headless "summarizer" you spawn at handoff time so it can still write the
# handoff file).
#
# Hook contract:
#   stdin:  { "session_id": "...", "tool_name": "...", "tool_input": {...}, ... }
#   stdout: PreToolUse hookSpecificOutput JSON; exit 0 always.

set -u

if [ "${CLAUDE_BYPASS_CEILING:-0}" = "1" ]; then
    echo '{}'
    exit 0
fi

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CRITICAL="${CLAUDE_CONTEXT_CRITICAL:-90}"
ROOT="${CLAUDE_PROJECT_DIR:-$PWD}"
STATE_DIR="${CLAUDE_STATE_DIR:-$ROOT/.claude-session}"

INPUT="$(cat)"
sid=$(printf '%s' "$INPUT" | jq -r '.session_id // "unknown"' 2>/dev/null)
tool=$(printf '%s' "$INPUT" | jq -r '.tool_name // "unknown"' 2>/dev/null)

if [ -n "${CLAUDE_USAGE_FILE:-}" ]; then
    usage_file="$CLAUDE_USAGE_FILE"
else
    usage_file="$STATE_DIR/ctx-usage-${sid}.json"
fi

if [ ! -f "$usage_file" ]; then
    echo '{}'
    exit 0
fi

percent=$(jq -r '.percent // 0' "$usage_file" 2>/dev/null)
percent=${percent:-0}
[ "$percent" = "null" ] && percent=0
case "$percent" in
    ''|*[!0-9]*) percent=0 ;;
esac

if [ "$percent" -lt "$CRITICAL" ]; then
    echo '{}'
    exit 0
fi

# Deny — produce a message Claude will see verbatim. Keep it actionable:
# tell Claude to wrap up and stop, since we can't force-stop from PreToolUse.
handoff_note=""
if [ -n "${CLAUDE_HANDOFF_FILE:-}" ]; then
    handoff_note=" Write your handoff summary to $CLAUDE_HANDOFF_FILE then end your turn."
fi
reason="Context ceiling reached (${percent}% >= ${CRITICAL}%). All further tool calls are denied for the rest of this session. Stop work now: produce a handoff summary describing (1) what was accomplished, (2) what remains, (3) the next concrete step, then end your turn.${handoff_note} Do not retry tool calls."

jq -n --arg r "$reason" '{
  hookSpecificOutput: {
    hookEventName: "PreToolUse",
    permissionDecision: "deny",
    permissionDecisionReason: $r
  }
}'
