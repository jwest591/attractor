#!/usr/bin/env bash
# att-status-line.sh -- statusLine command for ClaudeCodeTmuxBackend.
# Updates the shared ctx-usage file and outputs a one-line status.
set -u

HOOK_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INPUT="$(cat)"

# Update the shared usage file (discards the echoed JSON).
printf '%s' "$INPUT" | "$HOOK_DIR/ctx-usage.sh" update >/dev/null

# Extract percent for the visible status line.
sid=$(printf '%s' "$INPUT" | jq -r '.session_id // "unknown"')
usage_file="${CLAUDE_STATE_DIR:-${CLAUDE_PROJECT_DIR:-$PWD}/.claude-session}/ctx-usage-${sid}.json"
pct=0
[ -f "$usage_file" ] && pct=$(jq -r '.percent // 0' "$usage_file" 2>/dev/null || echo 0)

printf "attractor | ctx: %d%%\n" "$pct"
