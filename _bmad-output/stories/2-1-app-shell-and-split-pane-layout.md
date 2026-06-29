---
baseline_commit: 9bf4a543e88e80b782fd7eb24732a0b3be7dd50a
status: done
---

# Story 2.1: App shell and split-pane layout

## Story

As a pipeline author,
I want an app shell with a toolbar and split-pane layout,
So that I have a consistent workspace for the canvas, DOT pane, and property panels.

## Acceptance Criteria

**Given** Story 1.1 is complete
**When** `src/App.tsx` and `src/features/editor/components/EditorLayout.tsx` are created
**Then** the app renders a top-level `Toolbar` with New, Open, Save, Undo, Redo buttons (stubs — no behaviour yet)

**And** below the toolbar, `EditorLayout` renders a horizontal `SplitPane` (`src/shared/components/SplitPane.tsx`): left pane = canvas area (grey placeholder), right pane = stacked DOT text area (placeholder) above property panel area (placeholder)

**And** the split pane is resizable by dragging the divider; both panes maintain minimum widths of 200px

**And** a `LintStatusBar` placeholder renders below the layout showing "0 errors, 0 warnings"

**And** the app renders without console errors in Chrome and Firefox at 1280×800

## Tasks/Subtasks

- [x] Task 1: Create `src/shared/components/SplitPane.tsx` — horizontal resizable split pane with minWidth 200px on each pane
  - [x] 1.1 Implement drag-to-resize divider logic with mouse events
  - [x] 1.2 Enforce 200px minimum widths on both panes
  - [x] 1.3 Accept left/right children as props
- [x] Task 2: Create `src/features/editor/components/Toolbar.tsx` — stub toolbar with New, Open, Save, Undo, Redo buttons
- [x] Task 3: Create `src/features/editor/components/LintStatusBar.tsx` — placeholder showing "0 errors, 0 warnings"
- [x] Task 4: Create `src/features/editor/components/EditorLayout.tsx` — renders SplitPane with canvas placeholder (left) and stacked DOT/property placeholders (right)
- [x] Task 5: Replace `src/App.tsx` default content with Toolbar + EditorLayout + LintStatusBar
- [x] Task 6: `tsc -b` zero errors and `npm test` all pass (no regressions)

## Dev Notes

### Architecture Context

- Tailwind CSS v4 is configured via `@tailwindcss/vite` plugin — use Tailwind utility classes directly
- `#root` is set to `width: 100%; height: 100vh` in `index.css` — the app fills the viewport
- `@xyflow/react` stylesheet is imported in `index.css` — React Flow CSS is already available
- Phase gate: no Phase 2 or Phase 3 imports allowed in Phase 1 feature folders (`src/features/editor/`)
- This story is UI-only: no pipelineStore or connectionStore wiring needed

### Layout Structure

```
<div id="root">
  <App>
    <div class="flex flex-col h-screen">
      <Toolbar />          ← sticky top bar
      <EditorLayout />     ← flex-1, contains SplitPane
      <LintStatusBar />    ← sticky bottom bar
    </div>
  </App>
</div>
```

### SplitPane Notes

- Horizontal split: left pane | divider | right pane
- Divider: 4–6px wide, cursor `col-resize`, `bg-gray-300` or similar
- Drag logic: `onMouseDown` on divider → `mousemove` on `document` → `mouseup` to stop
- State: track left pane width in px; right pane fills remaining space (flex-1)
- Minimum widths: clamp left width so neither pane goes below 200px

### Right Pane Stack

The right pane stacks DOT text area above property panel:
```
<div class="flex flex-col h-full">
  <div class="flex-1 bg-gray-100">DOT text placeholder</div>
  <div class="h-48 bg-gray-200">Property panel placeholder</div>
</div>
```

### Testing

- This story has no logic to unit-test (all stubs/placeholders)
- Verify existing 56 tests still pass after changes
- `tsc -b` must report zero errors

## Dev Agent Record

### Implementation Plan

1. Create SplitPane with drag-resize logic
2. Create Toolbar with 5 stub buttons
3. Create LintStatusBar placeholder
4. Create EditorLayout using SplitPane
5. Replace App.tsx content
6. Run tsc -b and npm test

### Debug Log

(empty)

### Completion Notes

**App.tsx** (~10 lines): replaced default Vite scaffold with flex-column layout composing Toolbar + EditorLayout + LintStatusBar; fills full viewport via `h-screen`.

**SplitPane.tsx** (~45 lines): horizontal split with drag-to-resize divider. Drag logic attaches `mousemove`/`mouseup` to `document` on divider `mousedown`; clamps left width to [200px, containerWidth−200px]. Both panes respect 200px min-width.

**Toolbar.tsx** (~10 lines): stub toolbar with New, Open, Save, Undo, Redo buttons; no behaviour wired yet.

**LintStatusBar.tsx** (~5 lines): static "0 errors, 0 warnings" label in bottom status bar.

**EditorLayout.tsx** (~25 lines): wraps SplitPane; left pane = canvas placeholder (grey), right pane = stacked DOT text placeholder (flex-1) above property panel placeholder (h-48).

`tsc -b`: 0 errors. `npm test`: 56/56 passed (no regressions).

## File List

- `polestar/attractor-webui/src/App.tsx` — replaced
- `polestar/attractor-webui/src/shared/components/SplitPane.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/Toolbar.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/LintStatusBar.tsx` — created
- `polestar/attractor-webui/src/features/editor/components/EditorLayout.tsx` — created
- `polestar/attractor/_bmad-output/stories/2-1-app-shell-and-split-pane-layout.md` — created
- `polestar/attractor/_bmad-output/sprint-status.yaml` — updated

## Change Log

- 2026-06-29: Story implemented; all 5 components created/replaced; 56/56 tests pass; tsc -b clean; status set to review

## Status

done
