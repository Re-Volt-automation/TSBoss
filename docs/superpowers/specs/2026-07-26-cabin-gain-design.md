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

The environment carries a third measured parameter: **gain**, the fitted
pressure-zone plateau level relative to the 150–400 Hz midband modal
average of the same transfer. The applied correction is one smooth
formula at every frequency, and it is **additive only** — the cabin
boosts the pressure zone and does nothing above; it never subtracts:

    correction(f) = smooth-max(0, gain + shape(f))
                  = (s + sqrt(s² + w²)) / 2,  s = gain + shape, w = 2 dB

Design history: a hard 0 dB anchor at 80 Hz (over-boosted — 80 Hz sits
in a positional dip at the measured seat), a log-taper to 160 Hz
(kinked; with a wrong gain it carved a V-notch), a pure resonant
low-shelf (dip depth couples to gain: distorted fits, 6–8 dB RMS), and
a structural fade letting the shape's negative lobe model the seat's
70–200 Hz valley (best transfer RMS, but subtracts). The valley is
real at that seat in every leak state (~−11 dB) yet positional — it
moves with seat/enclosure placement, so a reusable model must not
carry it (owner decision 2026-07-28). Decisive evidence from a
no-crossover nearfield ladder: above ~45 Hz the closed/window/door
curves are identical (cabin state does not reach the nearfield there),
and each state's nearfield shows an absorption notch at its fitted f0
(22 / 31 / 36 Hz vs fitted 23.2 / 31.8 / 35.5) — the cabin acts as a
Helmholtz absorber at the cone while boosting the seat, independently
confirming the fitted resonances.

Measured gains on the ladder: closed +11.5 dB, window cracked +6.2,
window open +3.5, 1 door +4.7, 2 doors +5.0 (closed car at 20 Hz:
gain + shape = +21.4 dB over anechoic).

The crossover left in the chain during the capture session is harmless:
it cancels in the seat − nearfield division, and the 150–400 Hz baseline
was verified signal-dominated (seat midband *fell* 7 dB when the doors
opened — noise floor would have risen — and nearfield midband held at
82–83 dB across all five states).

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
