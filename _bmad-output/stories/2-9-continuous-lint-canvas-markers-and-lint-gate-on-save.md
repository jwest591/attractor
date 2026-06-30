---
baseline_commit: 89ca7c3766cd37965507cf64609c673ab8413460
---

# Story 2-9: Continuous lint, canvas markers, and lint gate on save

## Story

As a pipeline author,
I want lint violations shown as inline markers on the canvas and text pane, with a live summary bar and a save warning for errors,
So that I catch structural issues before saving or running.

## Acceptance Criteria

**Given** Story 1.6 (lintEngine) and Stories 2.2-2.6 (canvas + text pane) are complete
**When** `src/features/editor/hooks/useLintEngine.ts` subscribes to `pipelineStore` and calls `lint()` after every model change
**Then** ERROR-level violations appear as red markers on the relevant canvas nodes or edges; WARNING-level as yellow markers (FR-17)

**And** `LintStatusBar.tsx` updates in real time showing the count of errors and warnings (FR-17)

**And** each lint violation produces a CodeMirror diagnostic (squiggle) in the DOT text pane at the relevant line, matching the canvas marker (FR-12)

**And** a collapsible validation summary lists all active violations with their messages (FR-17)

**And** when the user clicks Save with one or more ERROR-level violations active, a warning dialog names the blocking violations and requires explicit confirmation before the save proceeds (FR-18 - save side)

**And** the user can cancel the save from the warning dialog and return to the editor

## Tasks / Subtasks

- [x] Task 1: Verify `useLintEngine.ts` subscribes to `pipelineStore` and updates `lintStore` on every model change
  - [x] Confirm hook exists at `src/features/editor/hooks/useLintEngine.ts`
  - [x] Confirm hook is mounted in `App.tsx`
  - [x] Write unit tests for `useLintEngine` core logic (`useLintEngine.test.ts`)

- [x] Task 2: Canvas lint markers (red/yellow node outlines) via `GraphCanvas.tsx`
  - [x] Confirm `nodeSeverityMap` built from `lintStore.diagnostics`
  - [x] Confirm node styles updated with red/yellow outlines via `useEffect`

- [x] Task 3: `LintStatusBar.tsx` real-time summary with collapsible violation list
  - [x] Confirm error/warning counts displayed in real time
  - [x] Confirm collapsible expansion shows per-violation messages

- [x] Task 4: CodeMirror squiggles in DOT text pane
  - [x] Confirm `lintDecorPlugin` in `DotTextPane.tsx` renders wavy underlines

- [x] Task 5: Lint gate on save with confirmation dialog
  - [x] Confirm `handleSave` in `useFileOperations.ts` reads `lintStore` errors and defers save
  - [x] Confirm warning dialog rendered in `Toolbar.tsx` with Cancel / Save Anyway
  - [x] Write lint gate precondition tests in `useFileOperations.test.ts`

## Dev Notes

All features were implemented as part of earlier stories (2-4 through 2-8). This story verifies and formalises the implementation with dedicated tests.

**Key files:**
- `src/features/editor/hooks/useLintEngine.ts` — subscribes to pipelineStore, calls lint(), updates lintStore
- `src/features/editor/store/lintStore.ts` — Zustand store holding `LintDiagnostic[]`
- `src/features/editor/components/LintStatusBar.tsx` — collapsible bar, rendered in App.tsx
- `src/features/editor/components/GraphCanvas.tsx` — `nodeSeverityMap` + useEffect sets node outline styles
- `src/features/editor/components/DotTextPane.tsx` — `lintDecorPlugin` renders wavy underlines
- `src/features/editor/hooks/useFileOperations.ts` — `handleSave` checks lintStore before calling `savePipelineFile`
- `src/features/editor/components/Toolbar.tsx` — renders `lintWarning` confirmation dialog

## Dev Agent Record

### Implementation Plan

Story 2-9 features already existed from pipeline story runs. This execution adds:
1. `useLintEngine.test.ts` — unit tests for the lint→store pipeline
2. Additional tests in `useFileOperations.test.ts` — lint gate precondition coverage

### Debug Log

N/A — no bugs encountered, implementation pre-existing.

### Completion Notes

- All 6 ACs verified against existing implementation
- Added `useLintEngine.test.ts` with 5 tests covering lint→lintStore integration
- Added 3 lint gate tests to `useFileOperations.test.ts`
- Full test suite passes (150+ tests)

## File List

### New Files
- `src/features/editor/hooks/useLintEngine.test.ts`

### Modified Files
- `src/features/editor/hooks/useFileOperations.test.ts`

## Change Log

- 2026-06-30: Verified pre-existing implementation satisfies all ACs; added unit tests for useLintEngine and lint gate behavior

## Status

review
