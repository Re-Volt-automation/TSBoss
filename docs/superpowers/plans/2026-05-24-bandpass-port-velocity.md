# Bandpass Port Velocity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the Port Velocity plot work for bandpass (BP4/BP6) by making the bandpass acoustic model velocity-aware and power-aware, and drawing both ports for BP6.

**Architecture:** Move the bandpass acoustic math out of `enclosurewidget.cpp` into a new testable header `src/bandpassphysics.h` (mirroring `portphysics.h`). Replace the linear `bandpassAmplitudes`/`bandpassImpedance` with one power-aware `bandpassSolve` that runs a coupled fixed-point on the two chamber-port velocities, adding a flare-based turbulent loss term to each chamber port (mirroring `pp::portZ`). Thread `power` through every bandpass consumer; metrics evaluate at `power = 0` (linear limit). Generalize `PortVelocityPlot` from one-curve-per-model to a port-curve list (BP6 = front solid + rear dashed), with per-port chuffing limits.

**Tech Stack:** Qt6 / C++17, header-only acoustics in `src/*.h`, hand-rolled test harness in `tests/portphysics_tests.cpp` (CMake target `portphysics_tests`).

**Spec:** `docs/superpowers/specs/2026-05-24-bandpass-port-velocity-design.md`

---

## File Structure

- **`src/portphysics.h`** — add `portFrontArea_m2()`. Already home to `flareK`, `chuffLimit`, `driverScaling`, `portArea_m2`, `RHO`, `g_C`, `PI`, `Cpx`. *(reused by bandpassphysics.h)*
- **`src/bandpassphysics.h`** *(NEW)* — all bandpass acoustic math in namespace `pp`: `hasBP4Data`, `hasBP6Data`, chamber helpers, `bandpassSolve`, and the thin `bandpassImpedance/Ua/…SplRaw/GroupDelay` wrappers. Includes `portphysics.h`.
- **`src/enclosurewidget.cpp`** — delete the moved statics; `#include "bandpassphysics.h"`; thread `power` through plot call sites; generalize `PortVelocityPlot`.
- **`src/enclosurewidget.h`** — `PortVelocityPlot` gains the curve-list helper types if needed.
- **`tests/portphysics_tests.cpp`** — `#include "bandpassphysics.h"`; bandpass regression + convergence + flare tests.
- **`CMakeLists.txt`** — add `src/bandpassphysics.h` to the app source list (line ~32 area) for IDE/header tracking.

---

## Task 1: `portFrontArea_m2()` helper

**Files:**
- Modify: `src/portphysics.h` (after `portArea_m2`, ~line 28)
- Test: `tests/portphysics_tests.cpp`

- [ ] **Step 1: Write the failing test**

Add inside `main()` in `tests/portphysics_tests.cpp`, just before the final `return` (after the "Bandpass diagram helpers" block, ~line 142):

```cpp
    // portFrontArea_m2: front-port area from the portFront* fields.
    {
        BoxModel fr;  fr.portFrontShape = 0; fr.portFrontWidth_mm = 100.0;   // round Ø100mm
        const double aRound = pp::PI * 0.05 * 0.05;                          // π r², r=0.05 m
        check(std::abs(pp::portFrontArea_m2(fr) - aRound) <= 1e-12, "front round area = πr²");
        BoxModel frr; frr.portFrontShape = 1; frr.portFrontWidth_mm = 80.0; frr.portFrontHeight_mm = 50.0;
        check(std::abs(pp::portFrontArea_m2(frr) - (0.08 * 0.05)) <= 1e-12, "front rect area = w·h");
    }
```

- [ ] **Step 2: Build and run — verify it fails to compile**

```bash
cmake --build /home/wessel/projects/TSBoss/build --target portphysics_tests
```
Expected: compile error — `portFrontArea_m2` is not a member of `pp`.

- [ ] **Step 3: Implement `portFrontArea_m2`**

In `src/portphysics.h`, immediately after the `portArea_m2` function (closing `}` at ~line 28):

```cpp
// Per-port cross-sectional area of the FRONT chamber port [m²] (bandpass).
inline double portFrontArea_m2(const BoxModel &m)
{
    if (m.portFrontShape == 0) {
        const double Dp = m.portFrontWidth_mm / 1000.0;
        return PI * (Dp / 2.0) * (Dp / 2.0);
    }
    return (m.portFrontWidth_mm / 1000.0) * (m.portFrontHeight_mm / 1000.0);
}
```

- [ ] **Step 4: Build and run — verify pass**

```bash
cmake --build /home/wessel/projects/TSBoss/build --target portphysics_tests && \
  /home/wessel/projects/TSBoss/build/portphysics_tests
```
Expected: all lines `ok:`, exit 0. New lines `ok: front round area = πr²`, `ok: front rect area = w·h`.

- [ ] **Step 5: Commit**

```bash
git add src/portphysics.h tests/portphysics_tests.cpp
git commit -m "feat(portphysics): add portFrontArea_m2 for bandpass front port"
```

---

## Task 2: Move bandpass acoustics into `src/bandpassphysics.h` (pure refactor)

