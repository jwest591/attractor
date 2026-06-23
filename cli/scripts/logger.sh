#!/usr/bin/env bash
# logger.sh -- Universal Claude Code hook event logger for ClaudeCodeTmuxBackend.
#
# Usage: logger.sh <event-name> [matcher]
# Reads hook JSON on stdin. Writes:
#   $ATTRACTOR_NODE_LOG_DIR/<session_id>.log   per-session detail
#   $ATTRACTOR_NODE_LOG_DIR/_events.log        rolling index
#
# Silently exits 0 if ATTRACTOR_NODE_LOG_DIR is unset (non-pipeline context).
# Concurrent invocations are serialized via mkdir as a POSIX-atomic lock.
# Stale locks (owner PID dead) are reclaimed automatically. Always exits 0
# so a misbehaving logger can never block Claude Code.

set -u

[ -z "${ATTRACTOR_NODE_LOG_DIR:-}" ] && exit 0

EVENT="${1:-UNKNOWN}"
MATCHER="${2:-}"
INPUT="$(cat)"
PID="$$"

extract_session_id() {
    local input="$1"
    if command -v jq >/dev/null 2>&1; then
        printf '%s' "$input" | jq -r '.session_id // empty' 2>/dev/null
        return
    fi
    printf '%s' "$input" | sed -n 's/.*"session_id"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' | head -n1
}

append_locked() {
    local target="$1"
    local lock="${target}.lock.d"
    local tries=0
    local max_tries=1000   # ~10 s at 10 ms per spin

    while ! mkdir "$lock" 2>/dev/null; do
        if [ -f "$lock/pid" ]; then
            local owner
            owner="$(cat "$lock/pid" 2>/dev/null)"
            if [ -n "$owner" ] && ! kill -0 "$owner" 2>/dev/null; then
                rm -rf "$lock" 2>/dev/null
                continue
            fi
        fi
        tries=$((tries + 1))
        if [ $tries -ge $max_tries ]; then
            cat >> "$target" 2>/dev/null
            return 0
        fi
        sleep 0.01
    done

    printf '%s\n' "$PID" > "$lock/pid" 2>/dev/null
    cat >> "$target" 2>/dev/null
    rm -rf "$lock" 2>/dev/null
}

SESSION_ID="$(extract_session_id "$INPUT")"
[ -z "$SESSION_ID" ] && SESSION_ID="no-session-id"

LOG_DIR="$ATTRACTOR_NODE_LOG_DIR"
mkdir -p "$LOG_DIR" 2>/dev/null

LOG="$LOG_DIR/${SESSION_ID}.log"
INDEX="$LOG_DIR/_events.log"

TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

{
    printf '========================================================\n'
    printf 'TS         %s\n'     "$TS"
    printf 'EVENT      %s\n'     "$EVENT"
    [ -n "$MATCHER" ] && printf 'MATCHER    %s\n' "$MATCHER"
    printf 'SESSION_ID %s\n'     "$SESSION_ID"
    printf 'PID        %s\n'     "$PID"
    printf 'CWD        %s\n'     "$PWD"
    printf -- '-- CLAUDE_* env --\n'
    env | grep -E '^(CLAUDE|CLAUDECODE)' | sort
    printf -- '-- stdin --\n'
    if command -v jq >/dev/null 2>&1; then
        printf '%s' "$INPUT" | jq . 2>/dev/null || printf '%s\n' "$INPUT"
    else
        printf '%s\n' "$INPUT"
    fi
    printf '\n'
} | append_locked "$LOG"

printf '%s  %-22s  %-18s  session=%s  pid=%s\n' \
    "$TS" "$EVENT" "${MATCHER:--}" "$SESSION_ID" "$PID" \
    | append_locked "$INDEX"

exit 0
