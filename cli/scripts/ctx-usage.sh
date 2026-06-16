#!/usr/bin/env bash
# ctx-usage.sh -- shared context-usage tracker.
#
# Single source of truth for the context-usage JSON file that other hooks
# (status-line, context-ceiling, anything else that wants to gate behavior on
# context size) read.
#
# Modes:
#   parse-stream    Pipe target for `claude -p ... --output-format stream-json`.
#                   Reads stream-json from stdin, updates the usage file with
#                   the highest input/output token counts seen, passes every
#                   line through to stdout so the caller can still capture the
#                   final result.
#   update          Pipe target inside the interactive statusline hook. Reads
#                   one statusline JSON object on stdin, recomputes the usage
#                   file from .context_window, echoes the input back to stdout
#                   so callers can chain into jq.
#   read            Print "<percent> <total>/<window>" for the current session
#                   (no JSON), e.g. "42 84321/200000". Exit 0 if file present,
#                   exit 1 if not.
#   percent         Print just the integer percent (no trailing newline).
#                   Exits 0 if known, 1 if unknown.
#
# Configuration via env (with defaults):
#   CLAUDE_USAGE_FILE              path; default per-session under
#                                  $CLAUDE_PROJECT_DIR/.claude-session/
#   CLAUDE_CONTEXT_WINDOW          token window; default 200000
#                                  (only consulted in parse-stream; statusline
#                                  always knows the real window)
#   CLAUDE_CONTEXT_THRESHOLD       warn percent; default 75
#   CLAUDE_CONTEXT_CRITICAL        deny percent; default 90
#
# Usage-file shape (consumed by other hooks):
#   {
#     "session_id": "...",
#     "input_tokens": N, "output_tokens": N, "total_tokens": N,
#     "context_window": N,
#     "percent": N,
#     "threshold": N, "critical": N,
#     "exceeded": bool, "critical_exceeded": bool,
#     "source": "stream-json" | "statusline" | "init",
#     "updated": "ISO-8601"
#   }

set -u

MODE="${1:-}"
[ -z "$MODE" ] && { echo "usage: $0 {parse-stream|update|read|percent} [session_id]" >&2; exit 2; }

THRESHOLD="${CLAUDE_CONTEXT_THRESHOLD:-75}"
CRITICAL="${CLAUDE_CONTEXT_CRITICAL:-90}"
DEFAULT_WINDOW="${CLAUDE_CONTEXT_WINDOW:-200000}"
ROOT="${CLAUDE_PROJECT_DIR:-$PWD}"
STATE_DIR="${CLAUDE_STATE_DIR:-$ROOT/.claude-session}"
mkdir -p "$STATE_DIR" 2>/dev/null

# Resolve the usage file path. Prefer explicit env var, then per-session under
# the state dir, then a fallback that won't collide if SESSION_ID is unknown.
usage_file_for() {
    local sid="$1"
    if [ -n "${CLAUDE_USAGE_FILE:-}" ]; then
        printf '%s' "$CLAUDE_USAGE_FILE"
    else
        printf '%s/ctx-usage-%s.json' "$STATE_DIR" "${sid:-no-session}"
    fi
}

write_usage() {
    local file="$1" sid="$2" input="$3" output="$4" window="$5" source="$6"
    local total=$((input + output))
    local percent=0
    [ "$window" -gt 0 ] && percent=$((total * 100 / window))
    local exceeded=false critical_exceeded=false
    [ "$percent" -ge "$THRESHOLD" ] && exceeded=true
    [ "$percent" -ge "$CRITICAL" ] && critical_exceeded=true

    local tmp="${file}.tmp.$$"
    jq -n \
        --arg     sid               "$sid" \
        --argjson input_tokens      "$input" \
        --argjson output_tokens     "$output" \
        --argjson total_tokens      "$total" \
        --argjson context_window    "$window" \
        --argjson percent           "$percent" \
        --argjson threshold         "$THRESHOLD" \
        --argjson critical          "$CRITICAL" \
        --argjson exceeded          "$exceeded" \
        --argjson critical_exceeded "$critical_exceeded" \
        --arg     source            "$source" \
        --arg     updated           "$(date -u +%Y-%m-%dT%H:%M:%SZ)" \
        '{
          session_id:        $sid,
          input_tokens:      $input_tokens,
          output_tokens:     $output_tokens,
          total_tokens:      $total_tokens,
          context_window:    $context_window,
          percent:           $percent,
          threshold:         $threshold,
          critical:          $critical,
          exceeded:          $exceeded,
          critical_exceeded: $critical_exceeded,
          source:            $source,
          updated:           $updated
        }' > "$tmp"
    mv "$tmp" "$file"
}

case "$MODE" in

