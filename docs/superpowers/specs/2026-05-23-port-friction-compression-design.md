# Port Friction & Compression — Design Spec

**Date:** 2026-05-23
**Status:** Approved (design), pending implementation plan
**Area:** Enclosure modeller — ported (vented) box simulation

## Problem

The ported-box simulation is **linear and power-independent**. `portedAmplitudes()`
solves the driver/box/port circuit for a 1 V input, and the selected power is applied
afterward as a flat dB offset on the plots. Consequently every curve (SPL, impedance,
group delay, excursion, port velocity) has the **same shape at 1 W and 1 kW**.

Real ports lose energy to air friction, and that loss grows with air velocity. At high
drive this produces *port compression*: the impedance saddle fills in, the SPL response
flattens, and the box behaves measurably differently than the small-signal prediction.
TSBoss currently cannot show any of this. Port loss is a single user-guessed constant
`QL`, independent of velocity, power, or port geometry.

## Goal

Make port loss **velocity-dependent**, so the simulation bends realistically with drive
level. Build the port impedance as a single velocity-aware complex quantity with room for
both the dominant friction (damping) effect and a secondary tuning-shift effect, even
though v1 only drives the friction term hard.

### Non-goals (v1)
- A static end-correction geometry refinement (flare/aspect/wall-proximity on `Δl`).
  Considered and deferred — orthogonal to the friction work.
- Exposing the acoustic-mass velocity coefficient `cᵥ` in the UI. It is wired in but
  defaults to 0.
- Bandpass (BP4/BP6) port compression. The bandpass path keeps its current linear loss
  model in v1; the shared helper is written so it can adopt the same model later.

## Physics

Port impedance becomes velocity-dependent and complex:

```
Zport(u) = [ Rp_visc + Rp_turb(u) ]  +  jω · Map(u)
              └──── friction / damping ────┘     └─ tuning-shift hook ─┘
```

where `u = |Up| / Ap` is the port-air *particle* velocity (volume velocity / port area).

- **Viscous floor** `Rp_visc = ωb · Map₀ / QL` — identical to today's `Rp`. `QL` keeps its
  current meaning as the small-signal leakage Q.
- **Turbulent minor loss** `Rp_turb(u) = ½ · ρ · K · |u| / Ap`.
  Derivation: a minor-loss pressure drop `Δp = ½·ρ·K·u²`, with volume velocity `U = u·Ap`,
  gives acoustic resistance `R = Δp / U = ½·ρ·K·|u| / Ap` — **linear in velocity**.
  `K` is the flare-dependent end-loss coefficient.
- **Tuning-shift hook** `Map(u) = Map₀ · (1 − cᵥ·|u|)`. Velocity reduces the effective end
  correction, lowering acoustic mass and nudging tuning upward. `cᵥ` defaults to **0**, so
  v1 introduces **no** frequency shift until validated against a real box.

### Why friction barely moves the tuning frequency
For a damped resonator `ω_d = ω₀·√(1 − 1/4Q²)`. Port Q is typically 5–20, so resistance
alone shifts the *center frequency* by a fraction of a percent. Friction's large, visible
effect is on **damping/height** (saddle fill, ripple flattening). The genuine tuning-shift
mechanism is the *acoustic mass* term `Map(u)`, which is why the hook lives there and not
in the resistance.

### Flare → K
| Port end type | K    |
|---------------|------|
| Sharp         | 1.0  |
| Radiused      | 0.5  |
| Flared        | 0.2  |

Flared ports lose less to turbulence and chuff at higher velocity — a real, design-relevant
effect this captures.

## Architecture

### New: single port-impedance helper
```cpp
// Velocity-aware complex port impedance. u = port particle velocity [m/s].
static Cpx portZ(const BoxModel &m, double f, double u);
```
Consolidates the loss math currently duplicated between `portedAmplitudes()` and
`portedImpedance()`.

### New: fixed-point solver
```cpp
struct PortedSolution { Cpx cone, port; double u, Z; };
static PortedSolution portedSolve(const BoxModel &m, double f, double power);
```
Because `Zport` depends on the solution `u`, and the drive voltage depends on the input
impedance `Z` (`V = √(P·Z)`), each frequency iterates the coupled loop:

