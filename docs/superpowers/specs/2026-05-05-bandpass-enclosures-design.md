# Bandpass Enclosure Support — Design

**Date:** 2026-05-05
**Status:** Approved for implementation planning
**Scope:** Add 4th-order and 6th-order bandpass enclosure types to TSBoss, with full numerical acoustic models and dedicated UI restructure.

## Goal

Extend the EnclosureWidget to support two new enclosure types:

- **Bandpass 4th-order (BP4):** sealed rear chamber + vented front chamber; port-only radiation.
- **Bandpass 6th-order (BP6):** vented rear chamber + vented front chamber; sum of both ports radiates.

Both must support the existing plot suite (SPL, Group Delay, Voltage, Excursion, Port Velocity) using the same `std::complex<double>` numerical-circuit approach used for the current vented model.

## Non-goals (v1)

- Bandpass-specific alignment presets (B2/B4/Bessel buttons stay only on Driver tab for sealed/vented).
- Port resonance / pipe-coloration warnings for the second port.
- Bandpass-aware Xmax/Vd checks beyond what the existing cone-excursion logic already does.

## 1. Data model — `BoxModel` (enclosurewidget.h)

Replace `bool isVented` with an enum:

```cpp
enum class EncType { Sealed, Vented, IB, Bandpass4, Bandpass6 };
EncType encType = EncType::Sealed;
```

Add front-chamber fields (existing `volumeL`, `fb`, `QL`, `portShape`, `portWidth_mm`, `portHeight_mm`, `portSharedWall` become the **rear** chamber for bandpass — sealed-only for BP4, vented for BP6):

```cpp
double volumeFront_L      = 0.0;   // BP4 + BP6
double fbFront            = 0.0;   // BP4 + BP6
double QLFront            = 7.0;
int    portFrontShape     = 0;     // 0 = round, 1 = rect
double portFrontWidth_mm  = 0.0;
double portFrontHeight_mm = 0.0;
bool   portFrontSharedWall = false;

// Bandpass results
double f3Low            = 0.0;
double f3High           = 0.0;
double passbandRippleDb = 0.0;
double peakSpl          = 0.0;
```

A code comment will mark that for bandpass types the original (un-prefixed) volume/port fields refer to the **rear** chamber.

## 2. Acoustic math (enclosurewidget.cpp)

### BP4

Driver loaded by parallel:
- Sealed rear compliance `Cr = Vr / (ρ·c²)`
- Vented front Helmholtz network (mass `Map_f`, compliance `Cf = Vf / (ρ·c²)`, leakage `R_QL_f`)

Solve coupled equations (extension of existing `portedAmplitudes`) for cone velocity `v` and front-port volume velocity `Up_f`. Output:

```
SPL_BP4(f) = ω · |Up_f|
```

### BP6

Driver loaded by two Helmholtz networks (rear and front). Solve for cone velocity `v`, rear-port volume velocity `Up_r`, front-port volume velocity `Up_f`. Output:

```
SPL_BP6(f) = ω · |Up_r + Up_f|
```

### New helpers

```cpp
struct BP4Amplitudes { std::complex<double> coneVol, frontPort; };
struct BP6Amplitudes { std::complex<double> coneVol, rearPort, frontPort; };

BP4Amplitudes bandpass4Amplitudes(const BoxModel &m, double f);
BP6Amplitudes bandpass6Amplitudes(const BoxModel &m, double f);
```

F3 low / F3 high computed via binary search above and below the SPL peak.
Group delay computed from numerical derivative of phase of total-output velocity.
Passband ripple = max(SPL) − min(SPL within −3 dB band).

## 3. UI restructure

The current "Model" parameter tab is split into two new tabs and the Port tab gets sub-tabs:

### Driver tab (renamed from Model)
- Driver combo box
- T/S parameters (editable on double-click): Fs, Vas, Qts, Qes
- Secondary parameters (locked): Qms, Re, Mms, BL, Sd
- Alignment preset buttons: B2 / B4 / Bessel (kept here)
- "View Driver Params" + "Reset to Driver" actions

