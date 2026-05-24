# BP6 Port-Tab Layout Reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reorganize the BP6 Port tab into three columns mirroring the chamber diagram — rear (inputs→results) | diagram | front (inputs→results) — and bring the front results to full parity with the rear (7 rows). Vented/BP4 unchanged.

**Architecture:** Two independent parts. (1) Front-results parity: add 3 front labels + compute them in `updatePortLength`, mirroring the rear formulas. (2) BP6 layout: promote the Port-tab container blocks to members, and a `rebuildPortArrangement(bool bp6)` method that re-homes them into a 3-column arrangement for BP6 and the original arrangement otherwise.

**Tech Stack:** C++17, Qt 6 Widgets, CMake. All work in `src/enclosurewidget.{h,cpp}`.

---

## Context (verified line anchors — re-grep before editing, they shift)

- Port tab built in the `m_portTabContent` block: header `m_portSectionHdr` (vb:2697), then `cols` HBox (2714) = `col1` (2756, rear controls) | divider | `col2` (2774, rear dims + diagram + bracing) | divider | `col3` (2925, rear results grid); `vb->addLayout(cols)` (2935); then `m_frontPortSection` `vb->addWidget` (3120); `vb->addStretch()` (3122).
- `col2` inner VBox `cv` order: `m_portDiamRow` (round dims), `m_portRectRows` (rect dims), `m_portDiagram` (the chamber diagram), then a bracing row holding `m_inPortBraceSurf`.
- Rear results grid `col3`/`g` rows → `m_portAreaLbl`, `m_portSurfAreaLbl`, `m_portVolInnerLbl`, `m_portVolDisplLbl`, `m_portLenLbl`, `m_portLenEachLbl`, `m_portF2HLbl`.
- Front results grid (inside `m_frontPortSection`, ~3092) → `m_portFrontAreaLbl`, `m_portFrontSurfAreaLbl`, `m_portFrontVolDisplLbl`, `m_portFrontLenLbl` (only 4).
- Front computation block: `if (isBP6(m) && m_portFrontLenLbl) { ... }` (~4011–4119) computes `Apf`, `Nf`, `Lp_effF`, `LpF`.
- Enclosure-type update happens in two spots that already call `m_frontPortSection->setVisible(bp6)` (~3684 and ~4300).
- `isBP6(m)` / `isBandpass(m)` helpers exist (62–64).

---

## Task 1: Add the 3 front result labels (parity — display only)

**Files:** Modify `src/enclosurewidget.h`, `src/enclosurewidget.cpp`

- [ ] **Step 1: Declare the 3 new members in `src/enclosurewidget.h`**

Next to the existing front result label declarations (`m_portFrontAreaLbl` … `m_portFrontLenLbl`, ~337–340), add:
```cpp
    QLabel            *m_portFrontVolInnerLbl = nullptr;  ///< front port air volume
    QLabel            *m_portFrontLenEachLbl  = nullptr;  ///< front per-port length
    QLabel            *m_portFrontF2HLbl      = nullptr;  ///< front 2nd pipe harmonic
```

- [ ] **Step 2: Add the rows to the front results grid in `src/enclosurewidget.cpp`**

In the front results grid block (the `addR2(...)` calls ~3101–3104), reorder/extend so the front matches the rear's 7-row order. Replace the four `addR2` lines with:
```cpp
                addR2("Port area (each):",   m_portFrontAreaLbl);
                addR2("Inner surface area:", m_portFrontSurfAreaLbl);
                addR2("Port vol. (air):",    m_portFrontVolInnerLbl);
                addR2("Vol. in box:",        m_portFrontVolDisplLbl);
                addR2("Port length:",        m_portFrontLenLbl);
                addR2("(per port):",         m_portFrontLenEachLbl);
                addR2("2nd harmonic:",       m_portFrontF2HLbl);
```
Keep the existing `m_portFrontLenLbl` stylesheet block that follows.

- [ ] **Step 3: Build**

Run: `cd build && cmake --build . --parallel`
Expected: clean build. The 3 new front rows show "–" (no computation yet).

- [ ] **Step 4: Commit**

```bash
git add src/enclosurewidget.h src/enclosurewidget.cpp
git commit -m "feat(ports): add front port air-volume / per-port / 2nd-harmonic result labels"
```

---

## Task 2: Compute the 3 new front results (complete parity)

**Files:** Modify `src/enclosurewidget.cpp` (front computation block ~4011–4119)

- [ ] **Step 1: Compute the 3 values inside the BP6 front block**

