# Port Friction & Compression Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the vented-box port loss velocity-dependent so SPL, impedance, group-delay and port-velocity curves bend realistically with drive level (port compression), while a tuning-shift hook stays wired-but-off for later validation.

**Architecture:** Extract the ported acoustic math into a header-only `portphysics.h` (so it is unit-testable without Qt Widgets), add a single velocity-aware port impedance `portZ(m,f,u)`, and replace the closed-form solve with a per-frequency fixed-point `portedSolve(m,f,power)`. All existing `ported*` functions become thin wrappers over `portedSolve`. The visible behaviour change is isolated to a single function (`flareK`) flipped on in the final task.

**Tech Stack:** C++17, Qt 6 (Core for the data model, Widgets for the UI), CMake, `std::complex<double>`. No new third-party libraries.

---

## Design Notes (read before starting)

- **K mapping (reusing `BoxModel::portFlare`):** `0 = Straight → K = 1.0`, `1 = One end flared → K = 0.6`, `2 = Both ends flared → K = 0.2`. (The spec's abstract `Sharp/Radiused/Flared` table maps onto the existing field this way.)
- **Regression anchor:** at `power → 0` the turbulent term vanishes (`u → 0`), so `portedSolve` must reproduce the legacy small-signal result exactly. This is the permanent golden-test invariant, since `flareK(0) = 1.0` means there is no "K=0" config after the feature lands.
- **Per-1 V amplitudes:** `portCore` returns cone/port volume velocities for a 1 V drive. The drive voltage `V = √(power·Zin)` only feeds the *velocity* used by the loss model — SPL plots normalise by a 1 kHz reference at the same power, so absolute V scaling cancels there.
- **`cᵥ` (tuning-shift hook):** new `BoxModel::mapVelCoeff`, default `0.0`, no UI in v1. Present so the acoustic-mass term can be activated later without another refactor.

## File Structure

- **Create `src/boxmodel.h`** — the `struct BoxModel` POD data model (moved out of `enclosurewidget.h`). Depends only on `<QString>`.
- **Create `src/portphysics.h`** — header-only ported acoustic math: `g_C`, `RHO`, `setSpeedOfSound`, `driverScaling`, `hasPortedData`, `portArea_m2`, `flareK`, `portZ`, `portCore`, `portedSolve`, and the `ported*` wrappers. Depends on `boxmodel.h`, `tscalculator.h`, `<complex>`, `<cmath>`.
- **Modify `src/enclosurewidget.h`** — remove the `BoxModel` definition, add `#include "boxmodel.h"`.
- **Modify `src/enclosurewidget.cpp`** — remove the moved free functions + globals, add `#include "portphysics.h"`, thread `power` through plot call sites, route the speed-of-sound setter through `setSpeedOfSound`.
- **Create `tests/portphysics_tests.cpp`** — assertion-based test (no framework).
- **Modify `CMakeLists.txt`** — add the test executable + `enable_testing()`/`add_test`.

---

## Task 1: Extract `BoxModel` into `boxmodel.h`

Pure mechanical move. No behaviour change. This decouples the data model from the Qt-Widgets header so the physics header can be tested with Qt Core only.

**Files:**
- Create: `src/boxmodel.h`
- Modify: `src/enclosurewidget.h` (remove struct, add include)
- Modify: `CMakeLists.txt:14` (add `src/boxmodel.h` to `PROJECT_SOURCES`)

- [ ] **Step 1: Identify the exact struct bounds**

Run: `grep -n "struct BoxModel" src/enclosurewidget.h` and find the matching closing `};`. The struct begins at the `struct BoxModel` line and ends at its closing brace (it contains the `EncType`, `WiringMode` enums and all driver/enclosure/port fields, ending before the widget class declarations).

- [ ] **Step 2: Create `src/boxmodel.h`**

Create the file with the include guard and `<QString>`, then **move the entire `struct BoxModel { ... };` block verbatim** from `enclosurewidget.h` into it. Skeleton (fill the body with the moved struct exactly as-is):

```cpp
#pragma once
#include <QString>

// Plain data model for one enclosure design. No Qt Widgets dependency so the
// acoustic math in portphysics.h can be unit-tested against it.
struct BoxModel
{
    // ... entire moved body, unchanged ...
};
```

- [ ] **Step 3: Add the new field for the tuning-shift hook**

Inside the moved struct in `src/boxmodel.h`, directly after the `double portExtraSurfArea_cm2 = 0.0;` line, add:

```cpp
    // Velocity sensitivity of the port end correction (acoustic-mass term).
    // 0 = off (v1 default). Wired into portZ() for later validation.
    double  mapVelCoeff = 0.0;
```

- [ ] **Step 4: Update `enclosurewidget.h`**

Delete the `struct BoxModel { ... };` block from `src/enclosurewidget.h` and add near the top includes:

```cpp
#include "boxmodel.h"
```

- [ ] **Step 5: Register the header in CMake**

In `CMakeLists.txt`, add `src/boxmodel.h` to the `PROJECT_SOURCES` list (next to `src/enclosurewidget.h`).

- [ ] **Step 6: Build**

Run: `cd build && cmake --build . --parallel`
Expected: builds cleanly. `pdfreport.h`/`pdfreport.cpp` still compile because they `#include "enclosurewidget.h"`, which now transitively includes `boxmodel.h`.

- [ ] **Step 7: Commit**

```bash
git add src/boxmodel.h src/enclosurewidget.h CMakeLists.txt
git commit -m "refactor: extract BoxModel into boxmodel.h"
```

---

## Task 2: Extract ported acoustic math into header-only `portphysics.h`

Move the existing ported free functions and globals out of `enclosurewidget.cpp` into an `inline` header. **No logic changes** — copy bodies verbatim. This makes the math linkable from a test target.

**Files:**
- Create: `src/portphysics.h`
- Modify: `src/enclosurewidget.cpp` (remove moved code, add include, route setter)
- Modify: `CMakeLists.txt` (add `src/portphysics.h` to `PROJECT_SOURCES`)

- [ ] **Step 1: Create `src/portphysics.h` with globals + setter**

```cpp
#pragma once
#include "boxmodel.h"
#include "tscalculator.h"
#include <complex>
#include <cmath>

namespace pp {

using Cpx = std::complex<double>;
static constexpr double PI  = TSCalculator::PI;
static constexpr double RHO = 1.2;            // kg/m³ air density

inline double g_C = TSCalculator::C;          // speed of sound [m/s], adjustable
inline void   setSpeedOfSound(double c) { g_C = c; }

} // namespace pp
```

(C++17 `inline` variables give `g_C` a single definition across translation units.)

- [ ] **Step 2: Move the ported helpers verbatim into `namespace pp`**

Cut the following functions from `src/enclosurewidget.cpp` and paste them into `portphysics.h` inside `namespace pp` (before the closing brace), **unchanged**: `driverScaling` (+ its `DriverScaling` struct), `hasPortedData`, the `PortedAmps` struct, `portedAmplitudes`, `portedUa`, `portedSplRaw`, `portedConeSplRaw`, `portedPortSplRaw`, `portedGroupDelay`, `portedImpedance`, `portArea_m2`, `portAirVelocity`, `hasPortVelocityData`. Remove the now-duplicate `using Cpx` / `RHO` / `g_C` lines (49 area) and the `PI` alias from the `.cpp` if they become unused there — leave any still used by sealed/bandpass code.

> If sealed/bandpass code in the `.cpp` still uses `driverScaling`, `hasPortedData`, `RHO`, `g_C`, or `PI`, do **not** delete them from the `.cpp`'s reach — instead reference them via `pp::` (next step). Bandpass functions stay in the `.cpp` for v1.

- [ ] **Step 3: Wire the `.cpp` to the namespace**

At the top of `src/enclosurewidget.cpp` add:

```cpp
#include "portphysics.h"
using namespace pp;   // keep existing unqualified call sites working
```

Replace the global definitions at lines ~48–49 (`static double g_C ...`, `static constexpr double RHO ...`) — they now live in `portphysics.h`. Update the speed-of-sound setter (the `g_C = c;` at ~line 3580) to:

```cpp
    pp::setSpeedOfSound(c);
```

- [ ] **Step 4: Register the header in CMake**

Add `src/portphysics.h` to `PROJECT_SOURCES` in `CMakeLists.txt`.

- [ ] **Step 5: Build and run the app**

Run: `cd build && cmake --build . --parallel && ./TSBoss`
Expected: builds; the enclosure modeller behaves identically (load a vented model, confirm SPL/impedance/port-velocity plots look unchanged).

- [ ] **Step 6: Commit**

```bash
git add src/portphysics.h src/enclosurewidget.cpp CMakeLists.txt
git commit -m "refactor: move ported acoustic math into header-only portphysics.h"
```

---

## Task 3: Add the test target and capture the legacy golden values

Establish the regression anchor before changing any math.

**Files:**
- Create: `tests/portphysics_tests.cpp`
- Modify: `CMakeLists.txt` (test target + `enable_testing`)

- [ ] **Step 1: Add the test scaffolding (no framework)**

Create `tests/portphysics_tests.cpp`:

```cpp
#include "portphysics.h"
#include <cstdio>
#include <cmath>
#include <vector>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// A known, fully-populated vented model used by every test.
static BoxModel makeModel() {
    BoxModel m;
    m.encType = BoxModel::EncType::Vented;
    m.fs = 30.0; m.Qms = 4.0; m.Qes = 0.4; m.Qts = 0.36;
    m.Re = 6.0;  m.mms_g = 60.0; m.BL = 12.0; m.Sd_cm2 = 220.0;
    m.volumeL = 40.0; m.fb = 32.0; m.QL = 7.0;
    m.portShape = 0; m.portWidth_mm = 75.0; m.numPorts = 1;
    m.portFlare = 0; m.numDrivers = 1;
    m.wiringMode = BoxModel::WiringMode::Series;
    return m;
}

static const std::vector<double> kFreqs = {20, 25, 32, 40, 60, 100, 1000};

int main() {
    using namespace pp;
    BoxModel m = makeModel();

    // ---- Task 3: print current (legacy) values to capture as golden ----
    std::printf("# legacy capture\n");
    for (double f : kFreqs)
        std::printf("  f=%6.1f  spl=%.10g  Z=%.10g\n",
                    f, portedSplRaw(m, f), portedImpedance(m, f));

    return g_failures == 0 ? 0 : 1;
}
```

> Note: this compiles against the **current** (Task 2) signatures `portedSplRaw(m,f)` / `portedImpedance(m,f)`. Tasks 4–5 change those signatures; the test is updated in lockstep there.

- [ ] **Step 2: Add the test target to CMake**

Append to `CMakeLists.txt`:

```cmake
# --- Tests ---
enable_testing()
add_executable(portphysics_tests tests/portphysics_tests.cpp)
# Qt6::Gui is required because BoxModel has a QColor field (QtGui), pulled in via boxmodel.h.
target_link_libraries(portphysics_tests PRIVATE Qt6::Core Qt6::Gui)
target_include_directories(portphysics_tests PRIVATE src)
add_test(NAME portphysics_tests COMMAND portphysics_tests)
```

- [ ] **Step 3: Build and run to capture values**

Run: `cd build && cmake .. && cmake --build . --target portphysics_tests && ./portphysics_tests`
Expected: prints `# legacy capture` followed by seven `f= ... spl= ... Z= ...` lines. **Record these 14 numbers.**

- [ ] **Step 4: Freeze the captured values as a golden test**

Replace the `// ---- Task 3 ...` capture block in `main()` with hardcoded expectations (paste YOUR captured numbers):

```cpp
    struct Golden { double f, spl, Z; };
    const std::vector<Golden> golden = {
        // PASTE captured values here, e.g.:
        // {20, 0.001234567, 8.91}, {25, ...}, ...
    };
    for (auto &g : golden) {
        const double spl = portedSplRaw(m, g.f);
        const double Z   = portedImpedance(m, g.f);
        check(std::abs(spl - g.spl) <= 1e-6 * std::max(std::abs(g.spl), 1e-12),
              "golden SPL matches legacy");
        check(std::abs(Z - g.Z) <= 1e-6 * std::max(std::abs(g.Z), 1e-12),
              "golden Z matches legacy");
    }
```

- [ ] **Step 5: Run the frozen test**

Run: `cd build && cmake --build . --target portphysics_tests && ctest --output-on-failure`
Expected: all `ok:` lines, `100% tests passed`.

- [ ] **Step 6: Commit**

```bash
git add tests/portphysics_tests.cpp CMakeLists.txt
git commit -m "test: golden anchor for legacy ported acoustic output"
```

---

## Task 4: Add `portZ`, `flareK`, `portCore` (inert — identical output)

Introduce the velocity-aware impedance and a shared circuit core, but keep them inert: `flareK` returns `0.0` (stub) and `mapVelCoeff` defaults `0.0`, so `portZ` equals the legacy constant `Zport` and output is unchanged.

**Files:**
- Modify: `src/portphysics.h`

- [ ] **Step 1: Add `flareK` (stub) and `portZ`**

In `namespace pp`, before `portedAmplitudes`, add:

```cpp
// Turbulent end-loss coefficient. Stub returns 0 (inert) until Task 6.
inline double flareK(int /*portFlare*/) { return 0.0; }

// Velocity-aware complex port impedance. u = port particle velocity [m/s].
//   Zport(u) = (Rp_visc + Rp_turb(u)) + jω·Map(u)
inline Cpx portZ(const BoxModel &m, double f, double u)
{
    const double omega  = 2.0 * PI * f;
    const double omegab = 2.0 * PI * m.fb;
    const double Vb     = m.volumeL * 1e-3;
    const double Cab    = Vb / (RHO * g_C * g_C);
    const double Map0   = 1.0 / (omegab * omegab * Cab);
    const double Rp_visc = omegab * Map0 / m.QL;

    const double Ap      = std::max(portArea_m2(m), 1e-9);
    const double Rp_turb = 0.5 * RHO * flareK(m.portFlare) * std::abs(u) / Ap;
    const double Map     = Map0 * (1.0 - m.mapVelCoeff * std::abs(u));
    return Cpx(Rp_visc + Rp_turb, omega * Map);
}
```

- [ ] **Step 2: Add `portCore` (the shared circuit solve for a given Zport)**

Add after `portZ`:

```cpp
struct PortCoreResult { Cpx cone, port; double Zin; };

// Solve the driver/box/port circuit for a 1 V drive given a fixed port impedance.
inline PortCoreResult portCore(const BoxModel &m, double f, const Cpx &Zport)
{
    const auto   ds   = driverScaling(m);
    const double omega = 2.0 * PI * f;
    const double mms   = ds.mms, Sd = ds.Sd, BL = ds.BL, Re_eff = ds.Re_eff;
    const double Vb    = m.volumeL * 1e-3;
    const double Cms   = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms   = 2.0*PI * m.fs * mms / m.Qms;
    const double Cab   = Vb / (RHO * g_C * g_C);

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Za    = Zcab * Zport / (Zcab + Zport);
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za);
    if (std::abs(denom) < 1e-100) return { Cpx(0.0), Cpx(0.0), m.Re };
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Up  = (Sd * v * Za) / Zport;
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za;
    const double Zin = std::abs(Ze + Cpx(BL*BL) / Zmot);
    return { Sd * v, Up, Zin };
}
```

- [ ] **Step 3: Re-express the existing solve via `portCore` + `portZ(...,0)`**

Replace the **body** of `portedAmplitudes` so it builds through the new core at `u = 0` (still identical, since `flareK=0`, `cᵥ=0`):

```cpp
inline PortedAmps portedAmplitudes(const BoxModel &m, double f)
{
    if (!hasPortedData(m)) return {};
    const PortCoreResult c = portCore(m, f, portZ(m, f, 0.0));
    return { c.cone, c.port };
}
```

Replace the **body** of `portedImpedance` likewise:

```cpp
inline double portedImpedance(const BoxModel &m, double f)
{
    if (!hasPortedData(m)) return m.Re;
    return portCore(m, f, portZ(m, f, 0.0)).Zin;
}
```

- [ ] **Step 4: Build and run the golden test**

Run: `cd build && cmake --build . --target portphysics_tests && ctest --output-on-failure`
Expected: `100% tests passed` — `portZ`/`portCore` reproduce legacy output exactly.

- [ ] **Step 5: Build the app**

Run: `cd build && cmake --build . --parallel`
Expected: builds cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/portphysics.h
git commit -m "refactor: add inert portZ/portCore, route solve through them"
```

---

## Task 5: Implement `portedSolve` fixed-point and thread `power` through

Add the iterative solver and a `power` argument to every `ported*` accessor. With `flareK` still `0`, results stay identical regardless of power — golden test must still pass.

**Files:**
- Modify: `src/portphysics.h`
- Modify: `src/enclosurewidget.cpp` (call sites)
- Modify: `tests/portphysics_tests.cpp` (signatures + convergence test)

- [ ] **Step 1: Add `PortedSolution` and `portedSolve`**

In `portphysics.h`, after `portCore`:

```cpp
struct PortedSolution { Cpx cone, port; double Zin; double u; };

// Fixed-point solve: port loss depends on velocity u, and u depends on the
// drive voltage V = sqrt(power*Zin). Iterate to convergence (max 4 passes).
inline PortedSolution portedSolve(const BoxModel &m, double f, double power)
{
    if (!hasPortedData(m)) return { Cpx(0.0), Cpx(0.0), m.Re, 0.0 };
    const int    N  = std::max(1, m.numPorts);
    const double Ap = std::max(portArea_m2(m), 1e-9);
    double u = 0.0;
    PortCoreResult c = portCore(m, f, portZ(m, f, u));
    for (int i = 0; i < 4; ++i) {
        c = portCore(m, f, portZ(m, f, u));
        const double V = (c.Zin > 0.0 && power > 0.0) ? std::sqrt(power * c.Zin) : 0.0;
        const double u_new = std::abs(c.port) * V / (N * Ap);
        if (std::abs(u_new - u) <= 1e-4 * std::max(u_new, 1.0)) { u = u_new; break; }
        u = u_new;
    }
    return { c.cone, c.port, c.Zin, u };
}
```

- [ ] **Step 2: Make every accessor a `power`-taking wrapper over `portedSolve`**

Replace the bodies/signatures in `portphysics.h`:

```cpp
inline PortedAmps portedAmplitudes(const BoxModel &m, double f, double power)
{ auto s = portedSolve(m, f, power); return { s.cone, s.port }; }

inline Cpx portedUa(const BoxModel &m, double f, double power)
{ auto a = portedAmplitudes(m, f, power); return a.cone - a.port; }

inline double portedSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(portedUa(m, f, power)); }

