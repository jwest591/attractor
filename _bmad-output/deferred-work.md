# Deferred Work

## Deferred from: code review of 1-1-initialise-project-and-install-dependencies (2026-06-28)

- `#root { height: 100vh }` should use `100dvh` for correct mobile viewport height — `src/index.css:14`
- Smoke test placed in `src/shared/types/` — semantically wrong directory for a setup verification test
- Smoke test asserts `expect(true).toBe(true)` — minimal diagnostic value; would not catch misconfigured environment
- No `@testing-library/react` installed — required when component tests are written in later stories
- No `ResizeObserver` polyfill or `setupFiles` configured — jsdom does not implement `ResizeObserver`; required when `@xyflow/react` components are tested
- `@dagrejs/dagre` is an effectively unmaintained community fork — long-term maintenance risk for layout logic
- Inconsistent version pinning strategy: `~6.0.2` for `typescript` vs `^x.y.z` for all others
- No `engines` field specifying minimum Node.js version
- No path aliases (`@/`) configured — will cause deep relative imports across feature directories in later stories

## Deferred from: code review of 1-3-zustand-stores (2026-06-28)

- `addNode`/`addEdge`/`addSubgraph` accept duplicate IDs silently — store-level uniqueness guard omitted; callers responsible; revisit if duplicate IDs surface in canvas or serializer stories
- `addEdge` does not validate `source`/`target` node existence — referential integrity not enforced at store level; DOT import may add edges before nodes; revisit when implementing parser→store wiring
- `setPipelineModel` leaves `syncSource` stale — callers must pair with explicit `setSyncSource(null)` or `setSyncSource(source)` before/after bulk model import
- `setServerUrl` accepts malformed/empty strings — no URL validation; errors surface at connection time; add validation when implementing the connection health-check hook (story 3-1)
- `updateEdge`/`updateSubgraph` have zero test coverage — spec only requires add/remove/syncSource/undo tests; add coverage when these actions gain meaningful callers
- localStorage corrupt shape causes silent TypeError on reload — `persist` has no `migrate` function; add schema migration if `connectionStore` shape changes in future
- `setPipelineModel` called with same-reference arrays records no undo checkpoint — reference-equality guard intentionally skips snapshots; unexpected for callers expecting an explicit undo boundary; document in calling code

## Deferred from: code review of 1-4-dot-parser (2026-06-28)

- **`key=value` inside a subgraph body is silently dropped** [parser.ts:247] — Intentional per dev notes ("Graph-level attributes may be key = value top-level declarations"). Inside a subgraph, a bare `key = value` declaration is consumed by `parseStatement` but the guard `if (!ctx.memberNodeIds)` prevents it from being written to `graphAttrs`, so it is silently dropped. Subgraph attribute scoping relies exclusively on `node [...]` blocks. If future attractor DOT variants need subgraph-level key-value declarations this will need revisiting.

## Deferred from: code review of 1-6-lint-engine (2026-06-28)

- Dangling edge references produce false-positive reachability errors — edges referencing non-existent node IDs are silently dropped from the BFS; a node whose only outgoing edges target ghost IDs will be incorrectly flagged as unreachable [lintEngine.ts:38-41]; out of scope per Dev Notes, revisit when edge-validation rule added
- Duplicate node IDs corrupt BFS reverse-adjacency map — `reverseAdj.set(node.id, [])` silently overwrites predecessors for duplicate IDs [lintEngine.ts:37]; parser should produce consistent models; revisit if duplicates surface in canvas stories
- No forward-reachability check from start node — BFS only answers "can this node reach exit?", not "is this node reachable from start?"; out of scope per spec ("use reverse BFS from the exit node"); candidate for a future §7.2 rule extension
- O(n²) BFS via Array.shift() — `shift()` is O(n); BFS becomes quadratic for large node counts [lintEngine.ts:46]; negligible for UI-scale graphs; fix with head-pointer index if performance becomes a concern
- Test hardcodes shape list instead of importing NODE_SHAPES — `['Mdiamond', 'Msquare', ...]` spelled out in test [lintEngine.test.ts:93]; low risk since VALID_SHAPES derives from NODE_SHAPES at runtime; import the constant for tighter coupling
- No test for empty model (nodes: [], edges: []) — would produce "no start" + "no exit" errors without crashing; trivial boundary case for future test suite expansion

