# TSBoss — Claude Code Project Context

## What this is
A Qt6 / C++17 desktop application for measuring, storing, and modelling Thiele/Small loudspeaker driver parameters. Three main areas:

1. **Driver database** — enter raw measurements, compute T/S parameters, store in SQLite.
2. **Enclosure modelling** — multi-model sealed / vented box comparison with SPL, group delay, and voltage plots.
3. **Wizard** — guided step-by-step T/S measurement workflow.

## Build
```
cd /home/wessel/projects/TSBoss/build
cmake --build . --parallel
./TSBoss
```
Source root: `/home/wessel/projects/TSBoss/src`
Build root:  `/home/wessel/projects/TSBoss/build`

## Tech stack
- Qt 6, CMake, C++17
- SQLite via Qt's `QSqlDatabase` / `QSqlQuery`
- No external audio or DSP libraries — all acoustics done analytically or with `std::complex<double>`

## Key files and their roles

| File | Role |
|------|------|
| `driverrecord.h` | **Central data struct** `DriverRecord` — all raw measurements + computed T/S params |
| `driverdb.h/cpp` | SQLite persistence: `saveDriver`, `loadDrivers`, migration via `ALTER TABLE ADD COLUMN` |
| `tscalculator.h/cpp` | Pure T/S math (static methods, `PI`, `C` constants used everywhere) |
| `datasheetentrywidget.h/cpp` | Full driver entry/edit form; computes Vd hint live |
| `quickentrywidget.h/cpp` | Minimal fast-entry form |
| `driverlistwidget.h/cpp` | Driver list with CSV import/export |
| `driverdetailwidget.h/cpp` | Read-only detail view of all parameters |
| `enclosurewidget.h/cpp` | **Enclosure modeller** — BoxModel, all three plots, port calculator |
| `tswizard.h/cpp` | Step-by-step measurement wizard |
| `mainwindow.h/cpp` | Top-level QMainWindow with QStackedWidget navigation |

## DriverRecord fields (driverrecord.h)
### Raw measurements
`Re, fs, Zmax, f1, f2, deltaM(kg), fo, Dd(m), Zmin, f3`
### Computed T/S params
`Z12, R0, fsVerify, Qms, Qes, Qts, mms(kg), Cms(m/N), Rms, BL, Sd(m²), Vas(m³), Le`
### Additional linear params
`Znom, fLe, KLe`
### Large signal params
`Xmax(mm), Xlim(mm), Pe(W), Hg(mm), Vd(cm³)` — Vd is auto-calculated as Sd·Xmax in the UI

## BoxModel fields (enclosurewidget.h)
```cpp
// Driver T/S (editable, double-click to unlock)
fs, Vas_L, Qts, Qes
// Secondary driver params (locked spinboxes)
Qms, Re, mms_g, BL, Sd_cm2
// Enclosure
volumeL, isVented, fb, QL
// Port geometry (length calculation only, not acoustic sim)
portShape (0=round, 1=rect), portWidth_mm, portHeight_mm, portSharedWall
// Computed results
alpha, Fc, Qtc, f3, eta, spl
```

## Enclosure acoustics (enclosurewidget.cpp)
- **Sealed**: analytical — `Fc = fs·√(α+1)`, `Qtc = Qts·√(α+1)`, closed-form f3
- **Vented**: complex admittance circuit (`std::complex<double>`)
  - `portedAmplitudes(m, f)` returns `{Sd·v [cone], Up [port]}` separately
  - Total SPL = `ω·|cone + port|` (coherent sum — they can be out of phase!)
  - Cone/port shown as separate dashed/dotted curves on SPL plot
  - `portedF3()` binary-searches for −3 dB point
  - Group delay via numerical phase derivative of Ua
- **Port length formula**: `Lp = Map·Ap/ρ − Δl`
  - Standard end correction: `Δl = 0.732·De` (one flanged + one unflanged)
  - Shared wall: `Δl = 0.8216·De` (both flanged)
  - Rectangular port equivalent diameter: `De = 2√(Ap/π)`

## UI layout
```
QMainWindow
└── QStackedWidget
    ├── page 0: QSplitter(H) — driver list | driver detail
    ├── page 1: DatasheetEntryWidget
    ├── page 2: QuickEntryWidget
    ├── page 3: EnclosureWidget
    └── page 4: TSWizard
```

EnclosureWidget inner layout:
```
QSplitter(H)
├── Left panel (model list, file I/O)
└── QSplitter(V)
    ├── m_plotTabs (SPL Response | Group Delay | Voltage)
    └── m_paramTabs (Model | Port)
        ├── Model tab: driver combo, T/S params, box volume+type, secondary params, results
        └── Port tab: fb, QL, shape, dimensions, shared wall, computed area+length
```

## Database schema notes
- Migration via `ALTER TABLE ADD COLUMN` — always add new columns at the end with defaults.
- `driverdb.cpp` has a `migrations[]` array; add new entries there.

## Code conventions
- Qt signal/slot, no raw new without parent (except layouts).
- `m_updating` guard prevents re-entrant `onParamChanged` calls.
- T/S spinboxes are read-only by default; double-click triggers a QMessageBox confirmation before unlocking (`lockTsFields` / `unlockTsFields`).
- All physical quantities in SI internally; display conversions happen at the UI boundary.
- Constants `PI`, `C` from `TSCalculator`; `RHO = 1.2` (air density) local to enclosurewidget.cpp.
- Colour palette `kPalette[]` in enclosurewidget.cpp — 8 colours, cycles.
- Plot classes (`ResponsePlot`, `GroupDelayPlot`, `VoltagePlot`) are standalone `QWidget` subclasses with `paintEvent` doing all drawing — no third-party plot library.