inline double portedConeSplRaw(const BoxModel &m, double f, double power)
{ auto a = portedAmplitudes(m, f, power); return (2.0*PI*f) * std::abs(a.cone); }

inline double portedPortSplRaw(const BoxModel &m, double f, double power)
{ auto a = portedAmplitudes(m, f, power); return (2.0*PI*f) * std::abs(a.port); }

inline double portedGroupDelay(const BoxModel &m, double f, double power)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = portedUa(m, f + df, power);
    const Cpx ua2 = portedUa(m, f - df, power);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;
}

inline double portedImpedance(const BoxModel &m, double f, double power)
{ if (!hasPortedData(m)) return m.Re; return portedSolve(m, f, power).Zin; }

inline double portAirVelocity(const BoxModel &m, double f, double power)
{ return portedSolve(m, f, power).u; }
```

Delete the old `portAirVelocity` body (the one doing its own `V = sqrt(power*Z)`); the converged `u` already includes the `/(N·Ap)` division.

- [ ] **Step 3: Thread `power` through `enclosurewidget.cpp` call sites**

Apply this mechanical transformation — every call to one of the wrappers gains the local power. In plot `paintEvent`s the per-model power is `modelPower(m)` (often already in a local `mp`); use it. Edit each site:

  - **`portedF3` / f3 search (≈ lines 458–466):** add a `double power` parameter to the enclosing function and pass it to the `portedSplRaw(...)` calls inside; update its caller(s) — find them with `grep -n "portedF3\|f3(" src/enclosurewidget.cpp` — to pass `modelPower(m)`.
  - **ResponsePlot (≈ 605, 609, 721):** `portedSplRaw(m, 1000.0)` → `portedSplRaw(m, 1000.0, mp)`; `portedSplRaw(m, f)` → `portedSplRaw(m, f, mp)`.
  - **`buildPortedPath` calls (≈ 743, 751, 759):** this helper takes a `double(const BoxModel&, double)` function. Wrap each with a lambda that binds `mp`, e.g.:
    ```cpp
    buildPortedPath([mp](const BoxModel &mm, double ff){ return portedSplRaw(mm, ff, mp); });
    buildPortedPath([mp](const BoxModel &mm, double ff){ return portedConeSplRaw(mm, ff, mp); });
    buildPortedPath([mp](const BoxModel &mm, double ff){ return portedPortSplRaw(mm, ff, mp); });
    ```
    If `buildPortedPath`'s parameter type is a raw function pointer, change it to `const std::function<double(const BoxModel&,double)>&` (add `#include <functional>`).
  - **Cursor readout (≈ 977–994):** add `, mp` to the `portedSplRaw`/`portedConeSplRaw`/`portedPortSplRaw` calls (`mp` is defined at ≈973).
  - **Group-delay plot (≈ 1055, 1143, 1216):** `portedGroupDelay(m, f)` → `portedGroupDelay(m, f, mp)` (define `mp = modelPower(m)` in that scope if absent).
  - **`boxImpedance` / `systemImpedance` (≈ 1267, 1294):** add a `double power` parameter to both; pass it to their internal `portedImpedance(m, f)` → `portedImpedance(m, f, power)`. Then update every caller (VoltagePlot/ExcursionPlot, ≈ 1332, 1355, 1385, 1400, 1497, 1551 and the excursion sites) to pass `mp`. Find them: `grep -n "boxImpedance\|systemImpedance" src/enclosurewidget.cpp`.
  - **Direct `portedAmplitudes`/`portedImpedance` (≈ 1330, 1332):** add `, mp`.
  - **PortVelocityPlot (≈ 1859, 1946, 1988):** these already pass `mp`/`modelPower(m)` — no change.