No behaviour change. Move the existing **linear** functions verbatim into a testable header, in namespace `pp`, replacing the file-scope `static` versions in `enclosurewidget.cpp`.

**Files:**
- Create: `src/bandpassphysics.h`
- Modify: `src/enclosurewidget.cpp` (delete moved statics ~lines 134-271; add include)
- Modify: `CMakeLists.txt` (~line 32, add header to source list)
- Modify: `tests/portphysics_tests.cpp` (add include)

- [ ] **Step 1: Create `src/bandpassphysics.h`**

```cpp
#pragma once
#include "portphysics.h"
#include <complex>
#include <cmath>

namespace pp {

// Bandpass enclosure type test (avoids a dependency on enclosurewidget's isBP6).
inline bool bpIsBP6(const BoxModel &m) { return m.encType == BoxModel::EncType::Bandpass6; }

inline bool hasBP4Data(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0
        && m.volumeL > 0                         // rear sealed
        && m.volumeFront_L > 0 && m.fbFront > 0  // front vented
        && m.QLFront > 0;
}

inline bool hasBP6Data(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0
        && m.volumeL > 0 && m.fb > 0 && m.QL > 0  // rear vented
        && m.volumeFront_L > 0 && m.fbFront > 0 && m.QLFront > 0;
}

struct BPAmps { Cpx cone, rearPort, frontPort; };

// Per-chamber acoustic impedance: compliance ∥ (port mass+leakage). Sealed → fb=0 → just compliance.
inline Cpx chamberAcousticZ(double Vb_L, double fb, double QL_, double omega)
{
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    if (fb <= 0.0) return Zcab;
    const double omegab = 2.0 * PI * fb;
    const double Map    = 1.0 / (omegab*omegab*Cab);
    const double Rp     = omegab * Map / std::max(QL_, 0.1);
    const Cpx Zport(Rp, omega*Map);
    return Zcab * Zport / (Zcab + Zport);
}

inline Cpx chamberPortFlow(const Cpx &Sdv, double Vb_L, double fb, double QL_, double omega)
{
    if (fb <= 0.0 || Vb_L <= 0.0) return Cpx(0.0, 0.0);
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const double omegab = 2.0 * PI * fb;
    const double Map    = 1.0 / (omegab*omegab*Cab);
    const double Rp     = omegab * Map / std::max(QL_, 0.1);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Zport(Rp, omega*Map);
    const Cpx Za = Zcab * Zport / (Zcab + Zport);
    return (Sdv * Za) / Zport;
}

inline BPAmps bandpassAmplitudes(const BoxModel &m, double f, bool bp6)
{
    if ((bp6 && !hasBP6Data(m)) || (!bp6 && !hasBP4Data(m))) return {};
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms    = ds.mms;
    const double Sd     = ds.Sd;
    const double BL     = ds.BL;
    const double Re_eff = ds.Re_eff;
    const double Cms    = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms    = 2.0*PI * m.fs * mms / m.Qms;

    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,    omega);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront, omega);
    const Cpx Za_total = ZaRear + ZaFront;

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za_total);
    if (std::abs(denom) < 1e-100) return {};
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Sdv = Sd * v;

    BPAmps a;
    a.cone      = Sdv;
    a.rearPort  = chamberPortFlow(Sdv, m.volumeL,       rearFb,   rearQL,   omega);
    a.frontPort = chamberPortFlow(Sdv, m.volumeFront_L, m.fbFront, m.QLFront, omega);
    return a;
}

inline Cpx bandpassUa(const BoxModel &m, double f)
{
    auto a = bandpassAmplitudes(m, f, bpIsBP6(m));
    return bpIsBP6(m) ? (a.frontPort - a.rearPort) : a.frontPort;
}

inline double bandpassSplRaw(const BoxModel &m, double f)
{ return (2.0*PI*f) * std::abs(bandpassUa(m, f)); }

inline double bandpassConeSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, bpIsBP6(m)); return (2.0*PI*f) * std::abs(a.cone); }

inline double bandpassRearPortSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, bpIsBP6(m)); return (2.0*PI*f) * std::abs(a.rearPort); }

inline double bandpassFrontPortSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, bpIsBP6(m)); return (2.0*PI*f) * std::abs(a.frontPort); }

inline double bandpassGroupDelay(const BoxModel &m, double f)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = bandpassUa(m, f + df);
    const Cpx ua2 = bandpassUa(m, f - df);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;
}

inline double bandpassImpedance(const BoxModel &m, double f)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return m.Re;
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms    = ds.mms;
    const double Sd     = ds.Sd;
    const double BL     = ds.BL;
    const double Re_eff = ds.Re_eff;
    const double Cms    = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms    = 2.0*PI * m.fs * mms / m.Qms;
    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,    omega);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront, omega);
    const Cpx Za_total = ZaRear + ZaFront;
    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za_total;
    return std::abs(Ze + Cpx(BL*BL) / Zmot);
}

} // namespace pp
```

- [ ] **Step 2: Delete the moved statics from `enclosurewidget.cpp`**

