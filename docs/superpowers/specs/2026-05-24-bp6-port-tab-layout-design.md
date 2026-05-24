# BP6 Port-Tab Layout Reorganization — Design Spec

**Date:** 2026-05-24
**Status:** Approved (design), pending implementation plan
**Area:** Enclosure modeller — Port tab (BP6 only)

## Problem

For a 6th-order bandpass (BP6) the Port tab is cramped vertically: the rear-port
controls occupy three side-by-side columns (controls | dimensions+diagram+bracing |
results) and the front-port section is stacked **below** them, making the tab very tall.
The arrangement also doesn't match the chamber diagram (rear-left / front-right), so the
spatial mapping between controls and the physical layout isn't obvious.

## Goal

Reorganize the BP6 Port tab into **three columns that mirror the chamber diagram**:

- **Left — REAR PORT:** rear inputs (fb, QL, # ports, shape, flare, inner Ø/dims, wall
  thickness, extends-into-box + slider, bracing surface) **then below them** the rear
  results (port area, inner surface area, port air volume, vol-in-box, port length,
  per-port, 2nd harmonic).
- **Middle — CHAMBER:** the chamber diagram only.
- **Right — FRONT PORT:** front inputs (same set as rear) **then below them** the front
  results (full parity — same 7 rows as the rear).

This uses the horizontal space the divider removal freed and gives much more vertical room.
**Vented and BP4 keep their current Port-tab layout, unchanged.**

## Decisions (from brainstorming)

- **Three columns**, not two: rear (inputs→results) | diagram | front (inputs→results).
- **Diagram in the middle column**, by itself.
- **Front-results parity:** the front currently shows only 4 results
  (`m_portFrontAreaLbl`, `m_portFrontSurfAreaLbl`, `m_portFrontVolDisplLbl`,
  `m_portFrontLenLbl`). Add the 3 missing ones so the front matches the rear's 7:
  **port air volume**, **per-port length**, **2nd harmonic**.
- **BP6 only.** Vented/BP4 layout is untouched.
- **Reparent existing widgets** (chosen over a QStackedWidget with duplicated controls,
  which would require syncing two copies of state/signals).

## Architecture

### Part 1 — Front-results parity (computation)
Front-port results are already computed and updated in the Port-length refresh
(`updatePortLength` in `enclosurewidget.cpp`) — that is why changing front fb updates the
front length today. Add three front labels and compute their values there, mirroring the
existing **rear** formulas applied to the front geometry/tuning (`portFront*`,
`volumeFront_L`, `fbFront`, `QLFront`, `numPortsFront`):

- `m_portFrontVolInnerLbl` — port air volume (bore volume, all front ports).
- a front per-port length line (the front equivalent of the rear `(per port)` row).
- `m_portFrontF2HLbl` — 2nd pipe harmonic of the front port.

Use the same helper math the rear path uses; do not invent new acoustics. These values are
deterministic functions of the model — the computation can be exercised by a small unit
test if the math is factored into a pure helper; otherwise they are validated in UAT.

### Part 2 — BP6 layout (reparenting)
Promote the Port-tab container blocks to members so they can be re-homed:

- rear controls block (today's col1), rear dimensions block (today's col2 **minus** the
  diagram), bracing-surface row, rear results grid (today's col3), the diagram
  (`m_portDiagram`), and the front section (`m_frontPortSection`, already a member).

Maintain **two arrangements** of these same widgets:

- **Standard (vented/BP4):** the existing arrangement — controls | dims+diagram+bracing |
  results side-by-side, front section below. Unchanged from today.
- **BP6:** a three-column row — **left** = rear controls + rear dims + bracing + rear
  results stacked vertically; **middle** = the diagram; **right** = the front section
  (inputs + its results).

Switch arrangement in the existing enclosure-type update path (where
`m_frontPortSection->setVisible(bp6)` and the section-header relabeling already happen —
`enclosurewidget.cpp` ~3678/3684 and ~4294/4300). When the active model is BP6, move the
blocks into the 3-column arrangement; otherwise restore the standard arrangement. Reparenting
moves each block's child widgets with it, so all existing signal/slot wiring is preserved.

### Constraints / risk
- Reparenting is the regression-prone part. The implementation plan must pin down the exact
  container widgets, the two parent layouts, and ensure the switch is idempotent (safe to
  call repeatedly on every model change) and leaves no widget orphaned or double-parented.
- The diagram must end up in exactly one place at a time (middle column for BP6, inside the
  dims block for standard) — never both.
- No change to vented/BP4 visual layout.

## Data model
No `BoxModel` changes. All inputs already exist (rear `port*`, front `portFront*`). Part 1
adds only UI labels + computed display values, not stored fields.

## Testing
- **Front-results computation:** if the per-port length / port-air-volume / 2nd-harmonic
  math is factored into a pure helper, assert it in the existing `tests/portphysics_tests.cpp`
  harness against known inputs. If it stays inline in `updatePortLength`, it is covered by UAT.
- **Layout:** visual UAT — switch a model Vented ↔ BP4 ↔ BP6 repeatedly and confirm:
  - BP6 shows the 3-column rear|diagram|front arrangement with results under each port;
  - front shows all 7 result rows matching the rear;
  - Vented and BP4 look exactly as before (no regression);
  - no orphaned/duplicated widgets, no clipping, correct in light & dark themes.

## Out of scope
- Vented/BP4 layout changes.
- Bandpass port-velocity (separate deferred follow-up).
- Any new acoustic model or stored field.
- The chamber diagram's internals (already built; only its placement changes).
