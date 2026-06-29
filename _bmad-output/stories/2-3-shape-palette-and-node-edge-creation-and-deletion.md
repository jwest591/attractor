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

## Status

review