Delete these now-duplicated definitions:
- `hasBP4Data` (~134-141) and `hasBP6Data` (~143-149)
- `struct BPAmps` (~151)
- `chamberAcousticZ` (~155-166), `chamberPortFlow` (~169-181)
- `bandpassAmplitudes` (~183-214)
- `bandpassUa` (~221-225), `bandpassSplRaw`/`bandpassConeSplRaw`/`bandpassRearPortSplRaw`/`bandpassFrontPortSplRaw` (~227-237)
- `bandpassGroupDelay` (~239-248), `bandpassImpedance` (~250-271)

Keep `isBP4`/`isBP6`/`isBandpass` (still used widely). Because `enclosurewidget.cpp` has `using namespace pp;` (line 50), the `pp::` versions resolve unqualified at every existing call site — no call-site edits in this task.

- [ ] **Step 3: Add the include**

In `src/enclosurewidget.cpp`, after line 2 (`#include "portphysics.h"`):

```cpp
#include "bandpassphysics.h"
```

- [ ] **Step 4: Register header in CMake and test**

In `CMakeLists.txt`, in the app source list near `src/portphysics.h` (~line 32), add:

```cmake
    src/bandpassphysics.h
```

In `tests/portphysics_tests.cpp`, after line 1 (`#include "portphysics.h"`):

```cpp
#include "bandpassphysics.h"
```

- [ ] **Step 5: Build app + tests — verify green (no behaviour change)**

```bash
cmake --build /home/wessel/projects/TSBoss/build --parallel 2>&1 | tail -20 && \
  /home/wessel/projects/TSBoss/build/portphysics_tests | tail -5
```
Expected: app links; `portphysics_tests` exits 0 (all `ok:`). No new tests yet — this is a pure move.

- [ ] **Step 6: Commit**

```bash
git add src/bandpassphysics.h src/enclosurewidget.cpp CMakeLists.txt tests/portphysics_tests.cpp
git commit -m "refactor(bandpass): extract acoustics into testable bandpassphysics.h"
```

---

## Task 3: Velocity-aware `bandpassSolve` (power-aware, two-port turbulent loss)

Replace the linear model with a coupled fixed-point. First **freeze the current linear output as a regression guard**, then add the solver. The `power = 0` path must reproduce the frozen values exactly.

**Files:**
- Modify: `src/bandpassphysics.h`
- Test: `tests/portphysics_tests.cpp`

- [ ] **Step 1: Add bandpass test models + capture harness**

In `tests/portphysics_tests.cpp`, add near the top (after `makeModel`, ~line 24):

```cpp
static BoxModel makeBP4() {
    BoxModel m = makeModel();
    m.encType = BoxModel::EncType::Bandpass4;
    m.volumeL = 30.0;                                   // rear sealed
    m.volumeFront_L = 40.0; m.fbFront = 60.0; m.QLFront = 7.0;
    m.portFrontShape = 0; m.portFrontWidth_mm = 100.0; m.numPortsFront = 1; m.portFrontFlare = 0;
    return m;
}
static BoxModel makeBP6() {
    BoxModel m = makeBP4();
    m.encType = BoxModel::EncType::Bandpass6;
    m.fb = 35.0; m.QL = 7.0;                            // rear vented
    m.portShape = 0; m.portWidth_mm = 80.0; m.numPorts = 1; m.portFlare = 0;
    return m;
}
static const std::vector<double> kBPFreqs = {25, 40, 60, 80, 120};
```

Add a temporary capture block inside `main()` (before the final `return`):

```cpp
    // TEMP CAPTURE — print current linear bandpass values, then delete this block.
    for (auto *mk : { +[]{ return makeBP4(); }, +[]{ return makeBP6(); } }) {
        BoxModel bm = mk();
        for (double f : kBPFreqs)
            std::printf("CAPTURE %s f=%.0f spl=%.10g Z=%.10g\n",
                        bm.encType == BoxModel::EncType::Bandpass6 ? "BP6" : "BP4",
                        f, pp::bandpassSplRaw(bm, f), pp::bandpassImpedance(bm, f));
    }
```

- [ ] **Step 2: Build, run, record the printed numbers**

```bash
cmake --build /home/wessel/projects/TSBoss/build --target portphysics_tests && \
  /home/wessel/projects/TSBoss/build/portphysics_tests | grep CAPTURE
```
Copy each printed `spl`/`Z` value. These are the empirical golden values for the next step (the vented golden table at line ~34 was produced the same way).

- [ ] **Step 3: Replace the capture block with frozen regression assertions**

Delete the TEMP CAPTURE block and replace with the table below, pasting the captured numbers into the `spl`/`Z` columns (rows ordered BP4 25/40/60/80/120 then BP6 25/40/60/80/120):

