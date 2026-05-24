# Bandpass Port Velocity — Design Spec

**Date:** 2026-05-24
**Status:** Approved (brainstorm complete, ready for plan)
**Branch:** `feat/diagram-toolkit`

## Problem

The **Port Velocity** plot tab is non-functional for bandpass enclosures (BP4 / BP6):

- `pp::hasPortVelocityData()` (`src/portphysics.h`) gates on `encType == Vented`, so bandpass models compute nothing.
- The tab is still enabled for bandpass (`setTabEnabled(4, ported)`), so on a BP4/BP6 it shows the empty
  *"Select a vented enclosure to see port velocity."* state.

The data needed already exists: `bandpassAmplitudes()` returns the rear- and front-chamber port volume
velocities. The gaps are: (a) no velocity computation wired for bandpass, (b) a BP6 has **two** ports
(rear + front) with different areas/flares/limits, and the plot draws exactly one curve per model.

A second decision (made during brainstorming): for bandpass the selected port **flare** should actually
**load the acoustic circuit** — affecting the SPL/velocity curves — to stay consistent with the vented
model, rather than being a display-only chuffing threshold.

## Decisions

1. **Flare loads the circuit (bandpass becomes velocity-aware).** Add a velocity-dependent turbulent loss
   term to each bandpass chamber port, mirroring `pp::portZ` in the vented model. The bandpass model thus
   becomes power-aware and velocity-aware, like `pp::portedSolve`.
2. **BP6 shows both ports.** Two curves per BP6 model — front (solid) and rear (dashed) — each compared to
   its own flare-based chuffing limit line. BP4 shows the single front port only (rear chamber is sealed).
3. **Linear, power-scaled velocity falls out of the solve** — no separate velocity path; velocities come
   straight from the converged solver.
4. **Plots become power-dependent** for bandpass (move with the power slider), exactly like the vented
   plots already do. Bandpass SPL / group-delay curves will shift slightly from their current (linear)
   validated shapes due to the turbulent term. **Accepted.**
5. **`bandpassMetrics` (f3Low/f3High/ripple) stays power-independent.** Evaluated at `power = 0`, where
   `V = 0 → target velocity = 0 → u = 0`, the turbulent term vanishes exactly and the solve collapses to
   today's linear model.
   Reported design specs therefore remain **identical to today's validated numbers** and are decoupled from
   the power slider.
6. **Do not touch the power-input mechanism.** `modelPower(m) = m_power × (perDriverMode ? numDrivers : 1)`
   and the total-vs-per-driver toggle are unchanged. Plots pass `modelPower(m)` as they already do; metrics
   simply don't consume it.

## Out of Scope

- No change to the vented (`pp::portedSolve`) path — it already works.
- No new UI controls, no schema/`BoxModel` field additions (all needed fields exist: `portFront*`,
  `numPortsFront`, `portFrontFlare`).
- No change to the power-input UI or `modelPower`.

## Architecture

### 1. Unify the bandpass circuit into one solver

`bandpassImpedance()` and `bandpassAmplitudes()` (in `src/enclosurewidget.cpp`) currently duplicate the
same circuit setup (`ZaRear + ZaFront → Za_total → denom`). Replace both with a single power-aware solver:

```cpp
struct BPSolution {
    Cpx    cone, rearPort, frontPort;   // 1 V-normalized acoustic volume velocities
    double Zin;                         // input electrical impedance magnitude [Ω]
    double uRear, uFront;               // peak port air velocities [m/s] at `power`
};
BPSolution bandpassSolve(const BoxModel &m, double f, double power);
```

The existing helpers become thin wrappers over `bandpassSolve`:

- `bandpassImpedance(m, f, power)` → `bandpassSolve(...).Zin`
- `bandpassUa(m, f, power)` → `isBP6 ? frontPort - rearPort : frontPort`
- `bandpassConeSplRaw` / `bandpassRearPortSplRaw` / `bandpassFrontPortSplRaw` → `ω·|·|` of the field
- `bandpassGroupDelay(m, f, power)` → numerical phase derivative of `bandpassUa`

All gain a `power` parameter.

### 2. Velocity-dependent chamber port impedance

Generalize `chamberAcousticZ()` and `chamberPortFlow()` to take the port's current velocity `u`, area `Ap`,
and flare, adding the turbulent term to the existing viscous `Rp` (mirrors `pp::portZ`):

```
Rp_turb = 0.5 · RHO · flareK(flare) · |u| / Ap
Rp_total = Rp_visc + Rp_turb        // Rp_visc = ωb·Map/QL  (unchanged)
```

Reuse `pp::flareK()` and `pp::chuffLimit()` (already in portphysics.h) rather than redefining them.

Rear port uses `portFlare` / `portArea_m2(m)` / `numPorts`.
Front port uses `portFrontFlare` / **new** `portFrontArea_m2(m)` / `numPortsFront`.

### 3. New `portFrontArea_m2()` helper

Add to `src/portphysics.h` next to `portArea_m2()`, reading the `portFront*` fields:

```cpp
inline double portFrontArea_m2(const BoxModel &m) {
    if (m.portFrontShape == 0) {
        const double Dp = m.portFrontWidth_mm / 1000.0;
        return PI * (Dp / 2.0) * (Dp / 2.0);
    }
    return (m.portFrontWidth_mm / 1000.0) * (m.portFrontHeight_mm / 1000.0);
}
```

### 4. Coupled fixed-point solve

`bandpassSolve` runs a fixed-point on the two port velocities `(uRear, uFront)`, structured like
`pp::portedSolve`:

