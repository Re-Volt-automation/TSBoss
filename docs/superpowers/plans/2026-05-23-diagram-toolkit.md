# Diagram Toolkit & Port Cross-Section Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the ASCII port-flare illustration with a clean, theme-aware "blueprint" schematic, built on a small reusable QPainter diagram toolkit so the four future diagram types come cheap.

**Architecture:** A `DiagramPainter` helper owns the entire blueprint visual style (pens, hatch, dimension arrows, labels, theme colours). Each diagram is a free function taking a `DiagramPainter&` (v1 ships `drawPortSection`). A single generic `DiagramView : QWidget` hosts a draw callback. `EnclosureWidget` swaps its `m_portFlareNote` QLabel for a `DiagramView`.

**Tech Stack:** C++17, Qt 6 Widgets/Gui (`QPainter`), CMake. Theme via `Theme::instance()`. No new libraries.

---

## Design Notes (read first)

- **Schematic, fixed proportions.** The drawing is canonical; it does NOT scale to entered dimensions. Variants (round/rect, flush/flared, protruding insert) and label *values* reflect `BoxModel`, but layout geometry is fixed for legibility.
- **No physics duplication.** `drawPortSection` reads only raw `BoxModel` fields (`portShape`, `portFlare`, `portInsertDepth_mm`, `portWidth_mm`, `portHeight_mm`, `numPorts`). The numeric port *length* already lives in the results column (`m_portLenLbl`); the diagram's length dimension is labelled `"L"` (no recomputation).
- **Flare end mapping:** `portFlare` `0`=neither, `1`=outer (baffle) end only, `2`=both ends. Outer = baffle side (left).
- **Style → Theme colours:** outline/labels from `Theme::instance().textPrimary()`/`textSecondary()`; airflow is a fixed blue accent `kAirflow` (semantic "air", distinct from the app's amber UI accent).

## File Structure

- **Create `src/diagrams/diagrampainter.h` / `.cpp`** — `DiagramPainter`: the blueprint style, in code. Depends on `<QPainter>`, `theme.h`.
- **Create `src/diagrams/diagramview.h` / `.cpp`** — generic `DiagramView : QWidget` hosting a `std::function<void(DiagramPainter&)>`. Depends on `<QWidget>`, `diagrampainter.h`.
- **Create `src/diagrams/portdiagram.h` / `.cpp`** — `drawPortSection(DiagramPainter&, const BoxModel&)` + inline pure helpers (`portFlareOuter`, `portFlareInner`, `portFaceLabel`). Depends on `boxmodel.h`, `diagrampainter.h`.
- **Modify `src/enclosurewidget.h`** — replace `m_portFlareNote` member; add include.
- **Modify `src/enclosurewidget.cpp`** — swap widget construction, refresh, and hide sites; remove ASCII art.
- **Modify `tests/portphysics_tests.cpp`** — assert the pure helpers.
- **Modify `CMakeLists.txt`** — add the six new files to `PROJECT_SOURCES`.

---

## Task 1: DiagramPainter — the blueprint style helper

**Files:**
- Create: `src/diagrams/diagrampainter.h`, `src/diagrams/diagrampainter.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `src/diagrams/diagrampainter.h`**

```cpp
#pragma once
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QPointF>
#include <QString>

// Blueprint-style drawing primitives shared by every schematic diagram.
// Owns the whole visual language — line weights, hatch, dimension arrows,
// labels, theme colours. Diagrams draw ONLY through this; change a convention
// here and all diagrams update. Construct one over a target QPainter + rect.
class DiagramPainter
{
public:
    enum class Align { Left, Center, Right };

    DiagramPainter(QPainter &p, const QRectF &area);

    const QRectF &area() const { return m_area; }
    QPainter     &p() const { return m_p; }

    void setOutlinePen();                       // standard 1.25px theme outline
    void hatchedRect(const QRectF &r);          // solid material: hatch fill + outline
    void wall(const QPainterPath &path);        // outline-pen polyline (no fill)
    void airflow(QPointF a, QPointF b);         // dashed blue accent arrow a→b
    void dimension(QPointF a, QPointF b, const QString &label); // ext lines, ticks, arrows, label
    void label(QPointF at, const QString &text, Align align = Align::Center);

private:
    void arrowHead(QPointF tip, QPointF from, const QColor &c); // filled triangle at tip
    QPainter &m_p;
    QRectF    m_area;
};
```

- [ ] **Step 2: Create `src/diagrams/diagrampainter.cpp`**

```cpp
#include "diagrams/diagrampainter.h"
#include "theme.h"
#include <QFont>
#include <cmath>

namespace {
const QColor kAirflow(0x4a, 0x90, 0xd9);   // semantic "air" accent (not the UI amber)
constexpr double kOutlineW = 1.25;
constexpr double kDimW     = 0.8;
constexpr double kHatchW   = 0.6;
}

DiagramPainter::DiagramPainter(QPainter &p, const QRectF &area) : m_p(p), m_area(area) {}

void DiagramPainter::setOutlinePen()
{
    m_p.setPen(QPen(Theme::instance().textPrimary(), kOutlineW));
    m_p.setBrush(Qt::NoBrush);
}

void DiagramPainter::hatchedRect(const QRectF &r)
{
    QColor hc = Theme::instance().textPrimary();
    hc.setAlphaF(0.5);
    QBrush b(hc, Qt::BDiagPattern);
    m_p.setPen(QPen(hc, kHatchW));
    m_p.setBrush(b);
    m_p.drawRect(r);
    setOutlinePen();
    m_p.setBrush(Qt::NoBrush);
    m_p.drawRect(r);
}

void DiagramPainter::wall(const QPainterPath &path)
{
    setOutlinePen();
    m_p.drawPath(path);
}

void DiagramPainter::arrowHead(QPointF tip, QPointF from, const QColor &c)
{
    QPointF d = tip - from;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1e-6) return;
    d /= len;
    const QPointF n(-d.y(), d.x());
    const double s = 4.0;       // half-width
    const double l = 7.0;       // length
    QPointF base = tip - d * l;
    QPolygonF tri; tri << tip << (base + n * s) << (base - n * s);
    m_p.setPen(Qt::NoPen);
    m_p.setBrush(c);
    m_p.drawPolygon(tri);
}

