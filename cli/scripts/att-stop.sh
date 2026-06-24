#!/usr/bin/env bash
# att-stop.sh -- Stop and StopFailure hook for ClaudeCodeTmuxBackend.
# Writes done.json to $ATTRACTOR_NODE_LOG_DIR to signal task completion.
#
# Stop:        writes {"status":"ok","message":"..."} when background_tasks is empty.
#              Fires on every end_turn; skipped when agents are still running.
# StopFailure: writes {"status":"error","error_type":"...","message":"..."} unconditionally.
#
# done.json is written atomically (temp+rename). First writer wins; a second
# hook invocation for the same node_log_dir is a no-op.
#
# Silently exits 0 if ATTRACTOR_NODE_LOG_DIR is unset (non-pipeline context).
# Always exits 0 so a misbehaving hook can never block Claude Code.

set -u

[ -z "${ATTRACTOR_NODE_LOG_DIR:-}" ] && exit 0

INPUT="$(cat)"
EVENT="$(printf '%s' "$INPUT" | jq -r '.hook_event_name // empty')"
TARGET="$ATTRACTOR_NODE_LOG_DIR/done.json"

[ -f "$TARGET" ] && exit 0

write_done() {
    local tmp="${TARGET}.tmp.$$"
    printf '%s\n' "$1" > "$tmp" && mv "$tmp" "$TARGET"
}

case "$EVENT" in
    Stop)
        count="$(printf '%s' "$INPUT" | jq '(.background_tasks // []) | length')"
        [ "$count" != "0" ] && exit 0
        msg="$(printf '%s' "$INPUT" | jq -r '.last_assistant_message // ""')"
        write_done "$(jq -n --arg m "$msg" '{"status":"ok","message":$m}')"
        ;;
    StopFailure)
        etype="$(printf '%s' "$INPUT" | jq -r '.error_type // "unknown"')"
        emsg="$(printf '%s' "$INPUT" | jq -r '.error_message // ""')"
        write_done "$(jq -n --arg t "$etype" --arg m "$emsg" '{"status":"error","error_type":$t,"message":$m}')"
        ;;
esac

exit 0