In the `if (isBP6(m) && m_portFrontLenLbl)` block, AFTER `const double LpF = Lp_effF - endCorrF;` (~4054) and before the final `m_portFrontLenLbl` length text, add:
```cpp
        // Front port air volume (bore × length, all front ports).
        if (m_portFrontVolInnerLbl) {
            if (LpF > 0)
                m_portFrontVolInnerLbl->setText(QString("%1 L").arg(Nf * Apf * LpF * 1000.0, 0, 'f', 3));
            else
                m_portFrontVolInnerLbl->setText("–");
        }
        // Front per-port length.
        if (m_portFrontLenEachLbl) {
            if (LpF > 0 && Nf > 1)
                m_portFrontLenEachLbl->setText(QString("%1 mm each").arg(LpF * 1000.0, 0, 'f', 0));
            else
                m_portFrontLenEachLbl->setText("–");
        }
        // Front 2nd pipe harmonic: f₂H = c / Lp_eff.
        if (m_portFrontF2HLbl) {
            if (Lp_effF > 0)
                m_portFrontF2HLbl->setText(QString("%1 Hz").arg(g_C / Lp_effF, 0, 'f', 1));
            else
                m_portFrontF2HLbl->setText("–");
        }
```

- [ ] **Step 2: Clear the 3 new labels in the front clear paths**

In the `clearFront` lambda (~4012–4017) add:
```cpp
            if (m_portFrontVolInnerLbl) m_portFrontVolInnerLbl->setText("–");
            if (m_portFrontLenEachLbl)  m_portFrontLenEachLbl ->setText("–");
            if (m_portFrontF2HLbl)      m_portFrontF2HLbl     ->setText("–");
```
And in the `else if (m_portFrontLenLbl)` non-BP6 branch (~4115–4119) add the same three `setText("–")` lines. Also add them to the global reset path near the other `m_portFront*Lbl->setText("–")` resets (~4526–4527).

- [ ] **Step 3: Build and verify by inspection**

Run: `cd build && cmake --build . --parallel && ctest --output-on-failure`
Expected: clean build; existing tests still 100%. (These are inline UI computations mirroring the rear; like the rear results they have no unit test — verified in UAT at the end.)

- [ ] **Step 4: Commit**

```bash
git add src/enclosurewidget.cpp
git commit -m "feat(ports): compute front port air-volume, per-port length, 2nd harmonic"
```

---

## Task 3: Promote Port-tab blocks to members (refactor, no visual change)

Make the movable blocks members so Task 4 can re-home them. The standard arrangement must look **identical** to today after this task.

**Files:** Modify `src/enclosurewidget.h`, `src/enclosurewidget.cpp`

- [ ] **Step 1: Declare block members in `src/enclosurewidget.h`**

Near the other Port-tab widget members, add:
```cpp
    QWidget *m_portColControls = nullptr;  ///< rear tuning/shape/flare column (col1)
    QWidget *m_portDimsBlock   = nullptr;  ///< rear dimension rows (round/rect)
    QWidget *m_portBraceBlock  = nullptr;  ///< rear bracing-surface row
    QWidget *m_portColResults  = nullptr;  ///< rear results grid column (col3)
    QWidget *m_portArrangeHost = nullptr;  ///< host whose layout is rebuilt per enclosure type
```
(`m_portDiagram` and `m_frontPortSection` are already members.)

- [ ] **Step 2: Assign col1 / col3 to members**

In the Port-tab construction, change `auto *col1 = new QWidget;` → `m_portColControls = new QWidget;` and use `m_portColControls` thereafter in that block (the `cols->addWidget(col1)` becomes `cols->addWidget(m_portColControls)`). Likewise `auto *col3 = new QWidget;` → `m_portColResults = new QWidget;` (and its `cols->addWidget(col3)`).

- [ ] **Step 3: Split col2 into dims + diagram + brace blocks**

In `col2`'s construction, wrap the dimension rows and the bracing row as their own member widgets so they can move independently of the diagram. Replace the `col2` inner layout so it reads:
```cpp
        {
            // Rear dimension rows (round + rect) as a movable block.
            m_portDimsBlock = new QWidget;
            auto *dimsV = new QVBoxLayout(m_portDimsBlock);
            dimsV->setContentsMargins(0,0,0,0); dimsV->setSpacing(0);
            dimsV->addWidget(m_portDiamRow);   // existing
            dimsV->addWidget(m_portRectRows);  // existing

            // Bracing-surface row as a movable block.
            m_portBraceBlock = new QWidget;
            auto *braceV = new QVBoxLayout(m_portBraceBlock);
            braceV->setContentsMargins(0,0,0,0); braceV->setSpacing(0);
            braceV->addWidget(/* the existing bracing row widget that holds m_inPortBraceSurf */);
        }
```
Keep `m_portDiamRow` / `m_portRectRows` / the bracing row exactly as constructed today; only their parent wrappers change. Do NOT add them to a `col2` anymore — Task-3 Step 4 builds the host instead.

