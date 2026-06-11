#pragma once
#include "plotsupport.h"
#include "portphysics.h"
#include "bandpassphysics.h"
#include "filters.h"
#include <algorithm>
#include <cmath>
#include <limits>

// ─────────────────────────────────────────────────────────────────
//  modeleval – model evaluation shared by the plots: electrical
//  impedance, cone displacement, small-signal SPL and the maximum
//  power/SPL solver. Header-only and Qt-Widgets-free so it is
//  unit-testable (see tests/modeleval_tests.cpp).
// ─────────────────────────────────────────────────────────────────

// ── Subsonic filter (line-level, before the amp) ──────────────────
// Off (hpfOrder < 2) is exact unity, so models without a filter are
// bit-identical to the pre-filter behaviour.

inline filt::Cpx hpfH(const BoxModel &m, double f)
{
    return (m.hpfOrder >= 2) ? filt::butterworthHP(m.hpfOrder, m.hpfFreq, f)
                             : filt::Cpx(1.0, 0.0);
}

/// Level shift [dB] the filter applies at f.
inline double hpfDb(const BoxModel &m, double f)
{
    if (m.hpfOrder < 2) return 0.0;
    return 20.0 * std::log10(std::max(std::abs(hpfH(m, f)), 1e-12));
}

/// Power reaching the driver scales by |H|².
inline double hpfPowerScale(const BoxModel &m, double f)
{
    if (m.hpfOrder < 2) return 1.0;
    const double a = std::abs(hpfH(m, f));
    return a * a;
}

/// Group delay [ms] the filter adds.
inline double hpfGroupDelayMs(const BoxModel &m, double f)
{
    return (m.hpfOrder >= 2) ? filt::groupDelayMs(m.hpfOrder, m.hpfFreq, f) : 0.0;
}

// Impedance |Z(f)| for the whole system (all N drivers, correct for sealed/IB/vented).
inline bool hasSystemZData(const BoxModel &m)
{
    if (isVented(m))         return pp::hasPortedData(m);
    if (isBP4(m))            return pp::hasBP4Data(m);
    if (isBP6(m))            return pp::hasBP6Data(m);
    if (isIB(m)) return m.Re > 0 && m.BL > 0 && m.mms_g > 0 && m.Qms > 0 && m.Fc > 0;
    return m.Re > 0 && m.BL > 0 && m.mms_g > 0 && m.Qms > 0 && m.Fc > 0 && m.alpha > 0;
}

inline double systemImpedance(const BoxModel &m, double f, double power)
{
    if (isVented(m))   return pp::portedImpedance(m, f, power);   // portedImpedance already scales for N
    if (isBandpass(m)) return pp::bandpassImpedance(m, f, power); // bandpassImpedance scales for N
    const double N = m.numDrivers;
    if (!hasSystemZData(m)) return m.Re * N;
    // Sealed / IB: use pre-computed Fc (which already incorporates box stiffness and N*Vas).
    // Single-driver equivalent circuit; final impedance scaled by wiring mode.
    const double wc      = 2.0 * pp::PI * m.Fc;
    const double mms     = m.mms_g * 1e-3;
    const double Cms_box = 1.0 / (wc * wc * mms);
    const double Qms_c   = m.Qms * (isIB(m) ? 1.0 : std::sqrt(m.alpha + 1.0));
    const double Rms_box = wc * mms / Qms_c;
    const double w       = 2.0 * pp::PI * f;
    const double Zm_i    = w * mms - 1.0 / (w * Cms_box);
    const double Zm2     = Rms_box * Rms_box + Zm_i * Zm_i;
    const double Zr      = m.Re + m.BL * m.BL * Rms_box / Zm2;
    const double Zi      = -m.BL * m.BL * Zm_i / Zm2;
    const double Zsingle = std::sqrt(Zr * Zr + Zi * Zi);
    // Series: N impedances in series. Parallel: N in parallel. Separate: per-channel = single.
    double scale = N;
    if      (m.wiringMode == BoxModel::WiringMode::Parallel)  scale = 1.0 / N;
    else if (m.wiringMode == BoxModel::WiringMode::Separate)  scale = 1.0;
    return scale * Zsingle;
}

