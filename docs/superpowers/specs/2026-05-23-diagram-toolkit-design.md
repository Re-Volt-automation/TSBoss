# Diagram Toolkit & Port Cross-Section — Design Spec

**Date:** 2026-05-23
**Status:** Approved (design), pending implementation plan
**Area:** Enclosure modeller — schematic illustrations

## Problem

The Port tab's flare illustration is Unicode box-drawing art in a monospace `QLabel`
(`m_portFlareNote` in `enclosurewidget.cpp`). It looks crude and doesn't scale or theme.
More illustrations are wanted across the app (enclosure cross-sections, bandpass chambers,
port end-on shapes, driver cross-sections), so a one-off fix would be wasted work.

## Goal

Build a small **reusable diagram toolkit** rendered with QPainter (the same technique as
every plot in TSBoss), and ship the **port cross-section** as its first instance —
replacing the ASCII art. The toolkit concentrates a single visual style ("technical
blueprint") in one place so the four future diagrams are cheap and consistent.

## Decisions (from brainstorming)

- **Schematic, not data-driven.** Diagrams are clean canonical illustrations. The *type* is
  honest (round/rect, flush/flared) but proportions are fixed for legibility; exact values
  appear in labels. (Data-driven proportional drawing was considered and rejected as more
  engine work for this use.)
- **Visual style: "technical blueprint."** Thin uniform outlines, hatched solids, dimension
  lines with extension marks + arrowheads, small UPPERCASE labels, and a single blue accent
  for airflow / the active part.
- **Screen-only for v1.** Not wired into the PDF report yet — but drawing goes through a
  painter abstraction so report embedding is a cheap later add (see Forward-compat).
- **Structure: painter helper + draw functions + one generic view widget** (chosen over a
  widget-subclass-per-diagram, which spreads style and adds boilerplate).
- **Five diagram types foreseen:** port cross-section (v1), enclosure cross-section,
  bandpass chambers, port shape (end-on), driver cross-section. Only the port ships in v1;
  the toolkit must not preclude the rest.

## Architecture

New directory `src/diagrams/`:

### `diagrampainter.h/.cpp` — the style guide, in code
`class DiagramPainter` wraps a `QPainter&` + target `QRect` and owns the entire blueprint
style. Every diagram draws *only* through it, so changing a convention here updates all
diagrams. Interface (names indicative):

- `DiagramPainter(QPainter &p, const QRect &area)`
- `outline()` → sets the standard pen (theme text color, 1.25 px)
- `QBrush hatchBrush()` → 45° hatch at 0.6 px / 0.5 alpha (solid material)
- `void dimension(QPointF a, QPointF b, const QString &label)` → extension lines, ticks,
  arrowheads, centred uppercase label
- `void airflow(QPointF a, QPointF b)` → dashed blue accent line with arrowhead
- `void label(QPointF at, const QString &text, Align)` → uppercase label text
- Theme colors pulled from `Theme::instance()`; the accent is one named constant
  `kAccent` (the blue).

### `portdiagram.h/.cpp` — first diagram
One free function `void drawPortSection(DiagramPainter &d, const BoxModel &m)`. Reads
`portShape`, `portFlare` (0/1/2), `portInsertDepth_mm`, the computed effective length,
`portWidth_mm`/`portHeight_mm`, and `numPorts`. Draws the canonical side cross-section:
baffle wall (hatched), tube walls, flare arcs on the flared end(s) (outer/baffle side for
one-end), a protruding segment when insert depth > 0, an airflow accent, and dimension +
labels (`L`, `Ø`/`W×H`, and `×N` when `numPorts > 1`). Round vs rectangular reads the same
in side section, so the visible difference is the label (the dedicated **port end-on**
diagram, a later sibling, shows the face shape).

### `diagramview.h/.cpp` — generic host widget
`class DiagramView : public QWidget`. Holds `std::function<void(DiagramPainter&)> m_draw`
and a `setDraw(fn)` setter. Its `paintEvent` fills the background, constructs a
`DiagramPainter` over its content rect, and invokes `m_draw`. One widget class is reused for
all diagram types — no per-diagram subclass.

### Future siblings (NOT in v1)
`drawEnclosureSection`, `drawBandpass`, `drawPortEndOn`, `drawDriverSection` — each a new
free function using the same `DiagramPainter`. Out of scope here; listed to confirm the
interface supports them.

## Integration / data flow

In `EnclosureWidget`, replace the `m_portFlareNote` `QLabel` with a `DiagramView` placed in
the same spot in the Port tab layout. At the existing port-field refresh point (where
`m_portFlareNote` is currently updated, ~`enclosurewidget.cpp:3987`), set the view's
callback to draw the current model and repaint:

```cpp
m_portDiagram->setDraw([m](DiagramPainter &d){ drawPortSection(d, m); });
m_portDiagram->update();
```

The diagram is shown for all ported configs (not only when flared) — it now conveys
shape/length/mounting, not just flare. Remove the ASCII-art construction and the
`m.portFlare == 0 → hide` logic.

## Theming

`DiagramPainter` reads stroke and label colors from `Theme::instance()` exactly as the plot
classes do, so light/dark themes work with no per-diagram code. `kAccent` (airflow blue) is
chosen to read on both themes.

## Forward-compat (not v1 scope)

Because every diagram is a free function taking a `DiagramPainter` over an arbitrary
`QPainter` + `QRect`, embedding one in the PDF report later means calling the same
`drawPortSection(d, m)` against the report's painter at the desired rect. No redesign
required; v1 simply does not wire it into `pdfreport.cpp`.

## Testing

Diagrams are visual; there are no golden-pixel tests. Verifiable logic is kept as small pure
helpers and asserted in the existing `tests/portphysics_tests.cpp` CTest harness:

- `flareBaffleSide(portFlare)` / equivalent — which end(s) flare for 0/1/2.
- Label formatting helper — e.g. effective-length and diameter strings.

The visual result (the blueprint look, variant correctness, theming) is validated by the
user in-app (UAT): switch `portFlare` and insert depth, toggle theme, confirm the diagram
updates correctly and reads cleanly.

## Out of scope (v1)

- The four future diagram types (enclosure, bandpass, end-on, driver).
- PDF-report embedding.
- Data-driven / true-to-scale proportions.
- Animation or interactivity (hover, click) on diagrams.
