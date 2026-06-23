#!/usr/bin/env bash
# statusLine hook for ClaudeCodeTmuxBackend.
# Writes compact context-usage JSON to $ATTRACTOR_NODE_LOG_DIR/ctx-usage.json (atomic).
# Always prints a one-line status string to stdout for the status bar.
set -u

INPUT="$(cat)"

pct_raw=$(printf '%s' "$INPUT" | jq -r '.context_window.used_percentage // 0' 2>/dev/null)
if [ $? -ne 0 ] || [ -z "$pct_raw" ] || [ "$pct_raw" = "null" ]; then
    pct=0
    pct_display="??"
else
    pct="$pct_raw"
    pct_display="$pct_raw"
fi
total_raw=$(printf '%s' "$INPUT" | jq -r '
    ((.context_window.current_usage.input_tokens // 0) +
     (.context_window.current_usage.cache_creation_input_tokens // 0) +
     (.context_window.current_usage.cache_read_input_tokens // 0) +
     (.context_window.current_usage.output_tokens // 0))
' 2>/dev/null)
if [ $? -ne 0 ] || [ -z "$total_raw" ] || [ "$total_raw" = "null" ]; then
    total=0
else
    total="$total_raw"
fi
window_raw=$(printf '%s' "$INPUT" | jq -r '.context_window.context_window_size // 0' 2>/dev/null)
if [ $? -ne 0 ] || [ -z "$window_raw" ] || [ "$window_raw" = "null" ]; then
    window=0
else
    window="$window_raw"
fi

if [ -n "${ATTRACTOR_NODE_LOG_DIR:-}" ]; then
    tmp="$ATTRACTOR_NODE_LOG_DIR/ctx-usage.json.tmp"
    dest="$ATTRACTOR_NODE_LOG_DIR/ctx-usage.json"
    printf '{"percent":%s,"total_tokens":%s,"context_window":%s}\n' "$pct" "$total" "$window" > "$tmp"
    mv "$tmp" "$dest"

    # debug: append a timestamped entry with full hook payload to status-line.log
    TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    {
        printf '%s  ctx=%s%%  tokens=%s/%s\n' "$TS" "$pct_display" "$total" "$window"
        printf '%s' "$INPUT" | jq . 2>/dev/null || printf '%s\n' "$INPUT"
        printf '\n'
    } >> "$ATTRACTOR_NODE_LOG_DIR/status-line.log"
fi

printf "attractor | ctx: %s%%\n" "$pct_display"
