# SPL Applied Power Level Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an "APPLIED POWER" spinbox to the SPL Response tab that shifts the plotted dB curves by `+10·log10(P)` and updates the Y-axis label dynamically.

**Architecture:** `ResponsePlot` gains a `m_power` member and `setPower()` method; its `paintEvent` applies a uniform dB offset derived from that power. `EnclosureWidget::buildUi()` wraps `m_splPlot` in a container widget with the spinbox — identical structure to the Voltage/Excursion/Port Velocity tabs — and adds `m_splPower` to the existing four-way sync chain.

**Tech Stack:** Qt6, C++17, CMake. No external libraries.

---

## Files

- Modify: `src/enclosurewidget.h` — add `setPower`/`m_power` to `ResponsePlot`; add `m_splPower` to `EnclosureWidget`
- Modify: `src/enclosurewidget.cpp` — implement `setPower`, update `paintEvent`, wrap SPL tab, extend sync chain

---

### Task 1: Add `setPower` / `m_power` to `ResponsePlot` header

**Files:**
- Modify: `src/enclosurewidget.h:125–147`

- [ ] **Step 1: Edit `ResponsePlot` class declaration**

In `src/enclosurewidget.h`, the `ResponsePlot` class currently ends at the `private:` section (around line 142–147). Add the `setPower` declaration after `resetYRange()` and add `m_power` to the private members:

```cpp
// after resetYRange():
    void setPower(double watts) { m_power = watts; update(); }

// in private: section, after m_yMin, m_yMax:
    double                m_power      = 1.0;
```

Full updated class (replace lines 125–147):

```cpp
class ResponsePlot : public QWidget
{
    Q_OBJECT
public:
    explicit ResponsePlot(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void clear();
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    void resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }
    void setPower(double watts)            { m_power = watts; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

private:
    QList<BoxModel>       m_models;
    int                   m_activeIdx  = -1;
    double                m_cursorFreq = -1.0;
    std::optional<double> m_yMin, m_yMax;
    double                m_power      = 1.0;
};
```

- [ ] **Step 2: Add `m_splPower` to `EnclosureWidget` member list**

In `src/enclosurewidget.h`, find the power spinbox members (around line 462–464):

```cpp
    QDoubleSpinBox   *m_power     = nullptr;
    QDoubleSpinBox   *m_excPower  = nullptr;
    QDoubleSpinBox   *m_pvPower   = nullptr;
```

Add `m_splPower` after `m_pvPower`:

```cpp
    QDoubleSpinBox   *m_power     = nullptr;
    QDoubleSpinBox   *m_excPower  = nullptr;
    QDoubleSpinBox   *m_pvPower   = nullptr;
    QDoubleSpinBox   *m_splPower  = nullptr;
```

- [ ] **Step 3: Build to verify header compiles**

```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -5
```

Expected: no errors (no implementation change yet, so behaviour unchanged).

---

### Task 2: Apply power offset in `ResponsePlot::paintEvent`

**Files:**
- Modify: `src/enclosurewidget.cpp` — `ResponsePlot::paintEvent`

The `paintEvent` has two kinds of dB computations:
- Vented/ported: `const double db = m.spl + 20.0*std::log10(raw / ref);`
- Sealed/bandpass: `const double db = m.spl + 10.0*std::log10(xsq*xsq/den);`

And the Y-axis label: `"SPL  (dB, 1 W / 1 m)"` (around line 666).

- [ ] **Step 1: Add power offset variable near top of `paintEvent`**

At the very start of `ResponsePlot::paintEvent`, just after the opening brace and before any drawing code, add:

```cpp
const double pwrOffset = (m_power > 0.0) ? 10.0 * std::log10(m_power) : 0.0;
```

- [ ] **Step 2: Apply offset to vented/ported dB computations**

Every line of the form:
```cpp
const double db  = m.spl + 20.0*std::log10(raw / ref);
```
becomes:
```cpp
const double db  = m.spl + pwrOffset + 20.0*std::log10(raw / ref);
```

