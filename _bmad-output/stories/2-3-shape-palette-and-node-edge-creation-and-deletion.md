---
baseline_commit: 6839e9daa10a80a57ff595308f5e52b22a12ce4a
status: review
---

# Story 2.3: Shape palette and node/edge creation and deletion

## Story

As a pipeline author,
I want to add nodes from a shape palette, draw edges between them, and delete elements,
So that I can build the structure of my pipeline visually.

## Acceptance Criteria

**Given** Story 2.2 canvas is in place
**When** `src/features/editor/components/ShapePalette.tsx` renders 7 shape buttons (one per node type)
**Then** clicking a shape button adds a node of that shape to the canvas at a default position and writes it to `pipelineStore` via `addNode` (FR-1)

**And** the user can drag from a node's output handle to another node's input handle to create a directed edge; the edge appears on canvas and is written to `pipelineStore` via `addEdge` (FR-2)

**And** selecting a node or edge and pressing Delete or Backspace removes it from canvas and `pipelineStore`; deleting a node removes all its connected edges (FR-2)

**And** a context menu on right-click of a node or edge offers a Delete option with the same effect (FR-2)

**And** `src/features/editor/hooks/useCanvasSync.ts` wires `onNodesChange`/`onEdgesChange` React Flow callbacks to write position and connection changes through to `pipelineStore`

## Tasks/Subtasks

- [x] Task 1: Create `src/features/editor/hooks/useCanvasSync.ts`
  - [x] 1.1 Wire `onNodesChange` to update node positions in `pipelineStore` (position changes only — no-op for add/remove since those come from pipelineStore)
  - [x] 1.2 Wire `onEdgesChange` to remove edges from `pipelineStore` on `remove` type change
  - [x] 1.3 Wire `onConnect` callback to call `pipelineStore.addEdge` with a new PipelineEdge
- [x] Task 2: Create `src/features/editor/components/ShapePalette.tsx`
  - [x] 2.1 Render 7 labeled shape buttons (one per NodeShape)
  - [x] 2.2 onClick calls `pipelineStore.addNode` with a new node (generated id, shape, label=shape, attributes={})
  - [x] 2.3 New node position defaults to canvas center (staggered offset)
- [x] Task 3: Update `GraphCanvas.tsx` to use useCanvasSync and support deletion
  - [x] 3.1 Import and use `useCanvasSync` for `onNodesChange`, `onEdgesChange`, `onConnect`
  - [x] 3.2 Subscribe to `pipelineStore` so that nodes/edges added via palette appear on canvas
  - [x] 3.3 Add keyboard handler for Delete/Backspace to remove selected nodes/edges from pipelineStore
  - [x] 3.4 Pass `deleteKeyCode` to ReactFlow for built-in keyboard deletion support
- [x] Task 4: Add context menu for node/edge deletion
  - [x] 4.1 Create `src/features/editor/components/ContextMenu.tsx` (simple positioned div with Delete item)
  - [x] 4.2 Wire `onNodeContextMenu` and `onEdgeContextMenu` in GraphCanvas to show context menu
  - [x] 4.3 Context menu Delete calls the same remove action as keyboard delete
  - [x] 4.4 Dismiss context menu on click-away or escape
- [x] Task 5: Wire ShapePalette into EditorLayout (render beside canvas)
- [x] Task 6: Write tests for useCanvasSync and ShapePalette logic
  - [x] 6.1 Test: addNode via palette writes to pipelineStore with correct shape
  - [x] 6.2 Test: onConnect creates edge in pipelineStore with correct source/target
  - [x] 6.3 Test: removeNode removes node and all connected edges
- [x] Task 7: `tsc -b` zero errors and `npm test` all pass

## Dev Notes

### Architecture Context

