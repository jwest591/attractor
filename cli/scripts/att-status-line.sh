#!/usr/bin/env bash
# statusLine hook for ClaudeCodeTmuxBackend.
# Writes compact context-usage JSON to $ATTRACTOR_NODE_LOG_DIR/ctx-usage.json (atomic).
# Always prints a one-line status string to stdout for the status bar.
set -u

INPUT="$(cat)"

pct=$(printf '%s' "$INPUT" | jq -r '.context_window.percent // 0' 2>/dev/null || echo 0)
total=$(printf '%s' "$INPUT" | jq -r '.context_window.current_usage.input_tokens // 0' 2>/dev/null || echo 0)
window=$(printf '%s' "$INPUT" | jq -r '.context_window.context_window_size // 0' 2>/dev/null || echo 0)

pct=${pct:-0}
[ "$pct" = "null" ] && pct=0

if [ -n "${ATTRACTOR_NODE_LOG_DIR:-}" ]; then
    tmp="$ATTRACTOR_NODE_LOG_DIR/ctx-usage.json.tmp"
    dest="$ATTRACTOR_NODE_LOG_DIR/ctx-usage.json"
    printf '{"percent":%s,"total_tokens":%s,"context_window":%s}\n' "$pct" "$total" "$window" > "$tmp"
    mv "$tmp" "$dest"
fi

printf "attractor | ctx: %d%%\n" "$pct"
