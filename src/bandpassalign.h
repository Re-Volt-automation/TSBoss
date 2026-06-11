#pragma once
#include "boxmodel.h"
#include "bandpassphysics.h"
#include <cmath>
#include <algorithm>

// Maximally-flat 4th-order bandpass (single-reflex) alignment.
//
// A BP4 box has a sealed REAR chamber and a vented FRONT chamber; the driver
// sits on the wall between them and only the front port radiates. The acoustic
// response is a 4th-order bandpass. Three design DOF: rear volume Vr
// (BoxModel::volumeL), front volume Vf (volumeFront_L) and front tuning fb
// (fbFront).
//
// "Maximally flat" alone leaves a residual bandwidth DOF, so the rear chamber is
// pinned to the canonical Butterworth loading Qbc = 1/sqrt(2): the sealed
// driver+rear sub-system then resonates at Fc = fs*sqrt(alpha_r+1) with Q = Qbc,
// exactly the same closed form the sealed "Butterworth" button uses. The front
// chamber is seeded symmetric (Vf = Vr, fb = Fc) and then refined numerically
// against the validated circuit model (pp::bandpassSplRaw) to flatten the
// passband. Vr stays fixed through the refine so the Qbc anchor is preserved.

namespace pp {

// Linear-limit (power = 0) passband description of a bandpass model.
//   f3Low/f3High : -3 dB corners around the SPL peak [Hz]
//   peakDb       : 20*log10(peak omega*|Ua|)
//   rippleDb     : flatness = (max-min) in dB over the passband EXCLUDING the
//                  outer `edgeFrac` of each side (log-f). Dropping the corners
//                  keeps the inherent -3 dB rolloff from dominating, so a
//                  maximally-flat top -> ~0 dB while peaking/dip -> larger.
//   valid        : a usable passband was found
struct BPResponse { double f3Low, f3High, peakDb, rippleDb; bool valid; };

inline BPResponse bandpassResponse(const BoxModel &m, double edgeFrac = 0.12)
{
    BPResponse out{0.0, 0.0, 0.0, 0.0, false};
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return out;

    constexpr int    N        = 320;
    constexpr double fHiSweep = 600.0;
    const double logHi = std::log10(fHiSweep);
    double peak = 0.0, fPeak = 1.0;
    for (int i = 0; i < N; ++i) {
        const double f = std::pow(10.0, (double(i) / (N - 1)) * logHi);   // 1 .. 600 Hz
        const double s = bandpassSplRaw(m, f, 0.0);
        if (s > peak) { peak = s; fPeak = f; }
    }
    if (peak <= 0.0) return out;
    out.peakDb = 20.0 * std::log10(peak);
    const double target = peak * std::pow(10.0, -3.0 / 20.0);

    // -3 dB corners by bisection in log-f.
    double f3Low = 1.0;
    if (bandpassSplRaw(m, 1.0, 0.0) < target) {
        double lo = 1.0, hi = fPeak;
        for (int i = 0; i < 60; ++i) {
            const double mid = std::sqrt(lo * hi);
            (bandpassSplRaw(m, mid, 0.0) < target) ? lo = mid : hi = mid;
        }
        f3Low = std::sqrt(lo * hi);
    }
    double f3High = fHiSweep;
    if (bandpassSplRaw(m, fHiSweep, 0.0) < target) {
        double lo = fPeak, hi = fHiSweep;
        for (int i = 0; i < 60; ++i) {
            const double mid = std::sqrt(lo * hi);
            (bandpassSplRaw(m, mid, 0.0) >= target) ? lo = mid : hi = mid;
        }
        f3High = std::sqrt(lo * hi);
    }
    if (!(f3Low > 0.0) || !(f3High > f3Low)) return out;
    out.f3Low = f3Low; out.f3High = f3High;

    // Interior flatness: sample [f3Low, f3High] in log-f, drop the outer
    // edgeFrac on each side, report (max-min) in dB of the interior.
    const double lL = std::log10(f3Low), lH = std::log10(f3High);
    const double a = lL + edgeFrac * (lH - lL);
    const double b = lH - edgeFrac * (lH - lL);
    double mn = 1e30, mx = 0.0;
    constexpr int M = 160;
    for (int i = 0; i < M; ++i) {
        const double f = std::pow(10.0, a + (double(i) / (M - 1)) * (b - a));
        const double s = bandpassSplRaw(m, f, 0.0);
        if (s > 0.0) { mn = std::min(mn, s); mx = std::max(mx, s); }
    }
    if (mn > 0.0 && mx > 0.0) out.rippleDb = 20.0 * std::log10(mx / mn);
    out.valid = true;
    return out;
}

// Golden-section minimiser of a unimodal 1-D objective f on [a, b].
template <class F>
inline double goldenMin(F f, double a, double b, int iters = 26)
{
    constexpr double gr = 0.6180339887498949;
    double c = b - gr * (b - a), d = a + gr * (b - a);
    double fc = f(c), fd = f(d);
    for (int i = 0; i < iters; ++i) {
        if (fc < fd) { b = d; d = c; fd = fc; c = b - gr * (b - a); fc = f(c); }
        else         { a = c; c = d; fc = fd; d = a + gr * (b - a); fd = f(d); }
    }
    return 0.5 * (a + b);
}

struct BP4FlatResult { double Vr_L, Vf_L, fbFront; bool ok; };

// Compute a maximally-flat BP4 alignment from the driver T/S and front-port
// geometry already present in `tmpl`. `tmpl` is used as a template: every field
// except the rear/front chamber volumes and front tuning is kept (driver params,
// front-port shape/size, QLFront, driver count, ...), so the suggestion matches
// the port the user has actually configured. Returns ok=false when Qts is out of
// the usable range (needs 0 < Qts < 1/sqrt2).
inline BP4FlatResult bp4MaxFlat(const BoxModel &tmpl)
{
    BP4FlatResult r{0.0, 0.0, 0.0, false};
    const double Qts = tmpl.Qts, Vas = tmpl.Vas_L, fs = tmpl.fs;
    constexpr double Qbc = 0.70710678;            // Butterworth rear loading
    if (!(fs > 0.0) || !(Vas > 0.0) || !(Qts > 0.0) || Qts >= Qbc) return r;

    const double alphaR = (Qbc / Qts) * (Qbc / Qts) - 1.0;   // > 0 since Qts < Qbc
    const double Vr = Vas / alphaR;
    const double Fc = fs * (Qbc / Qts);                      // = fs*sqrt(alphaR+1)

    // Symmetric chambers (Vf = Vr). Equal compliance gives a controlled, roughly
    // one-octave passband — the natural maximally-flat operating point for a
    // single-reflex BP4. Pinning Vf removes the bandwidth degeneracy: minimising
    // ripple over the volumes alone runs away to a very wide, barely-loaded front
    // (which is no longer a bandpass), whereas with Vf fixed the only freedom is
    // the front tuning, which simply centres/flattens the passband.
    const double Vf = Vr;

    BoxModel w = tmpl;
    w.encType = BoxModel::EncType::Bandpass4;
    w.volumeL = Vr;
    w.volumeFront_L = Vf;
    if (!(w.QLFront > 0.0)) w.QLFront = 7.0;

    auto ripple = [&](double fb) -> double {
        BoxModel t = w; t.fbFront = fb;
        const BPResponse p = bandpassResponse(t);
        return p.valid ? p.rippleDb : 1e9;
    };
    const double fb = goldenMin(ripple, 0.55 * Fc, 1.30 * Fc);   // flatten / centre

    r.Vr_L = Vr; r.Vf_L = Vf; r.fbFront = fb; r.ok = true;
    return r;
}

struct BP6FlatResult { double Vr_L, fbRear, Vf_L, fbFront; bool ok; };

// Compute a maximally-flat BP6 (dual-reflex, both chambers vented) alignment
// from the driver T/S and port geometry in `tmpl`. Both chamber volumes are
// pinned symmetric to the same compliance sizing as BP4 (Qbc = 1/sqrt2); the two
// freedoms that remain are the rear and front port tunings, which are optimised
// against the validated circuit model: the rear port extends the low edge and
// the front port shapes the top, for the flattest controlled passband. Returns
// ok=false when Qts is out of range (needs 0 < Qts < 1/sqrt2).
inline BP6FlatResult bp6MaxFlat(const BoxModel &tmpl)
{
    BP6FlatResult r{0.0, 0.0, 0.0, 0.0, false};
    const double Qts = tmpl.Qts, Vas = tmpl.Vas_L, fs = tmpl.fs;
    constexpr double Qbc = 0.70710678;
    if (!(fs > 0.0) || !(Vas > 0.0) || !(Qts > 0.0) || Qts >= Qbc) return r;

    const double alphaR = (Qbc / Qts) * (Qbc / Qts) - 1.0;
    const double Vr = Vas / alphaR;
    const double Vf = Vr;                                    // symmetric chambers
    const double Fc = fs * (Qbc / Qts);

    BoxModel w = tmpl;
    w.encType = BoxModel::EncType::Bandpass6;
    w.volumeL = Vr;
    w.volumeFront_L = Vf;
    if (!(w.QL > 0.0))      w.QL = 7.0;
    if (!(w.QLFront > 0.0)) w.QLFront = 7.0;

    auto ripple = [&](double fbRear, double fbFront) -> double {
        BoxModel t = w; t.fb = fbRear; t.fbFront = fbFront;
        const BPResponse p = bandpassResponse(t);
        return p.valid ? p.rippleDb : 1e9;
    };

    // Coordinate descent over the two tunings. Rear stays below the front so the
    // two ports extend opposite edges of the passband (the point of a BP6).
    double fbRear = 0.65 * Fc, fbFront = 1.05 * Fc;          // seed: rear low, front high
    for (int round = 0; round < 4; ++round) {
        fbRear  = goldenMin([&](double x) { return ripple(x, fbFront); },
                            0.40 * Fc, std::min(fbFront, 1.05 * Fc));
        fbFront = goldenMin([&](double x) { return ripple(fbRear, x); },
                            std::max(fbRear, 0.85 * Fc), 1.70 * Fc);
    }

    r.Vr_L = Vr; r.fbRear = fbRear; r.Vf_L = Vf; r.fbFront = fbFront; r.ok = true;
    return r;
}

} // namespace pp