// Cone displacement [mm peak] for a given applied power.
// Uses the same motional-impedance circuit as the voltage/impedance plots.
inline double coneDisplacement_mm(const BoxModel &m, double f, double power)
{
    if (power <= 0) return 0.0;
    const double omega = 2.0 * pp::PI * f;
    if (omega < 1e-10) return 0.0;

    if (isVented(m)) {
        if (!pp::hasPortedData(m)) return 0.0;
        const double Sd = m.Sd_cm2 * 1e-4;
        if (Sd <= 0) return 0.0;
        auto a = pp::portedAmplitudes(m, f, power); // cone = Sd·v  for 1 V input
        const double v_1V = std::abs(a.cone) / Sd;
        const double Z = pp::portedImpedance(m, f, power);
        if (Z <= 0) return 0.0;
        // V = sqrt(P·Z),  |x| = |v_1V|·V / ω
        return v_1V * std::sqrt(power * Z) / omega * 1000.0;
    } else if (isBandpass(m)) {
        if (isBP6(m) ? !pp::hasBP6Data(m) : !pp::hasBP4Data(m)) return 0.0;
        const double Sd = m.Sd_cm2 * 1e-4;
        if (Sd <= 0) return 0.0;
        auto a = pp::bandpassAmplitudes(m, f, power);
        const double v_1V = std::abs(a.cone) / Sd;
        const double Z = pp::bandpassImpedance(m, f, power);
        if (Z <= 0) return 0.0;
        return v_1V * std::sqrt(power * Z) / omega * 1000.0;
    } else {
        if (!hasSystemZData(m)) return 0.0;
        const double wc      = 2.0 * pp::PI * m.Fc;
        const double mms     = m.mms_g * 1e-3;
        const double Cms_box = 1.0 / (wc * wc * mms);
        const double Qms_c   = m.Qms * (isIB(m) ? 1.0 : std::sqrt(m.alpha + 1.0));
        const double Rms_box = wc * mms / Qms_c;
        const double Zm_i    = omega * mms - 1.0 / (omega * Cms_box);
        const double Zm_abs  = std::sqrt(Rms_box * Rms_box + Zm_i * Zm_i);
        const double Z_elec  = systemImpedance(m, f, power);
        if (Z_elec <= 0 || Zm_abs <= 0) return 0.0;
        // |x| = BL · |I| / (|Zm| · ω),  |I| = sqrt(P / Z)
        return m.BL * std::sqrt(power / Z_elec) / (Zm_abs * omega) * 1000.0;
    }
}

inline bool hasExcursionData(const BoxModel &m)
{
    if (isVented(m)) return pp::hasPortedData(m);
    if (isBP4(m))    return pp::hasBP4Data(m);
    if (isBP6(m))    return pp::hasBP6Data(m);
    return hasSystemZData(m);
}

// ─────────────────────────────────────────────────────────────────
//  Maximum-SPL solver
// ─────────────────────────────────────────────────────────────────

/// Max-SPL needs the excursion machinery plus a known Xmax.
inline bool hasMaxSplData(const BoxModel &m)
{
    return hasExcursionData(m) && m.xmax_mm > 0.0;
}

/// Largest power [W] in [pLo, pHi] for which value(P) <= limit.
/// value must be monotonically non-decreasing in P. Returns pHi when the
/// limit is never reached, 0 when it is exceeded even at pLo.
template <typename F>
inline double solveMaxPower(F value, double limit,
                            double pLo = 1e-4, double pHi = 2e4)
{
    if (value(pHi) <= limit) return pHi;
    if (value(pLo) >  limit) return 0.0;
    for (int i = 0; i < 20; ++i) {           // log-domain bisection
        const double pm = std::sqrt(pLo * pHi);
        (value(pm) <= limit ? pLo : pHi) = pm;
    }
    return std::sqrt(pLo * pHi);
}

/// Model-specific small-signal reference amplitude (computed once per model).
struct SplContext { double ref = 0.0; };

