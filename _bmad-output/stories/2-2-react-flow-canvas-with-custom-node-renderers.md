---
baseline_commit: 60d2e3d0c9ac389b4d21f8e17acccd94cf8a0b9f
status: review
---

# Story 2.2: React Flow canvas with custom node renderers

## Story

As a pipeline author,
I want the canvas to render all attractor node shapes and support pan/zoom,
So that I can see the visual pipeline graph and navigate it freely.

## Acceptance Criteria

**Given** Story 1.3 pipelineStore is in place and Story 2.1 shell exists
**When** `src/features/editor/components/GraphCanvas.tsx` wraps `<ReactFlow>` reading nodes and edges from `pipelineStore`
**Then** all 7 custom node renderers exist in `src/features/editor/components/nodes/`: `StartNode` (Mdiamond), `ExitNode` (Msquare), `BoxNode` (box/codergen), `HexagonNode`, `ParallelogramNode`, `ComponentNode`, `TripleoctagonNode`

**And** `nodes/index.ts` exports a `nodeTypes` registry object used as the `nodeTypes` prop on `<ReactFlow>`

**And** pipelineStore is seeded with one start node and one exit node so the canvas renders non-empty on first load

**And** pan (drag background) and zoom (scroll/pinch) work natively via React Flow

**And** a "Fit view" button calls React Flow's `fitView()` to fit all nodes in the viewport (FR-5 zoom reset)

**And** the canvas handles 100 nodes without perceptible lag on pan and zoom (NFR-3)

## Tasks/Subtasks

- [x] Task 1: Seed pipelineStore with initial start and exit nodes
  - [x] 1.1 Add initial `nodes` (Mdiamond start + Msquare exit) and `edges` (start→exit) to pipelineStore default state
- [x] Task 2: Create 7 custom node renderer components in `src/features/editor/components/nodes/`
  - [x] 2.1 `StartNode.tsx` — Mdiamond diamond shape
  - [x] 2.2 `ExitNode.tsx` — Msquare double-bordered square
  - [x] 2.3 `BoxNode.tsx` — box simple rectangle
  - [x] 2.4 `HexagonNode.tsx` — hexagon shape
  - [x] 2.5 `ParallelogramNode.tsx` — parallelogram shape
  - [x] 2.6 `ComponentNode.tsx` — UML component rectangle with side protrusions
  - [x] 2.7 `TripleoctagonNode.tsx` — octagon with multiple borders
  - [x] 2.8 `nodes/index.ts` — nodeTypes registry object mapping shape keys to components
- [x] Task 3: Create `src/features/editor/components/GraphCanvas.tsx`
  - [x] 3.1 Read nodes and edges from `pipelineStore`
  - [x] 3.2 Convert PipelineNode/PipelineEdge to React Flow Node/Edge format
  - [x] 3.3 Render `<ReactFlow>` with `nodeTypes` prop and native pan/zoom
  - [x] 3.4 Add "Fit view" button using `useReactFlow().fitView()` inside a `<Panel>`
- [x] Task 4: Wire GraphCanvas into EditorLayout (replace canvas placeholder)
- [x] Task 5: Write tests for nodeTypes registry and initial pipelineStore seed
- [x] Task 6: `tsc -b` zero errors and `npm test` all pass

## Dev Notes

### Architecture Context

- `@xyflow/react` v12.11.1 is installed — use `ReactFlow`, `useNodesState`, `useEdgesState`, `Handle`, `Position`, `Panel`, `useReactFlow` from this package
- `PipelineNode` has no position field — positions are React Flow's responsibility; use default layout (vertical stack) for initial seed
- `pipelineStore` holds the authoritative model; React Flow state holds positions and selection
- For Story 2.2, no write-through back to pipelineStore needed on drag (that is Story 2.3 `useCanvasSync.ts`)
- Phase gate: no Phase 2/3 imports in `src/features/editor/`

### Node Shape Mapping

| PipelineNode.shape | RF node type key | Component |
|---|---|---|
| `Mdiamond` | `startNode` | `StartNode` |
| `Msquare` | `exitNode` | `ExitNode` |
| `box` | `boxNode` | `BoxNode` |
| `hexagon` | `hexagonNode` | `HexagonNode` |
| `parallelogram` | `parallelogramNode` | `ParallelogramNode` |
| `component` | `componentNode` | `ComponentNode` |
| `tripleoctagon` | `tripleoctagonNode` | `TripleoctagonNode` |

### Initial pipelineStore Seed

Add to pipelineStore initial state:
```ts
nodes: [
  { id: 'start', shape: 'Mdiamond', label: 'Start', attributes: {} },
  { id: 'exit',  shape: 'Msquare',  label: 'Exit',  attributes: {} },
],
edges: [
  { id: 'e-start-exit', source: 'start', target: 'exit', label: '' }
],
```