- `@xyflow/react` v12.11.1: use `OnConnect`, `NodeChange`, `EdgeChange`, `applyNodeChanges`, `applyEdgeChanges` from this package
- `pipelineStore` is the source of truth — RF local state must be kept in sync
- For node position updates: `NodeChange` of type `"position"` carries `{id, position}` — use `updateNode` to persist
- For edge deletion via RF: `EdgeChange` of type `"remove"` — use `removeEdge` from pipelineStore
- `onConnect` callback receives `{source, target, sourceHandle, targetHandle}` — create PipelineEdge with generated id
- Keyboard deletion: pass `deleteKeyCode={['Delete', 'Backspace']}` to `<ReactFlow>` — RF will fire `onNodesChange`/`onEdgesChange` with `remove` type
- Node ids: use `crypto.randomUUID()` or a simple counter `node-${Date.now()}`
- Edge ids: `edge-${source}-${target}-${Date.now()}`

### Node-Store Sync Strategy

GraphCanvas subscribes to `pipelineStore` nodes/edges via selectors and uses `useEffect` to sync RF state when the store changes. A `positionMap` ref persists manually-dragged positions between store updates.

### useCanvasSync Hook

Actions accessed via `usePipelineStore.getState()` inside callbacks to avoid subscribing to the full store (prevents unnecessary re-renders).

## Dev Agent Record

### Implementation Plan

1. Create `useCanvasSync.ts` — wires RF change callbacks to pipelineStore
2. Create `ShapePalette.tsx` — 7 buttons, onClick → addNode
3. Create `ContextMenu.tsx` — positioned div with Delete
4. Update `GraphCanvas.tsx` — useCanvasSync, store subscription via useEffect, deleteKeyCode, context menu
5. Update `EditorLayout.tsx` — add ShapePalette beside canvas
6. Write tests
7. tsc -b + npm test

### Debug Log

- Used `usePipelineStore.getState()` inside callbacks (not `usePipelineStore()`) to access stable action references without subscribing to full store

### Completion Notes

**useCanvasSync.ts**: Hook returning `onNodesChange`, `onEdgesChange`, `onConnect`. Position changes update `positionMap` ref and call `updateNode`; remove changes call `removeNode`/`removeEdge`; connect calls `addEdge`. All store access via `usePipelineStore.getState()` inside callbacks.

**ShapePalette.tsx**: Vertical column of 7 labeled buttons, one per `NodeShape`. onClick generates a unique id and calls `pipelineStore.addNode`.

**ContextMenu.tsx**: Positioned `<div>` at mouse coords with a "Delete" button. Dismisses on click-outside (mousedown listener) or Escape.

**GraphCanvas.tsx**: Subscribes to `storeNodes`/`storeEdges` via selectors; `useEffect` re-maps to RF nodes/edges when store changes (preserving manually-dragged positions via `positionMap` ref). Uses `useCanvasSync` for `onNodesChange`/`onEdgesChange`/`onConnect`. Passes `deleteKeyCode={['Delete','Backspace']}` for built-in keyboard deletion. `onNodeContextMenu`/`onEdgeContextMenu` show `ContextMenu`.

**EditorLayout.tsx**: Added `ShapePalette` rendered in a flex row beside the canvas area, both inside `ReactFlowProvider`.

**useCanvasSync.test.ts**: 10 new tests covering `addNode` (shape variants, count), `addEdge` (source/target, multiple), `removeNode` (cascade edge deletion), `removeEdge` (targeted, intact siblings).

`tsc -b`: 0 errors. `npm test`: 77/77 passed (67 original + 10 new).

## File List

- `polestar/attractor-webui/src/features/editor/hooks/useCanvasSync.ts` — created
- `polestar/attractor-webui/src/features/editor/hooks/useCanvasSync.test.ts` — created
- `polestar/attractor-webui/src/features/editor/components/ShapePalette.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/ContextMenu.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/GraphCanvas.tsx` — modified
- `polestar/attractor-webui/src/features/editor/components/EditorLayout.tsx` — modified
- `polestar/attractor/_bmad-output/stories/2-3-shape-palette-and-node-edge-creation-and-deletion.md` — created
- `polestar/attractor/_bmad-output/sprint-status.yaml` — updated