After editing, confirm none remain: `grep -nE "ported(SplRaw|ConeSplRaw|PortSplRaw|GroupDelay|Impedance|Amplitudes|Ua)\(\s*[A-Za-z_]+\s*,\s*[^,)]+\)" src/enclosurewidget.cpp` should return **no** two-argument calls.

- [ ] **Step 4: Update the test signatures + add a convergence test**

In `tests/portphysics_tests.cpp`, change golden calls to pass a tiny power (the linear limit) and add convergence checks:

```cpp
    // Golden now evaluated at the linear limit (power -> 0):
    for (auto &g : golden) {
        const double spl = portedSplRaw(m, g.f, 1e-12);
        const double Z   = portedImpedance(m, g.f, 1e-12);
        check(std::abs(spl - g.spl) <= 1e-6 * std::max(std::abs(g.spl), 1e-12),
              "golden SPL matches legacy at power->0");
        check(std::abs(Z - g.Z) <= 1e-6 * std::max(std::abs(g.Z), 1e-12),
              "golden Z matches legacy at power->0");
    }
    // Convergence: solve returns finite u across the sweep at high power.
    for (double f : kFreqs) {
        const double u = portAirVelocity(m, f, 1000.0);
        check(std::isfinite(u) && u >= 0.0, "portedSolve converges to finite u");
    }
```