### React Flow Node Conversion

Convert `PipelineNode` → RF `Node` by mapping shape to type and assigning default positions:
```ts
const SHAPE_TO_RF_TYPE: Record<string, string> = {
  Mdiamond: 'startNode', Msquare: 'exitNode', box: 'boxNode',
  hexagon: 'hexagonNode', parallelogram: 'parallelogramNode',
  component: 'componentNode', tripleoctagon: 'tripleoctagonNode',
}

{ id, type: SHAPE_TO_RF_TYPE[shape] ?? 'boxNode', data: { label }, position: { x: 200, y: 80 + index * 160 } }
```

### Fit View Button

Use `useReactFlow()` inside the `ReactFlow` tree:
```tsx
function FitViewButton() {
  const { fitView } = useReactFlow()
  return (
    <Panel position="top-right">
      <button onClick={() => fitView({ padding: 0.2 })}>Fit view</button>
    </Panel>
  )
}
```

### Testing Strategy

- Tests live in `src/features/editor/components/nodes/nodeTypes.test.ts`
- Test 1: `nodeTypes` registry has all 7 required keys
- Test 2: pipelineStore initial nodes contain exactly one Mdiamond and one Msquare
- No component render tests needed (no @testing-library/react installed)
- The existing 56 tests must continue to pass

### Known Constraints

- The pipelineStore test `beforeEach` resets state to empty, so seeded initial nodes don't interfere
- `ReactFlow` must be wrapped in a `div` with explicit width/height (use `w-full h-full`)
- `Background` and `Controls` from `@xyflow/react` can be added optionally for better UX

## Dev Agent Record

### Implementation Plan

1. Seed pipelineStore initial state
2. Create 7 node renderer components + index.ts
3. Create GraphCanvas.tsx
4. Update EditorLayout.tsx
5. Write tests
6. Run tsc -b and npm test

### Debug Log

- `useNodesState`/`useEdgesState` do not accept factory functions (only plain arrays) — fixed by calling `usePipelineStore.getState()` directly and mapping immediately.

### Completion Notes

**pipelineStore.ts**: seeded initial state with `start` (Mdiamond) and `exit` (Msquare) nodes plus one connecting edge `e-start-exit`. Existing tests unaffected because `beforeEach` resets state.

**7 custom node renderers** (`StartNode`, `ExitNode`, `BoxNode`, `HexagonNode`, `ParallelogramNode`, `ComponentNode`, `TripleoctagonNode`): each renders a `Handle` pair (target top, source bottom) and a visually distinct shape using CSS `clip-path` or border styling. No external dependencies beyond `@xyflow/react`.

**nodes/index.ts**: exports `nodeTypes` (NodeTypes registry) and `SHAPE_TO_RF_TYPE` (shape string → RF type key map).

**GraphCanvas.tsx**: reads initial model from `pipelineStore.getState()`, converts to RF `Node`/`Edge` arrays with `useNodesState`/`useEdgesState`, renders `<ReactFlow>` with custom nodeTypes, `Background` (dots), and a `FitViewButton` panel using `useReactFlow().fitView()`. `ReactFlowProvider` is supplied by the parent `EditorLayout`.

**EditorLayout.tsx**: replaced `CanvasPlaceholder` with `<ReactFlowProvider><GraphCanvas /></ReactFlowProvider>`. Right pane placeholders unchanged.

**nodeTypes.test.ts**: 11 new tests — 3 for registry shape, 2 for SHAPE_TO_RF_TYPE, 5 for pipelineStore initial seed.

`tsc -b`: 0 errors. `npm test`: 67/67 passed (56 original + 11 new).

## File List

- `polestar/attractor-webui/src/shared/store/pipelineStore.ts` — modified (initial seed nodes/edges)
- `polestar/attractor-webui/src/features/editor/components/nodes/StartNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/ExitNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/BoxNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/HexagonNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/ParallelogramNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/ComponentNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/TripleoctagonNode.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/nodes/index.ts` — created
- `polestar/attractor-webui/src/features/editor/components/GraphCanvas.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/EditorLayout.tsx` — modified
- `polestar/attractor-webui/src/features/editor/components/nodes/nodeTypes.test.ts` — created
- `polestar/attractor/_bmad-output/stories/2-2-react-flow-canvas-with-custom-node-renderers.md` — created
- `polestar/attractor/_bmad-output/sprint-status.yaml` — updated

## Change Log

- 2026-06-29: Story implemented; 7 node renderers + GraphCanvas + EditorLayout wiring; 11 new tests; 67/67 pass; tsc -b clean; status → review

## Status

review