```cpp
    // Bandpass golden: power=0 (linear limit) must equal the pre-rewrite linear model.
    struct BPGolden { const char *kind; double f, spl, Z; };
    const std::vector<BPGolden> bpGolden = {
        {"BP4", 25, /*spl*/0, /*Z*/0}, {"BP4", 40, 0, 0}, {"BP4", 60, 0, 0}, {"BP4", 80, 0, 0}, {"BP4", 120, 0, 0},
        {"BP6", 25, /*spl*/0, /*Z*/0}, {"BP6", 40, 0, 0}, {"BP6", 60, 0, 0}, {"BP6", 80, 0, 0}, {"BP6", 120, 0, 0},
    };
    for (auto &g : bpGolden) {
        BoxModel bm = (std::string(g.kind) == "BP6") ? makeBP6() : makeBP4();
        const double spl = pp::bandpassSplRaw(bm, g.f, 0.0);     // power=0 → linear
        const double Z   = pp::bandpassImpedance(bm, g.f, 0.0);
        char ms[80], mz[80];
        std::snprintf(ms, sizeof ms, "%s golden SPL @ %.0f Hz (power=0==linear)", g.kind, g.f);
        std::snprintf(mz, sizeof mz, "%s golden Z   @ %.0f Hz (power=0==linear)", g.kind, g.f);
        check(std::abs(spl - g.spl) <= 1e-6 * std::max(std::abs(g.spl), 1e-12), ms);
        check(std::abs(Z   - g.Z)   <= 1e-6 * std::max(std::abs(g.Z),   1e-12), mz);
    }
```

Add `#include <string>` at the top of the test file if not present.

- [ ] **Step 4: Add the new-behaviour tests (these will fail until Step 6)**

Append in `main()`:

```cpp
    // Velocity-aware bandpass: power-aware fixed-point convergence + flare ranking.
    {
        BoxModel bp6 = makeBP6();
        for (double f : kBPFreqs) {
            const auto s = pp::bandpassSolve(bp6, f, 500.0);
            check(std::isfinite(s.uFront) && s.uFront >= 0.0, "BP6 uFront finite");
            check(std::isfinite(s.uRear)  && s.uRear  >= 0.0, "BP6 uRear finite");
        }
        // BP4 rear chamber is sealed → no rear port → uRear == 0.
        BoxModel bp4 = makeBP4();
        const auto s4 = pp::bandpassSolve(bp4, bp4.fbFront, 500.0);
        check(s4.uRear == 0.0, "BP4 uRear == 0 (rear sealed)");
        check(std::isfinite(s4.uFront) && s4.uFront > 0.0, "BP4 uFront > 0");
        // power=0 ⇒ velocities are zero (linear limit, no drive).
        const auto s0 = pp::bandpassSolve(bp6, bp6.fbFront, 0.0);
        check(s0.uFront == 0.0 && s0.uRear == 0.0, "BP6 power=0 ⇒ zero velocity");
        // Flare ranking: at high power a flared front port loses less ⇒ ≥ velocity than sharp.
        BoxModel sharp = makeBP6();  sharp.portFrontFlare = 0;
        BoxModel flared = makeBP6(); flared.portFrontFlare = 2;
        const double us = pp::bandpassSolve(sharp,  sharp.fbFront,  2000.0).uFront;
        const double uf = pp::bandpassSolve(flared, flared.fbFront, 2000.0).uFront;
        check(uf >= us - 1e-9, "flared front port velocity >= sharp at high power");
    }
```

- [ ] **Step 5: Build — verify new tests fail**

```bash
cmake --build /home/wessel/projects/TSBoss/build --target portphysics_tests
```
Expected: compile error — `bandpassSolve` undefined, and `bandpassSplRaw`/`bandpassImpedance` don't take a 3rd arg yet.

- [ ] **Step 6: Implement the velocity-aware solver in `bandpassphysics.h`**

Replace the chamber helpers and all wrappers from Task 2's header with the velocity-aware versions below. Keep `bpIsBP6`, `hasBP4Data`, `hasBP6Data`, and `struct BPAmps` as-is.

Replace `chamberAcousticZ` and `chamberPortFlow` with velocity-aware overloads (add turbulent `Rp_turb = 0.5·RHO·flareK·|u|/Ap`):

```cpp
// Velocity-aware per-chamber acoustic impedance. u = port air velocity [m/s];
// Ap = per-port area [m²]; flare selects the turbulent loss coefficient.
inline Cpx chamberAcousticZ(double Vb_L, double fb, double QL_, double omega,
                            double u, double Ap, int flare)
{
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    if (fb <= 0.0) return Zcab;                        // sealed chamber
    const double omegab  = 2.0 * PI * fb;
    const double Map     = 1.0 / (omegab*omegab*Cab);
    const double Rp_visc = omegab * Map / std::max(QL_, 0.1);
    const double Rp_turb = 0.5 * RHO * flareK(flare) * std::abs(u) / std::max(Ap, 1e-9);
    const Cpx Zport(Rp_visc + Rp_turb, omega*Map);
    return Zcab * Zport / (Zcab + Zport);
}

inline Cpx chamberPortFlow(const Cpx &Sdv, double Vb_L, double fb, double QL_, double omega,
                           double u, double Ap, int flare)
{
    if (fb <= 0.0 || Vb_L <= 0.0) return Cpx(0.0, 0.0);
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const double omegab  = 2.0 * PI * fb;
    const double Map     = 1.0 / (omegab*omegab*Cab);
    const double Rp_visc = omegab * Map / std::max(QL_, 0.1);
    const double Rp_turb = 0.5 * RHO * flareK(flare) * std::abs(u) / std::max(Ap, 1e-9);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Zport(Rp_visc + Rp_turb, omega*Map);
    const Cpx Za = Zcab * Zport / (Zcab + Zport);
    return (Sdv * Za) / Zport;
}
```

