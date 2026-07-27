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
formula at every frequency:

    correction(f) = (gain + shape(f)) / (1 + (f/100)^4)

No fit-edge cutoff, no taper segment (both earlier constructions kinked
the curve and, with a wrong gain, carved artificial notches). The fade
is negligible through the pressure zone, lets the shape's natural
above-resonance dip stand in for the broad 70–200 Hz valley every
measured state shows at the listening seat (~−11 dB closed), and retires
the correction where the field turns modal. Against the five-state
ladder this beats the log-taper everywhere: RMS 3.0–4.6 dB over
10–200 Hz (taper: 3.7–5.4), 1.4–2.5 dB over 15–100 Hz, and it matches
the measured 10 Hz transfer within 0.3 dB. A pure 3-parameter resonant
low-shelf was also tried and rejected: its dip depth is coupled to its
gain, which drove fits to distorted parameters (closed-car gain 22.5 dB
vs the measured 11.5) and 6–8 dB RMS once pinned to the true zero.

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