### Chambers tab (new)
- **Enclosure type combo:** Sealed / Vented (Ported) / Infinite Baffle / Bandpass 4th-order / Bandpass 6th-order
- **Volume controls:**
  - Sealed / Vented: single Vb spinbox (as today)
  - IB: hidden — replaced by a centered large "∞" glyph (~64 pt IBM Plex Sans, accent `#D97706`) with a small caption "Infinite Baffle — no enclosure"
  - BP4 / BP6: two spinboxes — `Vr` (rear) and `Vf` (front)
- **Results group** (dynamic labels):
  - Sealed: Fc, Qtc, F3, η
  - Vented: fb, F3, η
  - IB: Fs, Qts (free-air), η
  - BP4 / BP6: F3 low, F3 high, passband ripple, peak SPL

### Port tab (sub-tabs for bandpass)
- Sealed / IB: tab disabled (as today)
- Vented: single page — current content
- BP4: single page labelled "Front Port" (rear is sealed, no port)
- BP6: two sub-tabs — "Rear Port" / "Front Port", each containing the existing port-tab fields bound to the corresponding chamber's port fields

## 4. Plots

| Plot          | Sealed | Vented           | BP4                            | BP6                                                            |
|---------------|--------|------------------|--------------------------------|----------------------------------------------------------------|
| SPL           | cone   | cone+port+total  | front port (= total)           | rear + front ports (separate dashed) + total                   |
| Group Delay   | ✓      | ✓                | ✓ (front-port phase deriv)     | ✓ (summed-port phase deriv)                                    |
| Voltage       | ✓      | ✓                | ✓                              | ✓                                                              |
| Excursion     | cone   | cone             | cone (loaded by both chambers) | cone (loaded by both chambers)                                 |
| Port Velocity | —      | front port       | front port                     | rear + front, both as curves on the same plot, label-suffixed  |

Port Velocity for BP6 reuses the existing multi-model overlay machinery — each model contributes up to two curves with names suffixed `" (rear)"` / `" (front)"`, and existing color-cycling/dimming applies per-curve.

## 5. JSON / persistence migration

- `serializeModels()` writes `encType` as string ("sealed", "vented", "ib", "bandpass4", "bandpass6").
- `deserializeModels()` reads `encType` if present; otherwise falls back to the legacy `isVented` bool — `true` → Vented, `false` → Sealed.
- Old saved projects load without modification; new BP fields default to 0/false on missing keys.
- DriverDatabase schema unchanged.

## 6. Codebase impact

- ~25 `if (m.isVented)` call sites in enclosurewidget.cpp need updating to switch on `m.encType`.
- Helper `bool isPorted(const BoxModel &m)` introduced for "needs port math" checks (true for Vented, BP4, BP6).
- Existing `portedAmplitudes` reused for the Vented case; new `bandpass4Amplitudes` / `bandpass6Amplitudes` added next to it.
- `updateModelList()` tag string extended to render bandpass info, e.g. `[BP4 fb=42.0, F3=38–95]`.
- `m_resLblFc` / `m_resLblQtc` label-swap logic extended to handle bandpass result labels.
- `m_plotTabs->setTabEnabled(portVelocityIdx, …)` uses `isPorted(m)` instead of `m.isVented`.

## 7. Risks & mitigations

- **Numerical stability of BP6 solver near tuning frequencies:** add small leakage damping (existing `QL` machinery) and clamp denominators away from zero in the complex solve.
- **UI density on Chambers tab:** if cramped, allow the volume + results group to wrap into two columns when bandpass is selected.
- **JSON back-compat:** verified by including a fallback path for legacy `isVented` reads.

## 8. Files to touch

- `src/enclosurewidget.h` — `EncType` enum, `BoxModel` field additions, member additions for new UI widgets.
- `src/enclosurewidget.cpp` — math helpers, UI restructure, plot updates, JSON migration, all `isVented` call sites.

No changes to: `driverdb.*`, `driverrecord.h`, `tscalculator.*`, wizard, mainwindow.