Replace `bandpassAmplitudes` and `bandpassImpedance` with a single solver plus thin wrappers:

```cpp
struct BPSolution {
    Cpx    cone, rearPort, frontPort;   // 1 V-normalized acoustic volume velocities
    double Zin;                         // |input electrical impedance| [Ω]
    double uRear, uFront;               // peak port air velocities [m/s] at `power`
};

// One core evaluation of the circuit at fixed port velocities (uRear,uFront).
struct BPCore { Cpx cone, rearPort, frontPort; double Zin; };
inline BPCore bandpassCore(const BoxModel &m, double f, bool bp6,
                           double uRear, double uFront)
{
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms = ds.mms, Sd = ds.Sd, BL = ds.BL, Re_eff = ds.Re_eff;
    const double Cms = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms = 2.0*PI * m.fs * mms / m.Qms;

    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const double ApRear  = std::max(portArea_m2(m),      1e-9);
    const double ApFront = std::max(portFrontArea_m2(m), 1e-9);

    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,
                                         omega, uRear,  ApRear,  m.portFlare);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront,
                                         omega, uFront, ApFront, m.portFrontFlare);
    const Cpx Za_total = ZaRear + ZaFront;

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za_total);
    if (std::abs(denom) < 1e-100) return { Cpx(0.0), Cpx(0.0), Cpx(0.0), m.Re };
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Sdv = Sd * v;
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za_total;
    const double Zin = std::abs(Ze + Cpx(BL*BL) / Zmot);

    BPCore c;
    c.cone      = Sdv;
    c.rearPort  = chamberPortFlow(Sdv, m.volumeL,       rearFb,    rearQL,
                                  omega, uRear,  ApRear,  m.portFlare);
    c.frontPort = chamberPortFlow(Sdv, m.volumeFront_L, m.fbFront, m.QLFront,
                                  omega, uFront, ApFront, m.portFrontFlare);
    c.Zin = Zin;
    return c;
}

// Power-aware coupled fixed-point on (uRear,uFront). Mirrors pp::portedSolve:
// cold start u=0, under-relaxation alpha=0.5, 24 iterations, 1e-4 relative tol.
inline BPSolution bandpassSolve(const BoxModel &m, double f, double power)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m))
        return { Cpx(0.0), Cpx(0.0), Cpx(0.0), m.Re, 0.0, 0.0 };

    const double ApRear  = std::max(portArea_m2(m),      1e-9);
    const double ApFront = std::max(portFrontArea_m2(m), 1e-9);
    const int    NRear   = std::max(1, m.numPorts);
    const int    NFront  = std::max(1, m.numPortsFront);
    constexpr double alpha = 0.5;

    double uRear = 0.0, uFront = 0.0;
    BPCore c{};
    for (int i = 0; i < 24; ++i) {
        c = bandpassCore(m, f, bp6, uRear, uFront);
        const double V = (c.Zin > 0.0 && power > 0.0) ? std::sqrt(power * c.Zin) : 0.0;
        const double tgtR = bp6 ? std::abs(c.rearPort) * V / (NRear * ApRear) : 0.0;
        const double tgtF =        std::abs(c.frontPort) * V / (NFront * ApFront);
        const double nR = uRear  + alpha * (tgtR - uRear);
        const double nF = uFront + alpha * (tgtF - uFront);
        const bool done = std::abs(nR - uRear) <= 1e-4 * std::max(nR, 1.0)
                       && std::abs(nF - uFront) <= 1e-4 * std::max(nF, 1.0);
        uRear = nR; uFront = nF;
        if (done) break;
    }
    c = bandpassCore(m, f, bp6, uRear, uFront);   // final self-consistent recompute
    return { c.cone, c.rearPort, c.frontPort, c.Zin, uRear, uFront };
}
```

Now make every wrapper take `power` and delegate to `bandpassSolve`:

```cpp
inline BPAmps bandpassAmplitudes(const BoxModel &m, double f, double power)
{ auto s = bandpassSolve(m, f, power); return { s.cone, s.rearPort, s.frontPort }; }

inline Cpx bandpassUa(const BoxModel &m, double f, double power)
{ auto s = bandpassSolve(m, f, power);
  return bpIsBP6(m) ? (s.frontPort - s.rearPort) : s.frontPort; }

inline double bandpassSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassUa(m, f, power)); }

inline double bandpassConeSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassSolve(m, f, power).cone); }

inline double bandpassRearPortSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassSolve(m, f, power).rearPort); }

inline double bandpassFrontPortSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassSolve(m, f, power).frontPort); }

inline double bandpassGroupDelay(const BoxModel &m, double f, double power)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = bandpassUa(m, f + df, power);
    const Cpx ua2 = bandpassUa(m, f - df, power);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;
}

inline double bandpassImpedance(const BoxModel &m, double f, double power)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return m.Re;
    return bandpassSolve(m, f, power).Zin;
}

// Peak air velocity [m/s] of one bandpass chamber port.
inline double bandpassPortVelocity(const BoxModel &m, double f, double power, bool front)
{ auto s = bandpassSolve(m, f, power); return front ? s.uFront : s.uRear; }

// True when a bandpass model can produce a velocity curve for the given port.
inline bool hasBandpassPortVelocityData(const BoxModel &m, bool front)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return false;
    if (front) return portFrontArea_m2(m) > 0.0;
    return bp6 && portArea_m2(m) > 0.0;             // BP4 rear is sealed → no rear port
}
```

