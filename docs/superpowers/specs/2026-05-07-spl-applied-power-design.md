# SPL Applied Power Level — Design Spec
_2026-05-07_

## Summary

Add an "APPLIED POWER" spinbox to the SPL Response tab, matching the existing pattern on the Voltage, Excursion, and Port Velocity tabs. The SPL curve shifts by `+10·log10(P)` dB when power P ≠ 1 W, and the Y-axis label updates dynamically to reflect the selected power.

---

## Changes

### `ResponsePlot` (`enclosurewidget.h` / `enclosurewidget.cpp`)

- Add `double m_power = 1.0;` member.
- Add `void setPower(double watts);` — stores value, calls `update()`.
- In `paintEvent`, offset every computed dB value by `+10.0 * std::log10(m_power)` before mapping to pixel coordinates.
- Replace the hard-coded Y-axis label `"SPL  (dB, 1 W / 1 m)"` with a dynamic string:
  ```cpp
  QString("SPL  (dB, %1 W / 1 m)").arg(m_power, 0, 'g', 3)
  ```

### `EnclosureWidget` (`enclosurewidget.h` / `enclosurewidget.cpp`)

**Header:** Add `QDoubleSpinBox *m_splPower = nullptr;`

**`buildUi()` — SPL tab wrapper:**

Replace the direct `m_splPlot` tab registration with a `splTab` container widget:
```
splTab (QWidget)
└── QVBoxLayout (margins 6,6,6,0 / spacing 4)
    ├── QHBoxLayout
    │   ├── QLabel "APPLIED POWER"  (same stylesheet as other tabs)
    │   ├── m_splPower (mkPowerSpin(), default 100 W)
    │   └── stretch
    └── m_splPlot (stretch factor 1)
```

`m_plotTabs->addTab(splTab, "SPL")` — index 0 unchanged.

**Sync chain:** `m_splPower::valueChanged` updates `m_vpPlot`, `m_excPlot`, `m_pvPlot`, `m_splPlot` via `setPower()`, and mirrors value into `m_power`, `m_excPower`, `m_pvPower` with `blockSignals`. The three existing handlers each gain a matching mirror into `m_splPower` and a `m_splPlot->setPower(v)` call.

**Initial sync:** The existing block that pushes 100 W into the three plots gains `m_splPlot->setPower(w)`.

---

## Constraints

- Spinbox range / step / suffix: 0.1–10 000 W, 1 W step, " W" suffix — identical to existing spinboxes.
- Default power: 100 W (matching existing tabs).
- All four spinboxes always agree; changing power on any tab propagates to all.
- No per-model power level — one global power applies to all models on the SPL plot.
- Y-axis label uses `'g'` format with 3 significant figures (e.g., `"100 W"`, `"1.5 W"`, `"2e+03 W"`).
