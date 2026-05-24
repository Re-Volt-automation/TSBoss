# Bandpass Chamber Diagram Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give bandpass enclosures (BP4/BP6) a correct schematic — an integrated chamber view (driver on the divider, rear + front chambers, ports per chamber) — instead of the single-port cross-section that only shows the rear port.

**Architecture:** Add `drawBandpassSection(DiagramPainter&, const BoxModel&)` alongside the existing `drawPortSection`, built entirely on the existing `DiagramPainter`. `EnclosureWidget` dispatches by enclosure type: bandpass → chamber view, plain vented → the unchanged port cross-section. Two new pure inline helpers carry the testable logic.

**Tech Stack:** C++17, Qt 6 Widgets/Gui (`QPainter`/`QPainterPath`), CMake. Reuses `DiagramPainter` (no new style primitives).

---

## Context (current state)

- Branch `feat/diagram-toolkit` already has the toolkit: `DiagramPainter`, `DiagramView`, and `drawPortSection` + inline helpers (`portFlareOuter/Inner`, `portFaceLabel`) in `src/diagrams/portdiagram.{h,cpp}`.
- `tests/portphysics_tests.cpp` already `#include "diagrams/portdiagram.h"` and tests the existing helpers (hand-rolled `check(bool, const char*)` harness, no gtest).
- `EnclosureWidget` shows the diagram via `m_portDiagram` (a `DiagramView`); the refresh block currently always calls `drawPortSection`.
- `isBandpass(const BoxModel&)` already exists in `enclosurewidget.cpp`.
- `DiagramView` paints into a content rect; `DiagramPainter::area()` returns it. Methods available: `wall(QPainterPath)`, `hatchedRect(QRectF)`, `airflow(QPointF,QPointF)`, `label(QPointF, QString, Align)`.

## File Structure
- **Modify `src/diagrams/portdiagram.h`** — add inline helpers `bandpassRearSealed`, `portFaceLabelFront`; declare `drawBandpassSection`.
- **Modify `src/diagrams/portdiagram.cpp`** — implement `drawBandpassSection`.
- **Modify `tests/portphysics_tests.cpp`** — assert the two new helpers.
- **Modify `src/enclosurewidget.cpp`** — dispatch the refresh callback by enclosure type.

---

## Task 1: Pure helpers + tests (TDD)

**Files:**
- Modify: `src/diagrams/portdiagram.h`
- Modify: `tests/portphysics_tests.cpp`

- [ ] **Step 1: Add the helpers to `src/diagrams/portdiagram.h`**

After the existing `portFaceLabel` definition (and before the `drawPortSection` declaration), add:

```cpp
// Bandpass rear chamber: sealed (BP4) vs vented/ported (BP6).
inline bool bandpassRearSealed(const BoxModel &m)
{ return m.encType == BoxModel::EncType::Bandpass4; }

// Front-port face label (mirror of portFaceLabel, reads the portFront* fields).
inline QString portFaceLabelFront(const BoxModel &m)
{
    if (m.portFrontShape == 0)
        return QString("Ø %1 mm").arg(int(m.portFrontWidth_mm + 0.5));
    return QString("%1 × %2 mm").arg(int(m.portFrontWidth_mm + 0.5))
                                .arg(int(m.portFrontHeight_mm + 0.5));
}
```
(The `×` is U+00D7, matching `portFaceLabel`.)

- [ ] **Step 2: Add the failing tests to `tests/portphysics_tests.cpp`**

In `main()`, after the existing "Port-diagram pure helpers" block, add:

```cpp
    // Bandpass diagram helpers.
    {
        BoxModel bp4; bp4.encType = BoxModel::EncType::Bandpass4;
        BoxModel bp6; bp6.encType = BoxModel::EncType::Bandpass6;
        BoxModel v;   v.encType   = BoxModel::EncType::Vented;
        check(bandpassRearSealed(bp4),  "BP4 rear is sealed");
        check(!bandpassRearSealed(bp6), "BP6 rear is vented");
        check(!bandpassRearSealed(v),   "vented is not bandpass-sealed");
        BoxModel fr;  fr.portFrontShape = 0;  fr.portFrontWidth_mm = 100.0;
        check(portFaceLabelFront(fr) == "Ø 100 mm", "front round face label");
        BoxModel frr; frr.portFrontShape = 1; frr.portFrontWidth_mm = 90.0; frr.portFrontHeight_mm = 40.0;
        check(portFaceLabelFront(frr) == "90 × 40 mm", "front rect face label");
    }
```
(Use the literal `×` = U+00D7 in the source, matching the helper.)