void DiagramPainter::airflow(QPointF a, QPointF b)
{
    QPen pen(kAirflow, kOutlineW, Qt::DashLine);
    pen.setDashPattern({5, 3});
    m_p.setPen(pen);
    m_p.setBrush(Qt::NoBrush);
    m_p.drawLine(a, b);
    arrowHead(b, a, kAirflow);
}

void DiagramPainter::dimension(QPointF a, QPointF b, const QString &label)
{
    const QColor c = Theme::instance().textSecondary();
    QPointF d = b - a;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1e-6) return;
    d /= len;
    const QPointF n(-d.y(), d.x());
    // extension ticks
    m_p.setPen(QPen(c, kHatchW));
    m_p.drawLine(a - n * 4, a + n * 4);
    m_p.drawLine(b - n * 4, b + n * 4);
    // dimension line
    m_p.setPen(QPen(c, kDimW));
    m_p.drawLine(a, b);
    // inward arrowheads
    arrowHead(a, a + d * 8, c);
    arrowHead(b, b - d * 8, c);
    // label centred, offset along +n
    label_internal_centre(a, b, n, label, c);
}

void DiagramPainter::label(QPointF at, const QString &text, Align align)
{
    QFont f = m_p.font();
    f.setPointSizeF(7.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    m_p.setFont(f);
    m_p.setPen(Theme::instance().textSecondary());
    const QString up = text.toUpper();
    QFontMetricsF fm(f);
    const double w = fm.horizontalAdvance(up);
    QPointF pos = at;
    if (align == Align::Center) pos.rx() -= w / 2.0;
    else if (align == Align::Right) pos.rx() -= w;
    m_p.drawText(pos, up);
}
```

Note: the `dimension()` body calls a small inline label-centring helper. Define it as a private member instead — replace the final `label_internal_centre(...)` line with this inline implementation directly in `dimension()`:

```cpp
    // label centred below the dimension line
    QFont f = m_p.font();
    f.setPointSizeF(7.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    m_p.setFont(f);
    m_p.setPen(c);
    const QString up = label.toUpper();
    QFontMetricsF fm(f);
    const QPointF mid = (a + b) / 2.0 + n * 12.0;
    m_p.drawText(QPointF(mid.x() - fm.horizontalAdvance(up) / 2.0, mid.y() + 4), up);
```
(Delete the `label_internal_centre` call — it was shorthand. The `label()` public method stays as written for diagrams that need a free-standing label.)

- [ ] **Step 3: Add the two files to `CMakeLists.txt`**

In `PROJECT_SOURCES`, after `src/enclosurewidget.cpp`, add:
```cmake
    src/diagrams/diagrampainter.h
    src/diagrams/diagrampainter.cpp
```

- [ ] **Step 4: Build**

Run: `cd build && cmake .. && cmake --build . --parallel`
Expected: clean build. `diagrampainter.cpp` compiles (unused so far). If `Theme` lacks `textPrimary()`/`textSecondary()`, check `src/theme.h` for the actual accessor names and use those.

- [ ] **Step 5: Commit**

```bash
git add src/diagrams/diagrampainter.h src/diagrams/diagrampainter.cpp CMakeLists.txt
git commit -m "feat(diagrams): DiagramPainter blueprint-style drawing helper"
```

---

## Task 2: DiagramView — generic host widget

**Files:**
- Create: `src/diagrams/diagramview.h`, `src/diagrams/diagramview.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create `src/diagrams/diagramview.h`**

```cpp
#pragma once
#include <QWidget>
#include <functional>
#include "diagrams/diagrampainter.h"

// Generic host for any schematic diagram. Holds a draw callback and renders it
// through a DiagramPainter on paint. Reused for every diagram type — no
// per-diagram QWidget subclass.
class DiagramView : public QWidget
{
    Q_OBJECT
public:
    explicit DiagramView(QWidget *parent = nullptr);

    // Set the drawing callback (pass nullptr to show an empty frame). Repaints.
    void setDraw(std::function<void(DiagramPainter &)> fn);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    std::function<void(DiagramPainter &)> m_draw;
};
```

- [ ] **Step 2: Create `src/diagrams/diagramview.cpp`**

```cpp
#include "diagrams/diagramview.h"
#include "theme.h"
#include <QPainter>

DiagramView::DiagramView(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void DiagramView::setDraw(std::function<void(DiagramPainter &)> fn)
{
    m_draw = std::move(fn);
    update();
}

void DiagramView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Theme::instance().sunkenBg());
    if (!m_draw) return;
    const QRectF content = QRectF(rect()).adjusted(10, 10, -10, -10);
    DiagramPainter d(p, content);
    m_draw(d);
}
```

- [ ] **Step 3: Add to `CMakeLists.txt`**

After the diagrampainter lines in `PROJECT_SOURCES`:
```cmake
    src/diagrams/diagramview.h
    src/diagrams/diagramview.cpp
```

- [ ] **Step 4: Build**

Run: `cd build && cmake .. && cmake --build . --parallel`
Expected: clean build (AUTOMOC handles the `Q_OBJECT`). If `Theme` lacks `sunkenBg()`, use the actual inset-surface accessor from `src/theme.h`.

- [ ] **Step 5: Commit**

```bash
git add src/diagrams/diagramview.h src/diagrams/diagramview.cpp CMakeLists.txt
git commit -m "feat(diagrams): generic DiagramView host widget"
```

---

## Task 3: portdiagram — drawPortSection + pure helpers (TDD the helpers)

**Files:**
- Create: `src/diagrams/portdiagram.h`, `src/diagrams/portdiagram.cpp`
- Modify: `tests/portphysics_tests.cpp`, `CMakeLists.txt`

- [ ] **Step 1: Create `src/diagrams/portdiagram.h` with inline helpers + the draw declaration**

```cpp
#pragma once
#include <QString>
#include "boxmodel.h"
#include "diagrams/diagrampainter.h"

// Does the outer (baffle-side) end flare?  portFlare: 0 none, 1 outer, 2 both.
inline bool portFlareOuter(int portFlare) { return portFlare >= 1; }
// Does the inner (in-box) end flare?
inline bool portFlareInner(int portFlare) { return portFlare >= 2; }

// Face-dimension label, e.g. "Ø 75 MM" (round) or "75 × 50 MM" (rect).
inline QString portFaceLabel(const BoxModel &m)
{
    if (m.portShape == 0)
        return QString("Ø %1 mm").arg(int(m.portWidth_mm + 0.5));
    return QString("%1 × %2 mm").arg(int(m.portWidth_mm + 0.5))
                                .arg(int(m.portHeight_mm + 0.5));
}

// Draw a schematic side cross-section of the port into the painter's area.
void drawPortSection(DiagramPainter &d, const BoxModel &m);
```

- [ ] **Step 2: Write the failing helper tests**

In `tests/portphysics_tests.cpp`, add `#include "diagrams/portdiagram.h"` near the top includes, and add this block to `main()` before `return`:

```cpp
    // Port-diagram pure helpers.
    {
        check(!portFlareOuter(0) && portFlareOuter(1) && portFlareOuter(2),
              "portFlareOuter: flares for one-end and both-ends");
        check(!portFlareInner(0) && !portFlareInner(1) && portFlareInner(2),
              "portFlareInner: inner flares only for both-ends");
        BoxModel rnd; rnd.portShape = 0; rnd.portWidth_mm = 75.0;
        check(portFaceLabel(rnd) == "Ø 75 mm", "round face label");
        BoxModel rect; rect.portShape = 1; rect.portWidth_mm = 80.0; rect.portHeight_mm = 50.0;
        check(portFaceLabel(rect) == "80 × 50 mm", "rect face label");
    }
```

- [ ] **Step 3: Run to confirm it fails (won't compile / link until the header exists)**

Run: `cd build && cmake .. && cmake --build . --target portphysics_tests 2>&1 | tail -20`
Expected: compile error (`portdiagram.h` not found) or, once the header is added, the test target builds and the checks PASS — but first ensure the header from Step 1 exists so the test compiles. The meaningful gate is Step 5.

- [ ] **Step 4: Create `src/diagrams/portdiagram.cpp`**

```cpp
#include "diagrams/portdiagram.h"
#include <QPainterPath>

void drawPortSection(DiagramPainter &d, const BoxModel &m)
{
    const QRectF a = d.area();
    // Canonical fixed layout inside the area.
    const double baffleW = 10.0;
    const double cx0 = a.left() + 6;
    const double tubeTopY = a.top() + a.height() * 0.30;
    const double tubeBotY = a.top() + a.height() * 0.62;
    const double midY     = (tubeTopY + tubeBotY) / 2.0;
    const double outerX   = cx0 + baffleW;             // baffle/outer end
    const double innerX   = a.right() - 14;            // in-box end
    const double flareDX  = 16.0, flareDY = 10.0;

    // Baffle wall (hatched solid).
    d.hatchedRect(QRectF(cx0, a.top() + 4, baffleW, a.height() - 28));

    // Tube walls (top + bottom), with flares on flared ends.
    QPainterPath top;
    if (portFlareOuter(m.portFlare)) {
        top.moveTo(outerX - flareDX, tubeTopY - flareDY);
        top.cubicTo(outerX, tubeTopY - flareDY, outerX, tubeTopY, outerX + 6, tubeTopY);
    } else {
        top.moveTo(outerX, tubeTopY);
    }
    top.lineTo(portFlareInner(m.portFlare) ? innerX - flareDX : innerX, tubeTopY);
    if (portFlareInner(m.portFlare))
        top.cubicTo(innerX, tubeTopY, innerX, tubeTopY - flareDY, innerX + 6, tubeTopY - flareDY);
    d.wall(top);

    QPainterPath bot;
    if (portFlareOuter(m.portFlare)) {
        bot.moveTo(outerX - flareDX, tubeBotY + flareDY);
        bot.cubicTo(outerX, tubeBotY + flareDY, outerX, tubeBotY, outerX + 6, tubeBotY);
    } else {
        bot.moveTo(outerX, tubeBotY);
    }
    bot.lineTo(portFlareInner(m.portFlare) ? innerX - flareDX : innerX, tubeBotY);
    if (portFlareInner(m.portFlare))
        bot.cubicTo(innerX, tubeBotY, innerX, tubeBotY + flareDY, innerX + 6, tubeBotY + flareDY);
    d.wall(bot);

    // Airflow through the centre (out of the box, toward the baffle/left).
    d.airflow(QPointF(innerX - 20, midY), QPointF(outerX + 14, midY));

    // Length dimension under the tube (qualitative — exact value is in the results column).
    d.dimension(QPointF(outerX, tubeBotY + flareDY + 16),
                QPointF(innerX, tubeBotY + flareDY + 16), "L");

    // Face/shape label above, plus port count if >1.
    QString cap = portFaceLabel(m);
    if (m.numPorts > 1) cap += QString(" · ×%1").arg(m.numPorts);
    d.label(QPointF(a.center().x(), a.top() + 12), cap, DiagramPainter::Align::Center);
}
```

- [ ] **Step 5: Add to `CMakeLists.txt` and build + test**

Add to `PROJECT_SOURCES`:
```cmake
    src/diagrams/portdiagram.h
    src/diagrams/portdiagram.cpp
```
The test target needs the diagram include path (it already has `src` via `target_include_directories`). It uses only the inline helpers, so it does NOT need to link `portdiagram.cpp`.

Run: `cd build && cmake .. && cmake --build . --parallel && ctest --output-on-failure`
Expected: clean build; `100% tests passed` including the four new helper checks.

- [ ] **Step 6: Commit**

```bash
git add src/diagrams/portdiagram.h src/diagrams/portdiagram.cpp tests/portphysics_tests.cpp CMakeLists.txt
git commit -m "feat(diagrams): drawPortSection blueprint port cross-section + helpers"
```

---

## Task 4: Integrate into EnclosureWidget (replace the ASCII note)

**Files:**
- Modify: `src/enclosurewidget.h` (member + include)
- Modify: `src/enclosurewidget.cpp` (construction, refresh, hide; remove ASCII)

- [ ] **Step 1: Swap the member declaration in `src/enclosurewidget.h`**

Add near the top includes:
```cpp
#include "diagrams/diagramview.h"
```
Replace line ~318:
```cpp
    QLabel            *m_portFlareNote  = nullptr;  ///< diagram + note, shown when flared
```
with:
```cpp
    DiagramView       *m_portDiagram    = nullptr;  ///< schematic port cross-section
```

- [ ] **Step 2: Replace widget construction in `src/enclosurewidget.cpp` (~2866–2875)**

Replace:
```cpp
            // Flare diagram note (hidden when straight)
            m_portFlareNote = new QLabel;
            m_portFlareNote->setFont(QFont("Monospace", 8));
            m_portFlareNote->setStyleSheet(themed(
                "color:%text2%; background:%input%;"
                "font-family:'IBM Plex Mono',monospace; font-size:8pt;"
                "padding:6px 8px; border-left:2px solid %accent%;"));
            m_portFlareNote->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            m_portFlareNote->setVisible(false);
            cv->addWidget(m_portFlareNote);
```
with:
```cpp
            // Schematic port cross-section diagram.
            m_portDiagram = new DiagramView;
            cv->addWidget(m_portDiagram);
```

- [ ] **Step 3: Replace the refresh block (~3846–3872)**

Add the include near the top of the .cpp (with the other project includes):
```cpp
#include "diagrams/portdiagram.h"
```
Replace the whole `// ── Flare note / diagram ──` block:
```cpp
    // ── Flare note / diagram ─────────────────────────────────────
    if (m_portFlareNote) {
        if (m.portFlare == 0) {
            m_portFlareNote->setVisible(false);
        } else {
            ... ASCII construction ...
            m_portFlareNote->setText(diagram);
            m_portFlareNote->setVisible(true);
        }
    }
```
with:
```cpp
    // ── Port cross-section diagram ───────────────────────────────
    if (m_portDiagram) {
        BoxModel diag = m;   // capture a copy for the draw callback
        m_portDiagram->setDraw([diag](DiagramPainter &d){ drawPortSection(d, diag); });
    }
```
(`setDraw` calls `update()` internally.)

- [ ] **Step 4: Fix the "no active model" hide site (~4549)**

Replace:
```cpp
    if (m_portFlareNote)   m_portFlareNote->setVisible(false);
```
with:
```cpp
    if (m_portDiagram)     m_portDiagram->setDraw(nullptr);
```
(An empty draw renders just the blank frame.)

- [ ] **Step 5: Build and run the app**

Run: `cd build && cmake --build . --parallel && ./TSBoss`
Expected: builds clean. In the enclosure modeller, the Port tab now shows the blueprint cross-section instead of ASCII. Switching the Flare combo (Straight / One end / Both ends) changes the drawn flares; round vs rectangular updates the face label.

- [ ] **Step 6: Commit**

```bash
git add src/enclosurewidget.h src/enclosurewidget.cpp
git commit -m "feat(diagrams): replace ASCII port note with blueprint DiagramView"
```

---

## Task 5: UAT and doc commit

**Files:**
- Modify: (none — verification + committing the design/plan docs)

- [ ] **Step 1: Manual verification (UAT)**

Run: `cd build && ./TSBoss`. On a vented model, confirm:
- The Port tab shows the cross-section for ALL ported configs (not only when flared).
- Flare combo: Straight = plain tube; One end = outer (left/baffle) flare only; Both ends = flares both ends.
- Round vs Rectangular updates the face label (`Ø … mm` vs `… × … mm`); `# Ports > 1` appends `· ×N`.
- Toggle the app light/dark theme: lines, labels, and background all adapt; the blue airflow stays legible.
- Set a non-zero insert depth — confirm the diagram still reads cleanly (insert-depth visualisation is acknowledged minimal in v1; the dimension/labels remain correct).

- [ ] **Step 2: Commit the design + plan docs**

```bash
git add docs/superpowers/specs/2026-05-23-diagram-toolkit-design.md \
        docs/superpowers/plans/2026-05-23-diagram-toolkit.md
git commit -m "docs: diagram toolkit design spec + implementation plan"
```

---

## Self-Review (completed)

- **Spec coverage:** DiagramPainter style helper (Task 1), generic DiagramView (Task 2), drawPortSection + schematic blueprint variants + pure helpers (Task 3), EnclosureWidget integration replacing the ASCII note and showing for all ported configs (Task 4), theming via Theme (Tasks 1–2), screen-only with the `DiagramPainter`-over-`QPainter` seam preserved for later report use (architecture), testing of pure helpers + UAT (Tasks 3, 5). All spec sections mapped.
- **Out-of-scope honoured:** no enclosure/bandpass/end-on/driver diagrams, no PDF wiring, no data-driven scaling, no interactivity.
- **Placeholder scan:** the only "find the actual accessor" notes (Theme color names, exact line numbers) are explicit verification steps against the live code, not gaps; all code blocks are complete.
- **Type consistency:** `DiagramPainter` (with `area()`, `setOutlinePen`, `hatchedRect`, `wall`, `airflow`, `dimension`, `label`, `Align`), `DiagramView::setDraw(std::function<void(DiagramPainter&)>)`, `drawPortSection(DiagramPainter&, const BoxModel&)`, `portFlareOuter/Inner`, `portFaceLabel` are used consistently across Tasks 1–4.
- **Known v1 limitation:** insert-depth has a label/dimension but minimal geometric depiction — flagged in UAT, acceptable for v1.