There are multiple occurrences (minimap scan and full paintEvent). Use find-and-replace carefully — only the `db =` assignments inside `paintEvent`, not any other function. All occurrences take the same form; update each one.

- [ ] **Step 3: Apply offset to sealed/bandpass dB computations**

Every line of the form:
```cpp
const double db  = m.spl + 10.0*std::log10(xsq*xsq/den);
```
becomes:
```cpp
const double db  = m.spl + pwrOffset + 10.0*std::log10(xsq*xsq/den);
```

Same search scope: only inside `ResponsePlot::paintEvent`.

- [ ] **Step 4: Update the Y-axis label to be dynamic**

Find (around line 666):
```cpp
Qt::AlignCenter, "SPL  (dB, 1 W / 1 m)");
```

Replace with:
```cpp
Qt::AlignCenter,
    QString("SPL  (dB, %1 W / 1 m)").arg(m_power, 0, 'g', 3));
```

- [ ] **Step 5: Build**

```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -5
```

Expected: no errors.

---

### Task 3: Wrap SPL tab and wire up `m_splPower`

**Files:**
- Modify: `src/enclosurewidget.cpp` — `EnclosureWidget::buildUi()`

The current SPL tab registration (around line 2210):
```cpp
m_plotTabs->addTab(m_splPlot, "SPL");           // index 0
```

needs to become a wrapper widget identical in structure to `voltTab`, `excTab`, `pvTab`.

- [ ] **Step 1: Replace direct SPL tab registration with wrapped `splTab`**

Find and replace the block around lines 2206–2210 where `m_plotTabs` is set up. Before the `m_plotTabs = new QTabWidget;` line, insert the `splTab` construction:

```cpp
// SPL tab: power control + plot
auto *splTab = new QWidget;
{
    auto *vb = new QVBoxLayout(splTab);
    vb->setContentsMargins(6, 6, 6, 0);
    vb->setSpacing(4);
    auto *hb = new QHBoxLayout;
    auto *pwrLbl = new QLabel("APPLIED POWER");
    pwrLbl->setStyleSheet(
        "color:#D97706; font-family:'IBM Plex Sans',sans-serif;"
        "font-weight:600; font-size:8.5pt; letter-spacing:1.5px;"
        "padding-right:8px;");
    m_splPower = mkPowerSpin();
    hb->addWidget(pwrLbl); hb->addWidget(m_splPower); hb->addStretch();
    vb->addLayout(hb);
    vb->addWidget(m_splPlot, 1);
    connect(m_splPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        m_splPlot->setPower(v);
        m_vpPlot->setPower(v);
        m_excPlot->setPower(v);
        m_pvPlot->setPower(v);
        for (auto *s : {m_power, m_excPower, m_pvPower}) if (s) {
            s->blockSignals(true); s->setValue(v); s->blockSignals(false);
        }
    });
}
```

Then change:
```cpp
m_plotTabs->addTab(m_splPlot, "SPL");           // index 0
```
to:
```cpp
m_plotTabs->addTab(splTab,    "SPL");           // index 0
```

- [ ] **Step 2: Extend the three existing sync handlers to include `m_splPower` and `m_splPlot`**

**Voltage tab handler** — find the lambda connected to `m_power::valueChanged`:
```cpp
connect(m_power, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double v) {
    m_vpPlot->setPower(v);
    m_excPlot->setPower(v);
    m_pvPlot->setPower(v);
    for (auto *s : {m_excPower, m_pvPower}) if (s) {
        s->blockSignals(true); s->setValue(v); s->blockSignals(false);
    }
});
```

Replace with:
```cpp
connect(m_power, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double v) {
    m_vpPlot->setPower(v);
    m_excPlot->setPower(v);
    m_pvPlot->setPower(v);
    m_splPlot->setPower(v);
    for (auto *s : {m_excPower, m_pvPower, m_splPower}) if (s) {
        s->blockSignals(true); s->setValue(v); s->blockSignals(false);
    }
});
```

