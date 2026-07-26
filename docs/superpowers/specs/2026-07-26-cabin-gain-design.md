# In-cabin (vehicle) gain environments — design

2026-07-26

## What

Predict in-car SPL instead of anechoic SPL, using a low-parameter cabin
model fitted from the user's own measurements. Environments are named,
persisted app-wide (they belong to a vehicle, not a project), and applied
via a CABIN selector on the SPL tab.

## Model — validated against a real five-state leak ladder

A car cabin below ~80 Hz behaves as a damped Helmholtz system: the cabin
volume with its leak aperture resonates at f0 with damping Q. Measured on
one vehicle across closed / window cracked / window open / 1 door / 2 doors:
f0 rose 23→28→32→36→54 Hz and Q fell 4.6→1.8, both monotone with leak, and
the 2-parameter fit held every state to 1.1–1.9 dB RMS over 10–80 Hz.

The applied correction is the fitted shape re-anchored to 0 dB at 80 Hz and
switched off above — the modal region is position-dependent and no
low-order model should pretend to capture it. A dotted "cabin fit ≤ 80 Hz"
marker draws the honesty boundary on the plot.

## Capture protocol (power-agnostic)

Two REW sweeps, same session and untouched level: nearfield (2–5 cm off
the cone) and listening position. Transfer = seat − nearfield: amp gain,
unknown power, mic cal, chain and any crossover cancel in the subtraction.
Import && fit in the Manage dialog runs parse → log-resample with 1/6-oct
smoothing → subtract → grid-search fit (cabingain.h, unit-tested with
synthetic-recovery cases).

## Scope notes

- SPL tab only in v1 — excursion/port velocity are physical driver
  quantities the cabin does not change; Max SPL could inherit later.
- f₋₃ / alignment results stay anechoic by design.
- Nearfield contamination by cabin pressure (~15 dB at 10–15 Hz in the
  measured car) makes fitted gain conservative at the very bottom; a
  reference box measured outside the vehicle firms that up.