> Note: the old 3-arg `bandpassAmplitudes(m, f, bool bp6)` no longer exists. Task 4 fixes the one remaining `bp6`-style call site.

- [ ] **Step 7: Build and run — verify all tests pass**

```bash
cmake --build /home/wessel/projects/TSBoss/build --target portphysics_tests && \
  /home/wessel/projects/TSBoss/build/portphysics_tests
```
Expected: exit 0; the `power=0==linear` golden rows pass (rewrite preserves the linear limit), plus all convergence/flare/zero-velocity checks.

- [ ] **Step 8: Commit**

```bash
git add src/bandpassphysics.h tests/portphysics_tests.cpp
git commit -m "feat(bandpass): velocity-aware power-aware bandpassSolve (two-port turbulent loss)"
```

---

## Task 4: Thread `power` through `enclosurewidget.cpp` call sites

The app won't compile after Task 3 until every bandpass call passes `power`. `modelPower(m)` is a member of each plot class and is already computed (`mp`) in the bandpass branches.

**Files:**
- Modify: `src/enclosurewidget.cpp`

- [ ] **Step 1: SPL plot (ResponsePlot) — Y-scan + curve drawing**

In the Y-scan bandpass branch (~line 500, 506), `mp` is already in scope (line 480). Change:
- `ref = std::max(ref, bandpassSplRaw(m, f));` → `ref = std::max(ref, bandpassSplRaw(m, f, mp));`
- `const double raw = bandpassSplRaw(m, f);` → `const double raw = bandpassSplRaw(m, f, mp);`

In the draw bandpass branch (~line 644-690), add `const double mp = modelPower(m);` at the top of the branch (after `const bool bp6 = isBP6(m);`), then change the three `buildBPath(...)` calls to pass power-capturing lambdas:
- `buildBPath(bandpassSplRaw)` → `buildBPath([&](const BoxModel &mm, double ff){ return bandpassSplRaw(mm, ff, mp); })`
- `buildBPath(bandpassFrontPortSplRaw)` → `buildBPath([&](const BoxModel &mm, double ff){ return bandpassFrontPortSplRaw(mm, ff, mp); })`
- `buildBPath(bandpassRearPortSplRaw)` → `buildBPath([&](const BoxModel &mm, double ff){ return bandpassRearPortSplRaw(mm, ff, mp); })`

(`buildBPath` takes `auto rawFn` and calls `rawFn(m, f)`, so a 2-arg lambda is compatible.)

- [ ] **Step 2: Group-delay plot call sites (~942, 1032, 1098)**

At each `bandpassGroupDelay(m, f)` / `bandpassGroupDelay(m, cf)`, pass the model's power. `modelPower` is a member of `GroupDelayPlot`; add `const double mp = modelPower(m);` in scope if not already present, then:
- `bandpassGroupDelay(m, f)` → `bandpassGroupDelay(m, f, mp)`
- `bandpassGroupDelay(m, cf)` → `bandpassGroupDelay(m, cf, modelPower(m))`

- [ ] **Step 3: Impedance wrappers (~1147, 1174, 1222)**

`boxImpedance`/`systemImpedance` already receive `power`:
- line 1147: `return bandpassImpedance(m, f);` → `return bandpassImpedance(m, f, power);`
- line 1174: `return bandpassImpedance(m, f);` → `return bandpassImpedance(m, f, power);`
- line 1222: in the impedance plot loop, `const double Z = bandpassImpedance(m, f);` → `const double Z = bandpassImpedance(m, f, modelPower(m));`

- [ ] **Step 4: Impedance-plot amplitude call (~1220)**

`auto a = bandpassAmplitudes(m, f, bp6);` → `auto a = bandpassAmplitudes(m, f, modelPower(m));`
(The new `bandpassAmplitudes` signature is `(m, f, power)`; `bp6` is no longer a parameter. Remove any now-unused local `bp6` if the compiler warns.)

- [ ] **Step 5: `bandpassMetrics` — evaluate at `power = 0` (line ~276-330)**

Replace every `bandpassSplRaw(m, X)` inside `bandpassMetrics` with `bandpassSplRaw(m, X, 0.0)` (occurrences at ~288, 297, 301, 309, 313, 324). This keeps f3Low/f3High/peakDb/ripple identical to today and independent of the power slider.

- [ ] **Step 6: Build the whole app — verify it compiles and links**

```bash
cmake --build /home/wessel/projects/TSBoss/build --parallel 2>&1 | tail -20
```
Expected: clean build, no errors. If the compiler flags an unused `bp6` variable, delete that declaration.

- [ ] **Step 7: Run unit tests (regression)**

```bash
/home/wessel/projects/TSBoss/build/portphysics_tests | tail -3
```
Expected: exit 0.

- [ ] **Step 8: Commit**

