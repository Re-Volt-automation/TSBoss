# Isobaric mounting — design

2026-07-24

## What

Support isobaric (compound) driver mounting: two identical drivers
acoustically locked by a coupling chamber, presenting one cone to the box.
Composes with every enclosure type (sealed / vented / IB / BP4 / BP6).

## Physics (ideal rigid coupling)

The pair behaves as one composite driver:

| param | pair | note |
|---|---|---|
| fs, Qms, Qes, Qts | unchanged | ×2 factors cancel |
| Vas | ÷ 2 | two suspensions in parallel |
| mms | × 2 | coupled moving masses |
| BL, Rₑ | × 2 (series/separate) · BL×1, Rₑ÷2 (parallel) | pair coil wiring follows the wiring radio |
| Sd, Xmax, Xlim | unchanged | one cone faces the load |
| Pe | × 2 | two voice coils |
| η₀ | ÷ 2 (−3 dB) | the cost of the half-size box |

Finite coupling-chamber acoustics (cone decoupling above the chamber's
compliance corner) are deliberately out of scope — out of band for the
sub/woofer passbands this models, and every published isobaric alignment
uses the rigid-link assumption.

## How

- `BoxModel::Mounting { Normal, Isobaric }` — a mounting field, **not** a
  new EncType, because isobaric composes with all box types.
- `mounting.h` (header-only, unit-tested): `withMounting(BoxModel)` bakes
  the table above into a copy (idempotent — clears the flag), exactly like
  the existing `withEffectiveMass` added-mass transform.
- Applied in enclosurewidget at the two existing transform sites:
  `recalculate()` (scalars: alpha/η/SPL pick up Vas/2 automatically) and
  `updatePlot()` (all sims see the composite driver; `driverScaling`,
  bandpass physics, thermal limit and the plots need no changes).
  `numDrivers` counts *pairs* when isobaric; Pe×2 baked into the copy keeps
  `thermalPowerLimit` correct.
- UI: "Mounting" combo beside # Drivers; label flips to "# Pairs"; info
  line "Isobaric pair — Vas·½, −3 dB, 2× Pe". Locked T/S fields keep
  showing the *record's* values; the transform lives inside the sim.
- Serialization: additive `mounting` int in .tsbox/.tsproj (old files
  default Normal). Auto-names gain an "iso" tag.

## Acceptance test

The classic equivalence, asserted numerically at small signal: an isobaric
pair in Vb is *identical* to a single driver in 2·Vb except −3 dB output at
equal power and 2× electrical impedance — checked across the full frequency
range for the vented simulation (tests/isobaric_tests.cpp).