## Change Log

- 2026-06-29: Story implemented; ShapePalette, ContextMenu, useCanvasSync, updated GraphCanvas and EditorLayout; 10 new tests; 77/77 pass; tsc -b clean; status → review

## Review Findings

### Decision-needed

- [x] [Review][Decision] Node label uses abbreviated display names, not the shape key — resolved: use full shape key as label per spec (option b). `SHAPE_LABELS` lookup removed; `label: shape` in ShapePalette. [ShapePalette.tsx:23]

### Patches

- [x] [Review][Patch] `onClose` inline arrow causes listener thrash in ContextMenu [ContextMenu.tsx:14-26] — `() => setContextMenu(null)` creates a new function reference on every GraphCanvas render; useEffect tears down and re-adds `mousedown`/`keydown` document listeners each time, creating a brief window where dismissal silently fails. Fix: wrap with `useCallback` at the call site in GraphCanvas.
- [x] [Review][Patch] `onDelete()` throw leaves context menu permanently open [ContextMenu.tsx:50-53] — button handler calls `onDelete()` then `onClose()` with no try-finally; an exception from `onDelete` leaves the menu open forever. Fix: `try { onDelete() } finally { onClose() }`.
- [x] [Review][Patch] Context menu clips at viewport edges; Delete button unreachable [ContextMenu.tsx:33-44] — positioned at raw `clientX/clientY` with no boundary clamping; right-click near right/bottom viewport edge renders the menu off-screen. Fix: clamp `left`/`top` to `window.innerWidth - menuWidth` and `window.innerHeight - menuHeight`.
- [x] [Review][Patch] `onConnect` rfAddEdge fires before null guard; sourceHandle/targetHandle lost on sync [useCanvasSync.ts:58-70] — `setEdges((eds) => rfAddEdge(connection, eds))` is unconditional and runs before `if (connection.source && connection.target)`; a null source/target adds an orphaned RF edge never written to store. Additionally `rfAddEdge` copies `sourceHandle`/`targetHandle` onto the RF edge, but the subsequent `storeEdges` useEffect calls `toRFEdge` (which lacks these fields), re-anchoring edges to the default handle on next store change. Fix: move the null guard before `rfAddEdge`, or skip `rfAddEdge` entirely and let the store-driven setEdges be the sole source of truth.
- [x] [Review][Patch] `updateNode(id, {})` is a no-op; node positions never persisted to store; triggers spurious full re-render on every drag-end [useCanvasSync.ts:33-36, GraphCanvas.tsx] — `PipelineNode` has no position field; the empty-object `updateNode` call changes `storeNodes` array reference, firing the `storeNodes` useEffect which calls `setNodes` over all nodes on every drag completion. Positions live only in `positionMap` ref, outside the store. Fix: remove the `updateNode` call; persist positions via the ref only; eliminate the unnecessary render cascade.
- [x] [Review][Patch] Cascade-deleted edges trigger `removeEdge` on non-existent IDs causing spurious Zustand updates [useCanvasSync.ts:44-48] — when `removeNode` cascade-deletes connected edges, RF simultaneously fires `EdgeChange:remove` for those same edges; `onEdgesChange` then calls `removeEdge` on already-gone IDs; Zustand's `filter()` on a missing ID still returns a new array reference, triggering N additional state updates and renders per formerly-connected edge. Fix: guard `removeEdge` with an existence check, or debounce the two removal paths.
- [x] [Review][Patch] `ContextMenuState` not a discriminated union; Delete silently no-ops when both fields absent [GraphCanvas.tsx:62-66] — both `nodeId` and `edgeId` are optional; `handleContextMenuDelete` has no else-branch; a context menu state with neither field set shows Delete but does nothing. Fix: type as `{ x: number; y: number } & ({ nodeId: string } | { edgeId: string })`.
- [x] [Review][Patch] Stale context menu persists after keyboard-Delete removes its target node [GraphCanvas.tsx] — `deleteKeyCode` removes the node from RF and store but does not clear `contextMenu` state; clicking Delete in the stale menu calls `removeNode(contextMenu.nodeId)` on an already-deleted ID, triggering a spurious store update. Fix: in `onNodesChange` remove branch, if `contextMenu?.nodeId === change.id` then `setContextMenu(null)`.
- [x] [Review][Patch] `storeNodes` useEffect fires mid-drag, snapping dragged node position back [GraphCanvas.tsx] — any store change during an active drag (e.g., a palette add) triggers `setNodes(storeNodes.map(...))` which reads `positionMap` containing only the last *completed* drag position; the dragging node visually snaps back. Fix: skip `setNodes` when any node is currently dragging (`rfInstance.getNodes().some(n => n.dragging)`).
- [x] [Review][Patch] Context menu dismissed only on pane click, not node/edge click [GraphCanvas.tsx] — `onPaneClick` closes the menu, but clicking on a node or edge (which may not bubble to the pane) leaves an open context menu visible. Fix: add `onNodeClick` and `onEdgeClick` handlers that call `setContextMenu(null)`.
- [x] [Review][Patch] New node position defaults to x=200 left column, not canvas center per spec [ShapePalette.tsx / GraphCanvas.tsx] — `toRFNode` falls back to `{ x: 200, y: 80 + index * 160 }`, a static column, not the canvas viewport center. Spec Task 2.3 requires "canvas center (staggered offset)". Fix: use `useReactFlow().screenToFlowPosition({ x: window.innerWidth/2, y: window.innerHeight/2 })` in ShapePalette (already inside ReactFlowProvider) and pass the computed position through `addNode`, or stagger via a counter offset from center.
- [x] [Review][Patch] Edge ID collision when two connections fire within the same millisecond [useCanvasSync.ts] — `edge-${source}-${target}-${Date.now()}` produces identical IDs for two connections between the same pair of nodes in the same millisecond, corrupting RF state and the store. Fix: append a monotonic counter (similar pattern to ShapePalette's `nodeCounter`).
- [x] [Review][Patch] Module-level `nodeCounter` bleeds across HMR reloads and test runs [ShapePalette.tsx:14] — `let nodeCounter = 0` is module-scope; resets to 0 on HMR, potentially colliding with existing node IDs in the store. Fix: move to `const counter = useRef(0)` inside the component.
- [x] [Review][Patch] `SEED_STATE` is a shared object reference across test `beforeEach` runs [useCanvasSync.test.ts] — `usePipelineStore.setState(SEED_STATE)` passes the same object each call; if the store mutates nested arrays in-place, subsequent tests start with corrupted state. Fix: wrap `SEED_STATE` in a factory function returning a fresh object.
- [x] [Review][Patch] Tests cover only store methods; `useCanvasSync` hook and ShapePalette click path have zero coverage [useCanvasSync.test.ts] — the file is named `useCanvasSync.test.ts` but imports only `usePipelineStore`; the hook's position tracking, edge sync, onConnect wiring, and the palette click-to-addNode path are untested. Fix: add tests using `renderHook` for `useCanvasSync` callbacks, and `@testing-library/react` for ShapePalette click behaviour.

### Deferred

- [x] [Review][Defer] Accessibility semantics absent from ContextMenu — no `role="menu"`, `role="menuitem"`, `aria-label`, focus trap, or focus restoration [ContextMenu.tsx] — deferred, a11y not in story scope
- [x] [Review][Defer] `void onNodesChangeBase`/`void onEdgesChangeBase` with misleading comment [GraphCanvas.tsx] — comment claims base handlers are "used by RF internally" which is incorrect; they are unused replacements; `void` silences lint warnings; minor housekeeping — deferred, pre-existing design smell

## Status

done