- [ ] **Step 3: Build and run the test**

Run: `cd build && cmake --build . --target portphysics_tests && ctest --output-on-failure`
Expected: clean build; `100% tests passed`, including the 5 new bandpass-helper checks. (The helpers are inline in the header, so the test compiles and passes immediately — that is fine; the gate is a green run.)

- [ ] **Step 4: Commit**

```bash
git add src/diagrams/portdiagram.h tests/portphysics_tests.cpp
git commit -m "feat(diagrams): bandpass diagram helpers (rear-sealed, front face label)"
```

---

## Task 2: Implement `drawBandpassSection`

**Files:**
- Modify: `src/diagrams/portdiagram.h` (declaration)
- Modify: `src/diagrams/portdiagram.cpp` (implementation)

- [ ] **Step 1: Declare `drawBandpassSection` in `src/diagrams/portdiagram.h`**

After the existing `void drawPortSection(DiagramPainter &d, const BoxModel &m);` line, add:

```cpp
// Draw a schematic bandpass chamber cross-section: driver on the central
// divider, rear chamber (left) + front chamber (right). BP6 = both ports;
// BP4 = sealed rear (hatch wash) + front port only.
void drawBandpassSection(DiagramPainter &d, const BoxModel &m);
```

- [ ] **Step 2: Implement it in `src/diagrams/portdiagram.cpp`**

Add (after `drawPortSection`):

```cpp
void drawBandpassSection(DiagramPainter &d, const BoxModel &m)
{
    const QRectF a = d.area();
    // Landscape enclosure box: 18px each side for port stubs, 30px below for labels.
    const QRectF box(a.left() + 18, a.top() + 6, a.width() - 36, a.height() - 30);
    const double midX = box.center().x();
    const double cy   = box.center().y();

    // Sealed rear (BP4): faint hatch wash over the rear (left) chamber.
    if (bandpassRearSealed(m))
        d.hatchedRect(QRectF(box.left(), box.top(), midX - box.left(), box.height()));

    // Enclosure box + central divider.
    QPainterPath shell;
    shell.addRect(box);
    shell.moveTo(midX, box.top());
    shell.lineTo(midX, box.bottom());
    d.wall(shell);

    // Driver on the divider (cone opening toward the front/right chamber).
    QPainterPath drv;
    drv.moveTo(midX - 12, cy - 18);
    drv.lineTo(midX,      cy - 11);
    drv.lineTo(midX,      cy + 11);
    drv.lineTo(midX - 12, cy + 18);
    d.wall(drv);

    // Rear port (left wall) — only when the rear is vented (BP6).
    if (!bandpassRearSealed(m)) {
        QPainterPath rp;
        rp.addRect(QRectF(box.left() - 12, cy - 7, 12, 14));
        d.wall(rp);
        d.airflow(QPointF(box.left() + 28, cy), QPointF(box.left() - 6, cy));
    }

    // Front port (right wall) — present for BP4 and BP6.
    QPainterPath fp;
    fp.addRect(QRectF(box.right(), cy - 7, 12, 14));
    d.wall(fp);
    d.airflow(QPointF(box.right() - 28, cy), QPointF(box.right() + 6, cy));

    // Chamber labels.
    const QString rearFace = bandpassRearSealed(m) ? QStringLiteral("SEALED") : portFaceLabel(m);
    const QString rearTxt  = QString("REAR %1 L · %2").arg(int(m.volumeL + 0.5)).arg(rearFace);
    const QString frontTxt = QString("FRONT %1 L · %2")
                                 .arg(int(m.volumeFront_L + 0.5)).arg(portFaceLabelFront(m));
    d.label(QPointF((box.left() + midX) / 2.0,  a.bottom() - 2), rearTxt,  DiagramPainter::Align::Center);
    d.label(QPointF((midX + box.right()) / 2.0, a.bottom() - 2), frontTxt, DiagramPainter::Align::Center);
}
```
(`portdiagram.cpp` already includes `<QPainterPath>` and `diagrams/portdiagram.h`; no new includes needed.)

- [ ] **Step 3: Build**

