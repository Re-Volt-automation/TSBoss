# Bandpass Chamber Diagram — Design Spec

**Date:** 2026-05-24
**Status:** Approved (design), pending implementation plan
**Area:** Enclosure modeller — schematic illustrations (extends the diagram toolkit)

## Problem

The blueprint port diagram (`drawPortSection`, shipped on `feat/diagram-toolkit`) is shown
for *all* ported configs, but it only depicts the single rear/main port (`port*` fields).
Bandpass enclosures don't map to one port:

- **BP6** = two vented chambers → **two ports** (rear `port*` + front `portFront*`). The
  diagram shows only the rear, missing the front.
- **BP4** = sealed rear + vented front → the real port is the **front** (`portFront*`);
  drawing the rear `port*` is misleading (the rear has no port).

Surfaced in UAT: on BP6 the diagram "misses the other port," and the always-on 120 px
port section doesn't fit the control-heavy bandpass Port tab.

## Goal

Add a bandpass-specific schematic — an **integrated chamber view** — and dispatch to it for
BP4/BP6, while plain **Vented keeps the detailed single-port cross-section** (`drawPortSection`,
unchanged). Built entirely on the existing `DiagramPainter`; no new style primitives.

## Decisions (from brainstorming)

- **Layout: integrated chamber view** (not two side-by-side port sections, not stacked).
  Chosen as the most honest depiction; it is landscape, so it fits the existing ~120 px
  `DiagramView` height — which also resolves the "not fitting" complaint.
- **Vented unchanged** — `drawPortSection` still serves plain Vented; only BP4/BP6 use the
  chamber view.
- **Schematic, fixed proportions** (consistent with the toolkit): chamber boxes and ports
  are canonical, not scaled to entered dimensions; values live in labels.
- **No physics recompute** — chamber labels show raw fields (volumes, port face size); the
  precise length/area stay in the results column.

## What the chamber view draws

`drawBandpassSection(DiagramPainter &d, const BoxModel &m)`:

- **Enclosure**: outer box outline + a central vertical divider (both via the outline pen).
- **Driver**: a small cone shape mounted on the divider (drawn with the outline pen / `wall`).
- **Rear chamber** (left, volume `volumeL`):
  - **BP6**: a port opening through the left wall using the rear `port*` fields, with a blue
    `airflow` arrow.
  - **BP4**: no port; a faint hatch wash fills the rear chamber and the label reads "SEALED".
- **Front chamber** (right, volume `volumeFront_L`): a port opening through the right wall
  using the `portFront*` fields, with a blue `airflow` arrow.
- **Labels** (uppercase, via `DiagramPainter::label`): `REAR <volumeL> L · <rear port face
  or SEALED>` and `FRONT <volumeFront_L> L · <front port face>`. Port face strings reuse the
  existing `portFaceLabel`-style formatting (round `Ø … mm` / rect `… × … mm`).

Orientation: rear chamber left, front chamber right (output side). Driver fires from rear
into front.

## Architecture

- **`src/diagrams/portdiagram.h`**: add a pure inline helper
  `inline bool bandpassRearSealed(const BoxModel &m)` → true for BP4, false for BP6 (drives
  the sealed-vs-ported rear). Declare `void drawBandpassSection(DiagramPainter&, const BoxModel&)`.
  Add `inline QString portFaceLabelFront(const BoxModel&)` mirroring the existing
  `portFaceLabel` but reading the `portFront*` fields (the existing helper reads only the
  rear `port*` fields). Inline and testable.
- **`src/diagrams/portdiagram.cpp`**: implement `drawBandpassSection` using only
  `DiagramPainter` methods (`hatchedRect`, `wall`, `airflow`, `label`). Reuses the canonical
  fixed-layout approach of `drawPortSection`.
- **`src/enclosurewidget.cpp`** (refresh block): dispatch by enclosure type —
  ```cpp
  if (m_portDiagram) {
      BoxModel diag = m;
      if (isBandpass(diag))
          m_portDiagram->setDraw([diag](DiagramPainter &d){ drawBandpassSection(d, diag); });
      else
          m_portDiagram->setDraw([diag](DiagramPainter &d){ drawPortSection(d, diag); });
  }
  ```
  (`isBandpass` already exists in `enclosurewidget.cpp`.)

No change to `DiagramPainter` or `DiagramView`. No height change to the widget.

## Data model

Uses existing `BoxModel` fields only — no new fields:
- Rear: `volumeL`, `portShape`, `portWidth_mm`, `portHeight_mm`, `portFlare`, `numPorts`.
- Front: `volumeFront_L`, `portFrontShape`, `portFrontWidth_mm`, `portFrontHeight_mm`,
  `portFrontFlare`, `numPortsFront`.
- Type: `encType` via `isBP4`/`isBP6`/`isBandpass`.

## Testing

Pure helpers asserted in the existing `tests/portphysics_tests.cpp` harness:
- `bandpassRearSealed`: true for `EncType::Bandpass4`, false for `EncType::Bandpass6`.
- Front face label helper (if added): round/rect formatting like the existing `portFaceLabel`
  tests.

`drawBandpassSection` itself is visual — validated in-app (UAT): switch a model between
Vented / BP4 / BP6 and confirm the diagram swaps correctly, BP4 shows a sealed rear with one
front port, BP6 shows both ports, labels reflect the chambers, and it fits the tab in both
light and dark themes.

## Out of scope

- Changing the plain Vented diagram (stays `drawPortSection`).
- Data-driven / true-to-scale proportions.
- The remaining future diagram types (port end-on, driver cross-section).
- PDF-report embedding.
- Per-port flare detail inside the small wall openings (flare is conveyed by labels in the
  chamber view, not by drawing a bell mouth on the tiny wall port).