- [ ] **Step 5: Build app + run tests**

Run: `cd build && cmake --build . --parallel && ctest --output-on-failure`
Expected: app builds; `100% tests passed` (output still identical because `flareK=0`).

- [ ] **Step 6: Commit**

```bash
git add src/portphysics.h src/enclosurewidget.cpp tests/portphysics_tests.cpp
git commit -m "feat: power-aware portedSolve fixed-point, threaded through plots"
```

---

## Task 6: Activate turbulent loss (the behaviour change) + compression tests

Flip `flareK` from the stub to real values. This is the entire visible behaviour change.

**Files:**
- Modify: `src/portphysics.h`
- Modify: `tests/portphysics_tests.cpp`

- [ ] **Step 1: Write the failing compression test first**

Add to `main()` in `tests/portphysics_tests.cpp`:

```cpp
    // Compression: at high power the port-region output gains less than the
    // small-signal case (normalised to 1 kHz, where the cone dominates).
    {
        auto gainAtFb = [&](double power) {
            const double ref = portedSplRaw(m, 1000.0, power);
            return 20.0 * std::log10(portedSplRaw(m, m.fb, power) / ref);
        };
        const double lowP  = gainAtFb(1e-3);
        const double highP = gainAtFb(2000.0);
        check(highP <= lowP + 1e-6, "port-region gain compresses at high power");
        check(lowP - highP > 0.1,   "compression is non-trivial (>0.1 dB)");
    }
    // Saddle fill: the impedance minimum between the two ported peaks rises with power.
    {
        auto saddleZ = [&](double power) {
            double zmin = 1e9;
            for (double f = m.fb*0.7; f <= m.fb*1.4; f += 0.5)
                zmin = std::min(zmin, portedImpedance(m, f, power));
            return zmin;
        };
        check(saddleZ(2000.0) >= saddleZ(1e-3) - 1e-6, "impedance saddle fills with power");
    }
    // Flare ranking: straight ports lose more (lower gain at fb) than flared.
    {
        BoxModel straight = m; straight.portFlare = 0;
        BoxModel flared   = m; flared.portFlare   = 2;
        auto g = [](const BoxModel &mm, double power) {
            return 20.0*std::log10(pp::portedSplRaw(mm, mm.fb, power)
                                 / pp::portedSplRaw(mm, 1000.0, power));
        };
        check(g(flared, 2000.0) >= g(straight, 2000.0) - 1e-6,
              "flared port compresses less than straight");
    }
```