Run: `cd build && cmake --build . --parallel && ctest --output-on-failure`
Expected: clean build; tests still 100% (no test calls `drawBandpassSection` — it is visual; the helpers it uses are already tested).

- [ ] **Step 4: Commit**

```bash
git add src/diagrams/portdiagram.h src/diagrams/portdiagram.cpp
git commit -m "feat(diagrams): drawBandpassSection chamber cross-section (BP4/BP6)"
```

---

## Task 3: Dispatch by enclosure type in EnclosureWidget

**Files:**
- Modify: `src/enclosurewidget.cpp` (refresh block)

- [ ] **Step 1: Replace the diagram refresh dispatch**

Find the current block (around line 3841):
```cpp
    // ── Port cross-section diagram ───────────────────────────────
    if (m_portDiagram) {
        BoxModel diag = m;   // copy for the draw callback
        m_portDiagram->setDraw([diag](DiagramPainter &d){ drawPortSection(d, diag); });
    }
```
Replace with:
```cpp
    // ── Port / chamber diagram ───────────────────────────────────
    if (m_portDiagram) {
        BoxModel diag = m;   // copy for the draw callback
        if (isBandpass(diag))
            m_portDiagram->setDraw([diag](DiagramPainter &d){ drawBandpassSection(d, diag); });
        else
            m_portDiagram->setDraw([diag](DiagramPainter &d){ drawPortSection(d, diag); });
    }
```
`drawBandpassSection` is reachable because `enclosurewidget.cpp` already `#include "diagrams/portdiagram.h"` (added when `drawPortSection` was integrated). `isBandpass` already exists in this file.

- [ ] **Step 2: Build and run the app**

Run: `cd build && cmake --build . --parallel && ctest --output-on-failure`
Expected: clean build, tests 100%.

- [ ] **Step 3: Commit**

```bash
git add src/enclosurewidget.cpp
git commit -m "feat(diagrams): dispatch bandpass models to the chamber diagram"
```

---

## Task 4: UAT and doc commit

**Files:** (none — verification + committing the spec/plan docs)

- [ ] **Step 1: Manual verification (UAT)**

Run: `cd build && ./TSBoss`. Create/select models and confirm on the Port tab:
- **Plain Vented** → still the detailed single-port cross-section (unchanged).
- **BP6** → chamber view with driver on the divider and BOTH ports (rear-left, front-right), each with a blue airflow arrow; labels read `REAR <V> L · <port>` and `FRONT <V> L · <port>`.
- **BP4** → chamber view with the rear (left) chamber hatch-washed + "SEALED", only the front (right) port drawn.
- Switching a model between Vented / BP4 / BP6 swaps the drawing correctly.
- Fits the Port tab without clipping (no height increase); reads cleanly in light AND dark themes.
- Changing front/rear port shape (round↔rect) updates the chamber labels.

- [ ] **Step 2: Commit the design + plan docs**

```bash
git add docs/superpowers/specs/2026-05-24-bandpass-chamber-diagram-design.md \
        docs/superpowers/plans/2026-05-24-bandpass-chamber-diagram.md
git commit -m "docs: bandpass chamber diagram design spec + implementation plan"
```

---

## Self-Review (completed)

- **Spec coverage:** chamber view with box + divider + driver, rear (BP6 port / BP4 sealed hatch) + front port, blue airflow, chamber labels (Task 2); dispatch by `isBandpass` with vented unchanged (Task 3); pure helpers `bandpassRearSealed` + `portFaceLabelFront` unit-tested (Task 1); rear-left/front-right orientation per the approved spec; fits existing 120px height (no height change); UAT (Task 4). All spec sections mapped.
- **Out-of-scope honoured:** vented diagram untouched, no data-driven scaling, no new `DiagramPainter` primitives, no PDF wiring, no port-velocity changes (that is the separate deferred follow-up).
- **Placeholder scan:** every code step is complete; no TBD/TODO; the only "visual, validated in UAT" note is accurate (drawing has no golden test, helpers do).
- **Type consistency:** `bandpassRearSealed(const BoxModel&)`, `portFaceLabelFront(const BoxModel&)`, `drawBandpassSection(DiagramPainter&, const BoxModel&)` declared in Task 1/2 and used consistently in Task 2/3; reuses existing `portFaceLabel`, `DiagramPainter::{wall,hatchedRect,airflow,label,Align}`, `isBandpass`.