- [ ] **Step 4: Build the arrangement host and seed it with the standard layout**

Replace the `cols`/`vb->addLayout(cols)` + `vb->addWidget(m_frontPortSection)` section with a single host whose layout `rebuildPortArrangement` owns:
```cpp
        m_portArrangeHost = new QWidget;
        vb->addWidget(m_portArrangeHost);
        // initial arrangement set after construction (standard) — see Task 4
```
Move `vb->addStretch();` to remain after the host. The header `m_portSectionHdr` stays above the host. Do not yet write the arrangement — Task 4 adds `rebuildPortArrangement` and calls it once here with `false` (standard).

- [ ] **Step 5: Build (will be wired in Task 4)**

Run: `cd build && cmake --build . --parallel`
Expected: clean build. NOTE: the Port tab will look empty until Task 4 wires `rebuildPortArrangement` — that's expected within this task pair; do Task 4 immediately after. (If you prefer a green intermediate, you may implement Task 4 Step 1–2 before first running the app.)

- [ ] **Step 6: Commit**

```bash
git add src/enclosurewidget.h src/enclosurewidget.cpp
git commit -m "refactor(ports): promote Port-tab blocks to members + arrangement host"
```

---

## Task 4: `rebuildPortArrangement` — the two arrangements

**Files:** Modify `src/enclosurewidget.h`, `src/enclosurewidget.cpp`

- [ ] **Step 1: Declare the method in `src/enclosurewidget.h`**

```cpp
    void rebuildPortArrangement(bool bp6);  ///< re-home Port-tab blocks per enclosure type
```

- [ ] **Step 2: Implement it in `src/enclosurewidget.cpp`**