```
u ← 0                       (or previous frequency's converged u as warm start)
repeat up to 4 times:
    Zport = portZ(m, f, u)
    solve circuit  → cone, Up (per 1 V), Zin
    V   = sqrt(power · Zin)
    u_new = |Up| · V / (N · Ap)
    if |u_new − u| / max(u_new, ε) < tol:  break
    u = u_new
return { cone·V, Up·V, u, Zin }
```

Convergence is fast — the turbulent term is a perturbation on the viscous floor.

### Refactor: existing functions become thin wrappers
`portedAmplitudes`, `portedImpedance`, `portedSplRaw`, `portedConeSplRaw`,
`portedPortSplRaw`, `portedGroupDelay`, and `portAirVelocity` all delegate to
`portedSolve`. `portAirVelocity` reads the converged `u` directly instead of re-deriving
it from a separate `V = √(P·Z)` step.

### Power threading
The SPL raw functions gain a `power` argument (today they assume 1 V). Every plot call
site already computes `mp = modelPower(m)` (enclosurewidget.cpp:601, 716, 931, 973, …), so
the change is localized to passing that value through.

## Data model & UI

> As built: the original plan to add a `portEndType` enum was dropped in favour of reusing
> the existing `portFlare` field, so v1 added **no new UI and no new serialized field**.

- `BoxModel::portFlare` (**existing**) — `0 = straight`, `1 = one end flared`,
  `2 = both ends flared`. Drives `flareK` → `K = 1.0 / 0.6 / 0.2`. Already has a "Flare"
  combo on the Port tab and is already persisted.
- `BoxModel::mapVelCoeff` (`cᵥ`) — new `double`, default `0.0`, internal (no UI, not
  serialized in v1). The tuning-shift hook, wired into `portZ` but inert.

## Error handling & compatibility

- Iteration cap = 4; on non-convergence, fall back to the linear (`u=0`) solution.
- Clamp `u` to a sane finite range; guard `Ap > 0` and `Zin > 0`.
- **Hard invariant:** with `K = 0` and `cᵥ = 0`, every output equals the current
  closed-form result. This is the regression anchor and the definition of "didn't break
  anything."

## Testing

No CTest harness exists yet; the plan adds a small test target that verifies:

1. **Legacy equivalence** — `K=0, cᵥ=0` reproduces captured golden values across a
   frequency sweep (sealed path untouched; ported path within float tolerance).
2. **Monotonic compression** — for a lossy port, peak SPL *gain* (dB above input) is
   non-increasing as power rises.
3. **Convergence** — the fixed point converges within tolerance at every frequency in the
   sweep, for sharp and flared ports across the power range.
4. **Saddle fill** — the impedance dip between the two ported peaks rises with power.

## Resolved decisions (as built)

- **Solver:** fixed-point with a 4-iteration cap, **cold start `u = 0` per frequency** (no
  sweep-carried warm start — simpler and converges fast since the turbulent term is a
  perturbation), tolerance **`1e-4` relative** (`|u_new − u| ≤ 1e-4·max(u_new, 1.0)`).
- **No extra read-out in v1** — the bent curves carry the story; no "compression at power X"
  number was added.
- **Flare input:** reused the existing `BoxModel::portFlare` field (`0 = straight`,
  `1 = one end flared`, `2 = both ends flared`) for the loss coefficient `K`
  (`1.0 / 0.6 / 0.2`) instead of adding a new `portEndType` enum. No new UI, field, or
  serialization was required.
- **Regression anchor:** the permanent invariant is **`power → 0`** (where `u → 0` so
  `Rp_turb → 0`), not a "`K = 0`" config — since `flareK(0) = 1.0`, straight ports are
  lossy after this feature. The golden test asserts equivalence at `power = 1e-12`.
- **`cᵥ` (`mapVelCoeff`):** in-memory `BoxModel` field, default `0.0`, no UI/serialization
  in v1 — the tuning-shift hook is wired into `portZ` but inert.

## Known v1 scope limitations

- **Group delay and f3 are evaluated at the small-signal limit** (`power = 0`), because the
  Group Delay plot and the f3 read-out have no power control in the UI. They therefore do
  **not** reflect compression at the operating power. SPL, impedance, and port-velocity
  plots are fully power-aware. Revisit if a power-aware group-delay view is wanted.
- **Triple solve per SPL frame:** `portedConeSplRaw`/`portedPortSplRaw` each run their own
  `portedSolve`, so drawing the total/cone/port curves runs the solver 3× per frequency
  point. Negligible today; a candidate optimization (single solve → all three outputs) if
  plot repaints feel heavy.
