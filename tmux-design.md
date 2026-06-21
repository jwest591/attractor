# Claude TMUX backend redesign

Goal: Manage claude sessions running in tmux as interactive sessions without user intervention.
Subgoal: Manage context use and avoid auto-compaction.

For a given attractor run, there is a single instance for the backend - maps to a single tmux session,
named after attractor run id.
Each Node of type tmux backend executes in a different claude process - maps to a single tmux window.
When the claude process completes work, it is shutdown and the tmux window is destroyed.
Multiple Nodes can execute in parallel.

## Logging

Each node execution has a separate log directory ("node log dir"): `.attractor/<run_id>/<node_id>-<counter>`
Every node in the graph maps to a unique `node-id`, we also have a monotonically incrementing
`counter` which increments every time *any* node is executed and allows nodes to be run multiple times
(loops occur in the graph) and keeps logs separate for different runs. The counter is an atomic integer
held in `Context` to ensure correctness under parallel node execution. Incrementing the counter and
deriving the node log dir is the first action taken when any node begins execution.

Note: applying this discipline to non-backend nodes is out of scope for this design.

## Use of hooks

All hook logic is implemented via scripts (bash or python) and configured via a single shared settings
file that is a static artifact of the attractor project, versioned and deployed alongside the hook
scripts. Its path is resolved once at attractor startup and passed to each claude invocation via
command line parameter. Hook scripts are referenced by absolute path in the settings file. Per-invocation data (node log dir, context threshold, etc.) is injected as
environment variables on the claude invocation; hook scripts inherit these from the claude process
environment. Because each claude process has its own environment copy, parallel nodes do not interfere.

SessionStart - create a file in node log dir containing transcript path. If the file already exists,
               treat as an error — each window runs exactly one claude process.
statusLine   - write context usage to context file in node log dir.
PreToolUse   - if current context usage (from context file) exceeds configured threshold:
               - if handoff file does not exist: write to `<node_log_dir>/handoff.tmp` then rename to
                 `<node_log_dir>/handoff.md` (atomic POSIX rename), block tool use (exit code 2).
               - if handoff file already exists: return `{"continue": false}` to trigger immediate halt.

## Composition

`ClaudeTmuxBackend` is a single-turn executor. `HandoffAwareBackend` wraps it and owns the handoff
loop. Before each inner `run()` call, `HandoffAwareBackend` increments the atomic counter in
`Context` and derives the node log dir path from run_id + node_id + counter. The inner backend reads
the same counter to create and populate that directory. After `run()` returns, `HandoffAwareBackend`
checks for `handoff.md` in the derived log dir. If present, it reads the file as the new prompt,
removes it, and calls the inner backend again — which increments the counter again, allocating a new
log dir and a new tmux window with a fresh claude process, implicitly clearing context.

## Workflow

Initialisation - a new (named) tmux session is created in detached mode when the backend instance is
constructed. The destructor kills the tmux session, terminating any in-progress windows.

When a tmux claude node is run:
  - Read the current counter from Context (incremented by HandoffAwareBackend before this call) and
    derive the node log dir path: `.attractor/<run_id>/<node_id>-<counter>`.
  - Create the node log dir on disk.
  - Construct the claude command line, injecting `ATTRACTOR_NODE_LOG_DIR` and any other per-node
    env vars, and pointing to the shared settings file containing the hook script configuration.
  - Launch claude in a new tmux window named `<node_id>-<counter>` with the initial prompt.

SessionStart hook fires and writes a file containing the jsonl log path into the node log dir. Each
window runs exactly one claude process, so this file must not already exist when the hook fires —
if it does, the hook should treat it as an error. The backend polls for this file with a 10-second
deadline; if it does not appear, return `Outcome::fail` (graph retry logic handles recovery).

Monitor the jsonl log for an assistant message containing a `stop_reason` field (any value signals
turn completion). A per-node configurable timeout (default 30 minutes) acts as a hard deadline;
expiry is treated as `Outcome::fail`. Process crashes produce no jsonl output and are caught only
by the timeout.

On `stop_reason` detected or timeout: destroy the tmux window and return result to caller
(`HandoffAwareBackend` or the engine directly).