```cpp
void EnclosureWidget::rebuildPortArrangement(bool bp6)
{
    if (!m_portArrangeHost) return;

    // Detach persistent blocks from whatever layout currently holds them.
    QWidget *blocks[] = { m_portColControls, m_portDimsBlock, m_portDiagram,
                          m_portBraceBlock, m_portColResults, m_frontPortSection };
    for (QWidget *b : blocks) if (b) b->setParent(nullptr);

    // Delete the old host layout (and any transient wrapper widgets it owned).
    if (QLayout *old = m_portArrangeHost->layout()) {
        QLayoutItem *it;
        while ((it = old->takeAt(0)) != nullptr) {
            if (QWidget *w = it->widget()) w->deleteLater();  // transient wrappers only
            delete it;
        }
        delete old;
    }

    auto mkVDiv = [] {
        auto *d = new QFrame; d->setFrameShape(QFrame::VLine);
        d->setFrameShadow(QFrame::Plain);
        d->setStyleSheet("color: rgba(127,127,127,0.3);");
        return d;
    };

    if (bp6) {
        auto *row = new QHBoxLayout(m_portArrangeHost);
        row->setContentsMargins(0,0,0,0); row->setSpacing(6);

        // Left: rear controls + dims + brace + results, stacked.
        auto *left = new QWidget;
        auto *lv = new QVBoxLayout(left);
        lv->setContentsMargins(0,0,0,0); lv->setSpacing(6);
        if (m_portColControls) lv->addWidget(m_portColControls);
        if (m_portDimsBlock)   lv->addWidget(m_portDimsBlock);
        if (m_portBraceBlock)  lv->addWidget(m_portBraceBlock);
        if (m_portColResults)  lv->addWidget(m_portColResults);
        lv->addStretch();
        row->addWidget(left);

        row->addWidget(mkVDiv());
        if (m_portDiagram) row->addWidget(m_portDiagram, 1);   // middle: diagram only
        row->addWidget(mkVDiv());

        if (m_frontPortSection) { m_frontPortSection->setVisible(true); row->addWidget(m_frontPortSection); }
    } else {
        // Standard (vented / BP4 / sealed): original side-by-side columns.
        auto *cols = new QHBoxLayout(m_portArrangeHost);
        cols->setContentsMargins(0,0,0,0); cols->setSpacing(0);
        if (m_portColControls) cols->addWidget(m_portColControls);
        cols->addWidget(mkVDiv()); cols->addSpacing(6);

        auto *mid = new QWidget;          // col2 = dims + diagram + brace
        auto *mv = new QVBoxLayout(mid);
        mv->setContentsMargins(0,0,0,0); mv->setSpacing(0);
        if (m_portDimsBlock)  mv->addWidget(m_portDimsBlock);
        if (m_portDiagram)    mv->addWidget(m_portDiagram);
        if (m_portBraceBlock) mv->addWidget(m_portBraceBlock);
        mv->addStretch();
        cols->addWidget(mid);

        cols->addSpacing(6); cols->addWidget(mkVDiv()); cols->addSpacing(6);
        if (m_portColResults) cols->addWidget(m_portColResults);
        cols->addStretch();

        if (m_frontPortSection) m_frontPortSection->setVisible(false);
    }
}
```
(If `<QFrame>`/`<QLayoutItem>` aren't already included in the .cpp, add them.)

- [ ] **Step 3: Seed the initial arrangement in construction**

At the spot from Task 3 Step 4 (right after `vb->addWidget(m_portArrangeHost)`), add:
```cpp
        rebuildPortArrangement(false);   // standard until a model is selected
```

- [ ] **Step 4: Call it on enclosure-type change**

At the two spots that currently do `m_frontPortSection->setVisible(bp6)` (~3684 and ~4300), replace that single line with:
```cpp
        rebuildPortArrangement(bp6);
```
(`bp6` is already in scope there as `isBP6(...)`; if the local is named differently, pass `isBP6(m)`/`isBP6(model)` accordingly. `rebuildPortArrangement` sets the front-section visibility itself, so the old `setVisible` line is fully replaced.)

- [ ] **Step 5: Build and run — VISUAL VERIFICATION (iterate here)**

Run: `cd build && cmake --build . --parallel && ./TSBoss`
This is the regression-prone step — verify by eye and adjust spacing/stretch if needed:
- **Vented**: Port tab looks exactly as before (controls | dims+diagram+brace | results).
- **BP4**: looks as before (no front section).
- **BP6**: three columns — rear (controls+dims+brace+results) | diagram | front (inputs+results).
- Switch Vented → BP6 → BP4 → BP6 repeatedly: no crash, no orphaned/duplicated/blank blocks, no widget appearing twice. Confirm the diagram is in exactly one place each mode.
- Editing rear/front spinboxes still updates results (signals preserved through reparenting).

- [ ] **Step 6: Commit**

```bash
git add src/enclosurewidget.h src/enclosurewidget.cpp
git commit -m "feat(ports): BP6 three-column rear|diagram|front Port-tab arrangement"
```

---

## Task 5: UAT and doc commit

**Files:** (none — verification + committing the spec/plan docs)

- [ ] **Step 1: Full UAT**

`cd build && ./TSBoss`. Confirm:
- BP6: 3 columns, rear results AND front results each show all 7 rows; front values change with front fb/shape/dims; rear with rear inputs.
- Vented & BP4 Port tabs visually identical to before this work.
- Repeated type switching is stable; light & dark themes both correct.

- [ ] **Step 2: Commit the design + plan docs**

```bash
git add docs/superpowers/specs/2026-05-24-bp6-port-tab-layout-design.md \
        docs/superpowers/plans/2026-05-24-bp6-port-tab-layout.md
git commit -m "docs: BP6 Port-tab layout design spec + implementation plan"
```

---

## Self-Review (completed)

- **Spec coverage:** front parity labels (Task 1) + computation (Task 2) covers the 3 added results; 3-column BP6 arrangement with rear-left/diagram-middle/front-right (Tasks 3–4); vented/BP4 unchanged (standard branch of `rebuildPortArrangement`, verified in UAT); reparenting risk handled via detach-then-rebuild with idempotent `rebuildPortArrangement` (Task 4 Step 2); BP6-only (Task 5). All spec sections mapped.
- **Placeholder scan:** one intentional descriptive reference — Task 3 Step 3 "the existing bracing row widget that holds `m_inPortBraceSurf`" — the implementer locates that exact widget in the current col2 construction (it is the `extraRow` built around `m_inPortBraceSurf`). All other steps have complete code.
- **Type consistency:** `m_portColControls`, `m_portDimsBlock`, `m_portBraceBlock`, `m_portColResults`, `m_portArrangeHost`, `rebuildPortArrangement(bool)`, and the three `m_portFront*Lbl` members are declared in Tasks 1/3/4 and used consistently. `m_portDiagram`/`m_frontPortSection` are pre-existing members.
- **Testing honesty:** front-result computations are inline UI code mirroring the un-unit-tested rear results; verification is the existing `portphysics_tests` (regression guard) plus visual UAT — matching the codebase's existing approach for these result labels. The reparenting is inherently visual and is explicitly an iterate-and-verify step.
- **Risk note:** Task 3 leaves the Port tab non-functional until Task 4 wires `rebuildPortArrangement`; Tasks 3 and 4 should be executed back-to-back (or merged if the executor prefers a single green checkpoint).