inline SplContext makeSplContext(const BoxModel &m)
{
    SplContext c;
    if (isVented(m)) {
        if (pp::hasPortedData(m)) c.ref = pp::portedSplRaw(m, 1000.0, 0.0);
    } else if (isBandpass(m)) {
        if (isBP6(m) ? pp::hasBP6Data(m) : pp::hasBP4Data(m)) {
            // Band peak at the small-signal limit (mirrors the SPL plot's scan)
            const double lfMin = std::log10(F_MIN), lfMax = std::log10(F_MAX);
            for (int i = 0; i <= 240; ++i) {
                const double f = std::pow(10.0, lfMin + (lfMax - lfMin) * i / 240.0);
                c.ref = std::max(c.ref, pp::bandpassSplRaw(m, f, 0.0));
            }
        }
    }
    return c;
}

/// Small-signal SPL [dB, 1 W / 1 m] — the same normalization the SPL plot
/// draws. NaN when the model can't produce a curve.
inline double splSmallSignalDb(const BoxModel &m, const SplContext &c, double f)
{
    constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
    if (isVented(m)) {
        if (!pp::hasPortedData(m) || m.spl <= 0 || c.ref <= 0) return kNaN;
        const double raw = pp::portedSplRaw(m, f, 0.0);
        if (raw <= 0) return kNaN;
        return m.spl + 20.0 * std::log10(raw / c.ref);
    }
    if (isBandpass(m)) {
        if ((isBP6(m) ? !pp::hasBP6Data(m) : !pp::hasBP4Data(m)) || c.ref <= 0) return kNaN;
        const double raw = pp::bandpassSplRaw(m, f, 0.0);
        if (raw <= 0) return kNaN;
        const double peakDb = (m.peakSpl > 0) ? m.peakSpl : 100.0;
        return peakDb + 20.0 * std::log10(raw / c.ref);
    }
    // Sealed / IB: closed-form 2nd-order highpass around Fc/Qtc
    if (m.Fc <= 0 || m.Qtc <= 0 || m.spl <= 0) return kNaN;
    const double x   = f / m.Fc;
    const double xsq = x * x;
    const double d1  = 1.0 - xsq;
    const double den = d1 * d1 + (x / m.Qtc) * (x / m.Qtc);
    if (den <= 0) return kNaN;
    return m.spl + 10.0 * std::log10(xsq * xsq / den);
}

struct MaxSplPoint {
    enum Limiter { None = -1, Xmax = 0, Port = 1 };
    double  pMax    = 0.0;
    Limiter limiter = None;
};

/// Largest power at frequency f before exceeding Xmax or the port
/// chuffing limit (whichever binds first).
inline MaxSplPoint maxPowerAt(const BoxModel &m, double f)
{
    MaxSplPoint out;
    if (!hasMaxSplData(m)) return out;

    const double pX = solveMaxPower(
        [&](double P) { return coneDisplacement_mm(m, f, P); }, m.xmax_mm);

    double pV = std::numeric_limits<double>::infinity();
    if (isVented(m) && pp::hasPortVelocityData(m)) {
        pV = solveMaxPower(
            [&](double P) { return pp::portAirVelocity(m, f, P); },
            pp::chuffLimit(m.portFlare));
    } else if (isBandpass(m)) {
        if (pp::hasBandpassPortVelocityData(m, true))
            pV = solveMaxPower(
                [&](double P) { return pp::bandpassPortVelocity(m, f, P, true); },
                pp::chuffLimit(m.portFrontFlare));
        if (pp::hasBandpassPortVelocityData(m, false))
            pV = std::min(pV, solveMaxPower(
                [&](double P) { return pp::bandpassPortVelocity(m, f, P, false); },
                pp::chuffLimit(m.portFlare)));
    }

    if (pV < pX) { out.pMax = pV; out.limiter = MaxSplPoint::Port; }
    else         { out.pMax = pX; out.limiter = MaxSplPoint::Xmax; }
    return out;
}

/// SPL [dB] at the maximum allowed power: small-signal curve + 10·log10(P).
/// (Linear extrapolation by allowed power; the power limit itself already
/// accounts for port compression via the power-aware solvers.)
inline double maxSplDb(const BoxModel &m, const SplContext &c, double f,
                       const MaxSplPoint &mp)
{
    const double base = splSmallSignalDb(m, c, f);
    if (!std::isfinite(base) || mp.pMax <= 0.0)
        return std::numeric_limits<double>::quiet_NaN();
    return base + 10.0 * std::log10(mp.pMax);
}