## Deferred from: code review of 1-2-core-typescript-types (2026-06-28)

- W1: `PipelineEdge` missing `attributes` field — DOT edges have attributes; absence creates asymmetry with `PipelineNode`; future breaking change if edge styling/metadata needed [pipeline.ts:27-32]
- W2: `Session.id` typed as `PipelineId` — conflates session and pipeline identity; may be intentional per backend response shape [api.ts:21]
- W3: `PipelineId = string` unbranded type alias — any `string` satisfies it; no compile-time safety against ID mixups [api.ts:1]
- W4: `NodeState`/`NodeArtifacts` structural overlap — both have `nodeId`, `status`, `outcome?`; intentional per spec (serve different API endpoints) [api.ts:9-13, 29-40]
- W5: `LintDiagnostic` missing rule identifier/code — programmatic rule filtering and suppression impossible without a `ruleId` or `code` field [lint.ts:3-10]
- W6: `SseEvent` no pipeline-level events or forward-compat catch-all — future server versions emitting unknown event types will fall through exhaustive switches [api.ts:55]
- W7: `NodeExecutionStatus 'idle'` undefined population semantics — unclear if idle nodes are pre-populated in `PipelineState.nodeStates` for all graph nodes [api.ts:3]
- W8: No referential integrity in `PipelineEdge`/`PipelineSubgraph` node references — `edge.source`, `edge.target`, `subgraph.memberNodeIds` can reference nonexistent node IDs at the type level [pipeline.ts]
- D3 (deferred): `LintDiagnostic` allows `nodeId` + `edgeId` simultaneously; `column` without `line` semantically meaningless — revisit when lint engine is built (story 1-6) [lint.ts:6-9]
- D4 (deferred): `Session.completedAt` optional even for terminal statuses — discriminated union would enforce the invariant; defer until `attractorClient.ts` confirms actual backend response shape [api.ts:21-27]

## Deferred from: code review of 2-3-shape-palette-and-node-edge-creation-and-deletion (2026-06-30)

- Accessibility semantics absent from ContextMenu — no `role="menu"`, `role="menuitem"`, `aria-label`, focus trap, or focus restoration on open/close; a11y concern outside story scope [ContextMenu.tsx]
- `void onNodesChangeBase`/`void onEdgesChangeBase` with misleading comment claiming handlers are "used by RF internally" — they are unused default handlers silenced by `void`; minor housekeeping [GraphCanvas.tsx]

## Deferred from: code review of 2-2-react-flow-canvas-with-custom-node-renderers (2026-06-30)

- Canvas frozen at mount — `getState()` snapshot with no store subscription [GraphCanvas.tsx:57-58]; story 2.3 useCanvasSync introduces proper bidirectional subscription
- Delete-key in ReactFlow removes nodes/edges from local RF state without updating pipelineStore [GraphCanvas.tsx]; write-back deferred to story 2.3
- Test `beforeEach` hardcodes seed values via `setState` instead of reading real initial state [nodeTypes.test.ts:67-77]; test quality improvement
- TripleoctagonNode nested clipPath+padding rendering may vary across browsers [TripleoctagonNode.tsx:11-21]; visual cosmetic
- ComponentNode tab positions are hardcoded magic numbers independent of node height [ComponentNode.tsx:36,44]; maintainability
- nodeTypes.test.ts intermingles registry tests with pipelineStore seed tests; test organisation
- Handle connection points float from visible boundary on clipPath shapes (hexagon, parallelogram); visual cosmetic; fix requires shape-specific handle offsets
- Empty-model call via `setPipelineModel` would wipe seed nodes silently; relevant when story 2.7 file-ops lands