- Cold start `uRear = uFront = 0`.
- Up to 24 iterations, under-relaxation `α = 0.5`.
- Each iteration:
  1. Build `ZaRear`, `ZaFront` from current `(uRear, uFront)` (turbulent term included).
  2. Solve the driver circuit → cone `Sdv`, port flows, `Zin`.
  3. `V = √(power · Zin)` (0 if power or Zin ≤ 0).
  4. Per port: `target = |portFlow| · V / (N · Ap)`; relaxed update `u += α·(target − u)`.
  5. Converged when both updates ≤ `1e-4 · max(u, 1.0)`.
- BP4: rear chamber sealed (`fb = 0`) → no rear port, `uRear ≡ 0`, only the front port is active.
- Recompute once with the converged velocities so all returned fields are self-consistent.

At `power → 0` the turbulent term is 0 and the result equals today's linear model (basis for the
power-independent metrics in Decision 5).

### 5. PortVelocityPlot generalization

Restructure `PortVelocityPlot::paintEvent` (`src/enclosurewidget.cpp`) from "one curve per model" to a
**list of port curves**. Avoid `std::function` in the hot draw loop (per the earlier perf note) — use an
enum-tagged dispatch:

```cpp
enum class PVKind { Vented, BPFront, BPRear };
struct PVCurve { int modelIdx; PVKind kind; QString labelSuffix; Qt::PenStyle style; double chuffMs; };
```

- `pvVelocity(const BoxModel &m, double f, double power, PVKind)` — plain dispatch returning m/s
  (Vented → `pp::portAirVelocity`; BPFront/BPRear → `bandpassSolve(...).uFront/uRear`).
- Build the curve list from `m_models`: Vented → one `Vented` curve (solid); BP4 → one `BPFront` curve
  (solid); BP6 → `BPFront` (solid) + `BPRear` (dashed). Label suffix `""` / `" (front)"` / `" (rear)"`.
- Y-scan, draw, legend, and cursor overlay all iterate this list.
- Pen style: front/vented = solid, rear = dashed. Colour still per model.

### 6. Chuffing limit lines

For the **active model** only:

- Collect the distinct `chuffLimit(flare)` values across its active ports.
- Draw one labelled dashed reference line per distinct value (a BP6 with different front/rear flares → two
  lines; equal flares → one). Vented / BP4 → single line (unchanged behaviour).

### 7. Gating + empty state

- Replace `pp::hasPortVelocityData(m)` (vented-only) with logic true for vented **and** bandpass with valid
  data. Suggested: keep `pp::hasPortVelocityData` for vented, add `hasBandpassPortVelocityData(m)` (true
  when `hasBP4Data`/`hasBP6Data` and the relevant port area > 0), and use a combined predicate in the plot.
- Update empty-state text: *"Select a ported or bandpass enclosure to see port velocity."*

### 8. Call-site threading

Thread `power` through every bandpass acoustic consumer:

- `ResponsePlot`, `GroupDelayPlot`, `VoltagePlot`, `ExcursionPlot` — pass `modelPower(m)` into the bandpass
  branches (they already compute `mp = modelPower(m)` for the vented path).
- `bandpassMetrics(m)` — evaluate `bandpassSolve` at `power = 0` so f3/ripple stay equal to today's values
  and slider-independent.

## Data Flow

```
power slider / per-driver toggle ──► modelPower(m) ──► plots
                                                         │
              ┌──────────────────────────────────────────┤
              ▼                                           ▼
     bandpassSolve(m, f, modelPower)            bandpassSolve(m, f, 0)   ← metrics (linear)
       │  fixed-point (uRear,uFront)
       ├─► cone/rear/front, Zin  ─► SPL / group delay / impedance plots (power-dependent)
       └─► uRear, uFront         ─► PortVelocityPlot curves + cursor
                                     compared to per-port chuffLimit(flare) lines
```

## Testing

- **Unit (portphysics / bandpass math):**
  - `portFrontArea_m2` round vs rect matches hand calc.
  - `bandpassSolve` at `power → 0` reproduces the current linear `bandpassAmplitudes`/`bandpassImpedance`
    outputs (regression guard for Decision 5).
  - Fixed-point converges (no NaN/Inf) across a frequency sweep for representative BP4 and BP6 models at
    high power / small port area.
  - Higher flare → lower turbulent loss → higher velocity / different SPL, monotonic as expected.
  - BP4: `uRear == 0`; BP6: both `uRear` and `uFront` finite and ≥ 0.
- **Regression:** existing `portphysics` tests stay green; vented velocity path unchanged.
- **Manual UAT:** BP4 shows one curve + limit; BP6 shows front (solid) + rear (dashed) + per-port limits;
  curves respond to the power slider; f3/ripple readouts unchanged by the slider; cursor overlay reports
  both port velocities.

## Files Touched

- `src/portphysics.h` — add `portFrontArea_m2()`; combined velocity-data predicate (or reuse flareK/chuffLimit).
- `src/enclosurewidget.cpp` — `bandpassSolve`; generalize `chamberAcousticZ`/`chamberPortFlow`; rewrite
  `bandpassImpedance`/`bandpassUa`/`…SplRaw`/`bandpassGroupDelay`/`bandpassMetrics` as wrappers; thread
  `power` through plot bandpass branches; generalize `PortVelocityPlot::paintEvent` + cursor + gating.
- `src/enclosurewidget.h` — `PortVelocityPlot` signature/state if needed (curve helpers).
- Tests under the existing portphysics test target.