**Excursion tab handler** — find the lambda connected to `m_excPower::valueChanged`:
```cpp
connect(m_excPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double v) {
    m_excPlot->setPower(v);
    m_vpPlot->setPower(v);
    m_pvPlot->setPower(v);
    for (auto *s : {m_power, m_pvPower}) if (s) {
        s->blockSignals(true); s->setValue(v); s->blockSignals(false);
    }
});
```

Replace with:
```cpp
connect(m_excPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double v) {
    m_excPlot->setPower(v);
    m_vpPlot->setPower(v);
    m_pvPlot->setPower(v);
    m_splPlot->setPower(v);
    for (auto *s : {m_power, m_pvPower, m_splPower}) if (s) {
        s->blockSignals(true); s->setValue(v); s->blockSignals(false);
    }
});
```

**Port Velocity tab handler** — find the lambda connected to `m_pvPower::valueChanged`:
```cpp
connect(m_pvPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double v) {
    m_pvPlot->setPower(v);
    m_vpPlot->setPower(v);
    m_excPlot->setPower(v);
    for (auto *s : {m_power, m_excPower}) if (s) {
        s->blockSignals(true); s->setValue(v); s->blockSignals(false);
    }
});
```

Replace with:
```cpp
connect(m_pvPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
        this, [this](double v) {
    m_pvPlot->setPower(v);
    m_vpPlot->setPower(v);
    m_excPlot->setPower(v);
    m_splPlot->setPower(v);
    for (auto *s : {m_power, m_excPower, m_splPower}) if (s) {
        s->blockSignals(true); s->setValue(v); s->blockSignals(false);
    }
});
```

- [ ] **Step 3: Push initial 100 W into `m_splPlot` in the initial sync block**

Find the block (around line 2200):
```cpp
{
    const double w = m_power->value();  // all three spins agree
    m_vpPlot ->setPower(w);
    m_excPlot->setPower(w);
    m_pvPlot ->setPower(w);
}
```

Replace with:
```cpp
{
    const double w = m_power->value();  // all four spins agree
    m_vpPlot ->setPower(w);
    m_excPlot->setPower(w);
    m_pvPlot ->setPower(w);
    m_splPlot->setPower(w);
}
```

- [ ] **Step 4: Build**

```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -10
```

Expected: no errors.

---

### Task 4: Manual verification

- [ ] **Step 1: Run the app**

```bash
cd /home/wessel/projects/TSBoss/build && ./TSBoss
```

- [ ] **Step 2: Verify SPL tab shows "APPLIED POWER" spinbox**

Open the Enclosure Modeller. Select the SPL tab. Confirm the "APPLIED POWER" label and spinbox appear above the plot, styled identically to the Voltage/Excursion/Port Velocity tabs.

- [ ] **Step 3: Verify dB shift**

With a model loaded, note the peak SPL at 1 W. Change power to 100 W. Curves should shift up by exactly 20 dB (`10·log10(100) = 20`). Change to 10 W → 10 dB shift.

- [ ] **Step 4: Verify Y-axis label updates**

At 1 W the label reads `"SPL  (dB, 1 W / 1 m)"`. At 100 W: `"SPL  (dB, 100 W / 1 m)"`. At 1500 W: `"SPL  (dB, 1.5e+03 W / 1 m)"`.

- [ ] **Step 5: Verify sync**

Switch to the Voltage tab and change its power spinbox. Switch back to SPL tab — the spinbox there should show the same value and the curves should be shifted accordingly.

- [ ] **Step 6: Commit**

```bash
cd /home/wessel/projects/TSBoss
git add src/enclosurewidget.h src/enclosurewidget.cpp docs/superpowers/specs/2026-05-07-spl-applied-power-design.md docs/superpowers/plans/2026-05-07-spl-applied-power.md
git commit -m "feat: add applied power level control to SPL Response tab"
```