parse-stream)
    # Headless: read stream-json from stdin, update usage on every event that
    # carries token counts, pass each line through unchanged.
    sid=""
    input_t=0
    output_t=0
    file=""

    while IFS= read -r line; do
        printf '%s\n' "$line"
        [ -z "$line" ] && continue

        # Cheap: only invoke jq when the line looks token-bearing.
        case "$line" in
            *'"session_id"'*|*'"usage"'*) ;;
            *) continue ;;
        esac

        # session_id may appear on early "system" events.
        if [ -z "$sid" ]; then
            new_sid=$(printf '%s' "$line" | jq -r '.session_id // empty' 2>/dev/null)
            if [ -n "$new_sid" ]; then
                sid="$new_sid"
                file=$(usage_file_for "$sid")
            fi
        fi

        # Usage may appear at .usage or .message.usage.
        new_in=$(printf '%s' "$line" | jq -r '
            (.usage.input_tokens // .message.usage.input_tokens // 0) +
            (.usage.cache_creation_input_tokens // .message.usage.cache_creation_input_tokens // 0) +
            (.usage.cache_read_input_tokens // .message.usage.cache_read_input_tokens // 0)
        ' 2>/dev/null)
        new_out=$(printf '%s' "$line" | jq -r '.usage.output_tokens // .message.usage.output_tokens // 0' 2>/dev/null)
        new_in=${new_in:-0}; new_out=${new_out:-0}

        if [ "$new_in" -gt "$input_t" ];  then input_t="$new_in";  fi
        if [ "$new_out" -gt "$output_t" ]; then output_t="$new_out"; fi

        if [ -n "$file" ] && { [ "$new_in" -gt 0 ] || [ "$new_out" -gt 0 ]; }; then
            write_usage "$file" "$sid" "$input_t" "$output_t" "$DEFAULT_WINDOW" "stream-json"
        fi
    done

    # Final write so a zero-token stream still produces a file.
    if [ -n "$file" ]; then
        write_usage "$file" "$sid" "$input_t" "$output_t" "$DEFAULT_WINDOW" "stream-json"
    fi
    ;;

update)
    # Interactive: read statusline JSON on stdin. The statusline payload
    # contains the *real* context_window from Claude -- trust it over
    # CLAUDE_CONTEXT_WINDOW.
    payload="$(cat)"
    printf '%s' "$payload"   # passthrough so caller can re-jq

    sid=$(printf '%s' "$payload" | jq -r '.session_id // "unknown"')
    window=$(printf '%s' "$payload" | jq -r '.context_window.context_window_size // 0')
    [ "$window" = "null" ] && window=0
    [ "$window" -le 0 ] && window="$DEFAULT_WINDOW"

    # current_usage is what's in the active context right now (the value the
    # built-in statusline shows); fall back to total_input + total_output.
    in_t=$(printf '%s' "$payload" | jq -r '
        (.context_window.current_usage.input_tokens // 0) +
        (.context_window.current_usage.cache_creation_input_tokens // 0) +
        (.context_window.current_usage.cache_read_input_tokens // 0)
    ')
    out_t=$(printf '%s' "$payload" | jq -r '.context_window.current_usage.output_tokens // 0')
    if [ "$in_t" = "0" ] && [ "$out_t" = "0" ]; then
        in_t=$(printf '%s' "$payload" | jq -r '.context_window.total_input_tokens // 0')
        out_t=$(printf '%s' "$payload" | jq -r '.context_window.total_output_tokens // 0')
    fi

    file=$(usage_file_for "$sid")
    write_usage "$file" "$sid" "$in_t" "$out_t" "$window" "statusline"
    ;;

init)
    # SessionStart: stdin = SessionStart hook payload. Just create an empty
    # file so other hooks have something to read before the first statusline
    # tick.
    payload="$(cat)"
    sid=$(printf '%s' "$payload" | jq -r '.session_id // "unknown"' 2>/dev/null)
    [ -z "$sid" ] && sid="unknown"
    file=$(usage_file_for "$sid")
    write_usage "$file" "$sid" 0 0 "$DEFAULT_WINDOW" "init"
    printf '%s\n' "$file"
    ;;

read)
    sid="${2:-${CLAUDE_SESSION_ID:-}}"
    file=$(usage_file_for "$sid")
    [ -f "$file" ] || { echo "no usage file: $file" >&2; exit 1; }
    pct=$(jq -r '.percent' "$file")
    tot=$(jq -r '.total_tokens' "$file")
    win=$(jq -r '.context_window' "$file")
    printf '%s %s/%s\n' "$pct" "$tot" "$win"
    ;;

percent)
    sid="${2:-${CLAUDE_SESSION_ID:-}}"
    file=$(usage_file_for "$sid")
    [ -f "$file" ] || exit 1
    jq -r '.percent' "$file"
    ;;

*)
    echo "unknown mode: $MODE" >&2
    exit 2
    ;;
esac