- [ ] **Step 2: Run to confirm it fails**

Run: `cd build && cmake --build . --target portphysics_tests && ./portphysics_tests`
Expected: FAIL on "compression is non-trivial" (with `flareK=0` there is no compression).

- [ ] **Step 3: Activate `flareK`**

In `src/portphysics.h` replace the stub:

```cpp
// Turbulent end-loss coefficient from the existing portFlare field:
//   0 = straight (both ends sharp), 1 = one end flared, 2 = both ends flared.
inline double flareK(int portFlare)
{
    switch (portFlare) {
        case 2:  return 0.2;   // both ends flared
        case 1:  return 0.6;   // one end flared
        default: return 1.0;   // straight / sharp
    }
}
```

- [ ] **Step 4: Run all tests**

Run: `cd build && cmake --build . --target portphysics_tests && ctest --output-on-failure`
Expected: `100% tests passed` — golden still matches at `power→0`, and compression/saddle/flare checks now pass.

- [ ] **Step 5: Build the app**

Run: `cd build && cmake --build . --parallel`
Expected: builds cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/portphysics.h tests/portphysics_tests.cpp
git commit -m "feat: velocity-dependent port friction (compression) via flare-aware loss"
```

---

## Task 7: Manual verification and spec close-out

**Files:**
- Modify: `docs/superpowers/specs/2026-05-23-port-friction-compression-design.md` (resolve open questions)

- [ ] **Step 1: Manual smoke test in the app**

Run: `cd build && ./TSBoss`
Verify on a vented model:
- Raise the power control on the SPL tab → the response near `fb` flattens/compresses as power climbs; at low power it matches the previous shape.
- Impedance curve: the dip between the two peaks rises with power.
- Port V tab: unchanged behaviour (already power-aware), still gated to vented.
- Set Flare to "Both ends flared" → less compression than "Straight" at the same high power.

- [ ] **Step 2: Resolve the spec's open questions**

In the design spec, replace the "Open questions for implementation" section with the resolved decisions: tolerance `1e-4` relative with a 4-iteration cap and cold `u=0` start per frequency; no extra on-screen read-out in v1 (bent curves carry the story).

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/specs/2026-05-23-port-friction-compression-design.md
git commit -m "docs: resolve port-friction spec open questions"
```

---

## Self-Review (completed)

- **Spec coverage:** velocity-dependent `Zport` with friction + tuning-shift hook (Task 4), fixed-point solve (Task 5), power threading (Task 5), flare→K via existing `portFlare` (Task 6), `K=0`/`power→0` regression anchor (Tasks 3–6), compression + saddle + convergence tests (Tasks 5–6), no new UI/serialized field for v1 (`mapVelCoeff` in-memory default 0). All covered.
- **Spec deviations (intentional, better):** reuse `portFlare` instead of a new `portEndType` enum (no new UI/persistence); the permanent regression anchor is `power→0` rather than a `K=0` config (since `flareK(0)=1.0` post-feature). Both noted inline.
- **Placeholder scan:** the only "fill in" is the captured golden numbers in Task 3 Step 4, which is an explicit capture-and-paste step — by design, not a gap.
- **Type consistency:** `PortCoreResult{cone,port,Zin}`, `PortedSolution{cone,port,Zin,u}`, `portedSolve(m,f,power)`, `portZ(m,f,u)`, `flareK(int)` used consistently across Tasks 4–6.