```bash
git add src/enclosurewidget.cpp
git commit -m "feat(bandpass): thread applied power through SPL/GD/impedance; metrics at power=0"
```

---

## Task 5: Generalize `PortVelocityPlot` for bandpass (two-port)

Convert the plot from one-curve-per-model to a port-curve list: Vented = 1 solid curve; BP4 = 1 solid (front); BP6 = front (solid) + rear (dashed). Per-port chuffing limit lines for the active model. No `std::function` in the draw loop.

**Files:**
- Modify: `src/enclosurewidget.cpp` (`PortVelocityPlot::paintEvent`, ~1689-1857)

- [ ] **Step 1: Add the port-curve model + dispatch (file scope, near other PortVelocityPlot code, before `paintEvent`)**

```cpp
// One drawable velocity curve. A vented model yields one; a BP6 yields two
// (front + rear). Kept as a plain enum dispatch — no std::function in the draw loop.
enum class PVKind { Vented, BPFront, BPRear };

static bool pvHasData(const BoxModel &m, PVKind k)
{
    switch (k) {
        case PVKind::Vented:  return hasPortVelocityData(m);                  // pp:: (vented-only)
        case PVKind::BPFront: return hasBandpassPortVelocityData(m, true);
        case PVKind::BPRear:  return hasBandpassPortVelocityData(m, false);
    }
    return false;
}

static double pvVelocity(const BoxModel &m, double f, double power, PVKind k)
{
    switch (k) {
        case PVKind::Vented:  return portAirVelocity(m, f, power);            // pp::
        case PVKind::BPFront: return bandpassPortVelocity(m, f, power, true);
        case PVKind::BPRear:  return bandpassPortVelocity(m, f, power, false);
    }
    return 0.0;
}

static int pvFlare(const BoxModel &m, PVKind k)
{ return k == PVKind::BPRear ? m.portFlare
       : k == PVKind::BPFront ? m.portFrontFlare
       : m.portFlare; }

struct PVCurve { int modelIdx; PVKind kind; QString suffix; Qt::PenStyle style; };

// Build the list of velocity curves a model contributes (0, 1, or 2).
static QVector<PVCurve> pvCurvesFor(const BoxModel &m, int idx)
{
    QVector<PVCurve> out;
    if (m.encType == BoxModel::EncType::Vented) {
        if (pvHasData(m, PVKind::Vented)) out.push_back({idx, PVKind::Vented, "", Qt::SolidLine});
    } else if (m.encType == BoxModel::EncType::Bandpass4
            || m.encType == BoxModel::EncType::Bandpass6) {
        if (pvHasData(m, PVKind::BPFront)) out.push_back({idx, PVKind::BPFront, " (front)", Qt::SolidLine});
        if (pvHasData(m, PVKind::BPRear))  out.push_back({idx, PVKind::BPRear,  " (rear)",  Qt::DashLine});
    }
    return out;
}
```

- [ ] **Step 2: Rebuild the curve list at the top of `paintEvent`, replace the Y-scan**

After computing `lfMin`/`lfMax` (~line 1700), build the list and Y-scan over it. Replace the existing Y-scan loop (~1703-1714):

```cpp
    QVector<PVCurve> curves;
    for (int i = 0; i < m_models.size(); ++i)
        curves += pvCurvesFor(m_models[i], i);

    double yMax = 1.0;
    bool anyValid = !curves.isEmpty();
    for (const auto &cv : curves) {
        const BoxModel &m = m_models[cv.modelIdx];
        const double mp = modelPower(m);
        for (int i = 0; i <= 80; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/80.0);
            const double v = pvVelocity(m, f, mp, cv.kind);
            if (std::isfinite(v)) yMax = std::max(yMax, v);
        }
    }
```

- [ ] **Step 3: Replace the chuffing-limit block with per-port limits for the active model (~1757-1776)**

```cpp
    // Per-port chuffing limit line(s) for the active model. A BP6 with different
    // front/rear flares shows two; equal flares (and vented/BP4) collapse to one.
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size()) {
        const BoxModel &am = m_models[m_activeIdx];
        QVector<double> limits;
        for (const auto &cv : pvCurvesFor(am, m_activeIdx)) {
            const double lim = chuffLimit(pvFlare(am, cv.kind));
            if (!limits.contains(lim)) limits.push_back(lim);
        }
        for (double lim : limits) {
            if (lim < yMin || lim > yMax) continue;
            p.setPen(QPen(Theme::instance().statusError(), 1.0, Qt::DashLine));
            p.drawLine(QPointF(area.left(), yPx(lim)), QPointF(area.right(), yPx(lim)));
            QFont rf; rf.setPointSize(7); p.setFont(rf);
            p.setPen(Theme::instance().statusError());
            p.drawText(QRectF(area.left()+4, yPx(lim)-13, 90, 12),
                       Qt::AlignLeft|Qt::AlignVCenter,
                       QString("%1 m/s limit").arg(int(std::round(lim))));
        }
    }
```

(The variables `yMin`, `yPx`, `area` are defined earlier in `paintEvent` and remain unchanged.)

- [ ] **Step 4: Update the empty-state text (~1797)**

`"Select a vented enclosure to see port velocity."` → `"Select a ported or bandpass enclosure to see port velocity."`

- [ ] **Step 5: Replace `drawCurve` and the draw loop (~1802-1824) with curve-list drawing**

```cpp
    auto drawPV = [&](const PVCurve &cv, bool active) {
        const BoxModel &m = m_models[cv.modelIdx];
        const double mp = modelPower(m);
        QPainterPath curve; bool first = true;
        for (int i = 0; i <= 500; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/500.0);
            const double v = pvVelocity(m, f, mp, cv.kind);
            if (!std::isfinite(v)) continue;
            const QPointF pt(xPx(f), yPx(v));
            if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setPen(QPen(c, active ? 3.0 : 1.8, cv.style));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);
    };

    p.setClipRect(area);
    for (const auto &cv : curves)
        if (cv.modelIdx != m_activeIdx) drawPV(cv, false);
    for (const auto &cv : curves)
        if (cv.modelIdx == m_activeIdx) drawPV(cv, true);
    p.setClipping(false);
```

- [ ] **Step 6: Update the legend to iterate curves (~1827-1842)**

```cpp
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-180; int ly = int(area.top())+8;
        for (const auto &cv : curves) {
            const BoxModel &m = m_models[cv.modelIdx];
            bool active = (cv.modelIdx == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5, cv.style));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx+24, ly, 150, 14), Qt::AlignLeft|Qt::AlignVCenter, m.name + cv.suffix);
            ly += 16;
        }
    }
```

- [ ] **Step 7: Update the cursor overlay to iterate curves (~1844-1856)**

```cpp
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        for (const auto &cv : curves) {
            const BoxModel &m = m_models[cv.modelIdx];
            const double v = pvVelocity(m, m_cursorFreq, modelPower(m), cv.kind);
            if (!std::isfinite(v)) continue;
            entries.append({m.color, cv.modelIdx == m_activeIdx, m.name + cv.suffix,
                            yPx(v), QString("%1 m/s").arg(v, 0, 'f', 2)});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
```

- [ ] **Step 8: Build the app — verify it compiles**

```bash
cmake --build /home/wessel/projects/TSBoss/build --parallel 2>&1 | tail -20
```
Expected: clean build.

- [ ] **Step 9: Commit**

```bash
git add src/enclosurewidget.cpp
git commit -m "feat(ports): PortVelocityPlot draws bandpass ports (BP6 front+rear, per-port limits)"
```

---

## Task 6: Verification & manual UAT

**Files:** none (verification only).

- [ ] **Step 1: Full build + unit tests green**

```bash
cmake --build /home/wessel/projects/TSBoss/build --parallel 2>&1 | tail -5 && \
  /home/wessel/projects/TSBoss/build/portphysics_tests | tail -3
```
Expected: build OK; tests exit 0.

- [ ] **Step 2: Launch the app and check the Port Velocity tab**

```bash
/home/wessel/projects/TSBoss/build/TSBoss
```
Manual checks:
- Select/create a **BP4** model → Port Velocity tab shows **one** curve (front) + one limit line; no empty-state text.
- Select/create a **BP6** model → **two** curves: front (solid) + rear (dashed), each in the model colour; per-port limit line(s).
- Drag the **power slider** → bandpass velocity *and* SPL curves move (vented behaviour parity). Toggle total/per-driver → curves scale, no crash.
- The **f3 / ripple readouts** (bandpass metrics labels) do **not** change when the power slider moves.
- Hover the velocity plot → cursor overlay lists each port velocity (`Name (front)`, `Name (rear)`).

- [ ] **Step 3: Update the follow-up memory**

Mark the deferred task done in `/home/wessel/.claude/projects/-home-wessel-projects-TSBoss/memory/project_bandpass_portvelocity_followup.md` (note it shipped on `feat/diagram-toolkit`, velocity-aware approach), and adjust the `MEMORY.md` pointer line.

- [ ] **Step 4: Final commit (if any verification fixes were needed)**

```bash
git add -A && git commit -m "chore(bandpass): port-velocity UAT fixes + close follow-up"
```

---

## Self-Review Notes

- **Spec coverage:** velocity-aware solver (T3) ✓, two-port BP6 display (T5) ✓, per-port chuff limits (T5 S3) ✓, `portFrontArea_m2` (T1) ✓, flare loads circuit (T3 S6) ✓, power threading + plots power-dependent (T4) ✓, metrics at power=0 (T4 S5, T3 golden) ✓, gating + empty-state (T5 S1/S4) ✓, power-input untouched (`modelPower` unchanged throughout) ✓.
- **Captured golden values (T3 S1-S3):** the zero placeholders in `bpGolden` are *empirical measurements* the engineer records from the capture run, exactly as the existing vented golden table was produced — not deferred design.
- **Type consistency:** `bandpassSolve`→`BPSolution{cone,rearPort,frontPort,Zin,uRear,uFront}`; wrappers all `(m,f,power)`; `PVKind`/`PVCurve`/`pvVelocity`/`pvHasData`/`pvFlare`/`pvCurvesFor` names used consistently across T5 steps; `hasBandpassPortVelocityData(m,front)` and `bandpassPortVelocity(m,f,power,front)` signatures match between T3 and T5.
