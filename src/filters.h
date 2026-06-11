#pragma once
#include <complex>
#include <cmath>

// ─────────────────────────────────────────────────────────────────
//  filt – line-level filter responses (subsonic protection).
//  Pure math, no Qt. See tests/filters_tests.cpp.
// ─────────────────────────────────────────────────────────────────
namespace filt {

using Cpx = std::complex<double>;

/// Complex response H(j2πf) of a Butterworth high-pass.
/// order: 0 (or anything < 2) = bypass (unity), 2 or 4 supported.
/// fc: −3 dB corner frequency [Hz].
inline Cpx butterworthHP(int order, double fc, double f)
{
    if (order < 2 || fc <= 0.0 || f <= 0.0) return Cpx(1.0, 0.0);
    const Cpx s(0.0, f / fc);                       // normalized jω
    const Cpx s2 = s * s;
    auto biquad = [&](double Q) { return s2 / (s2 + s / Q + 1.0); };
    if (order >= 4) {
        // 4th-order Butterworth = two biquads with Q = 1/(2·cos(π/8)), 1/(2·cos(3π/8))
        constexpr double kPi = 3.14159265358979323846;
        const double q1 = 1.0 / (2.0 * std::cos(kPi / 8.0));
        const double q2 = 1.0 / (2.0 * std::cos(3.0 * kPi / 8.0));
        return biquad(q1) * biquad(q2);
    }
    return biquad(1.0 / std::sqrt(2.0));            // 2nd-order Butterworth
}

/// Filter group delay [ms] via the numerical phase derivative,
/// same central-difference scheme as the box solvers.
inline double groupDelayMs(int order, double fc, double f)
{
    if (order < 2 || fc <= 0.0 || f <= 0.0) return 0.0;
    const double df = std::max(f * 0.002, 0.02);
    const Cpx h1 = butterworthHP(order, fc, f + df);
    const Cpx h2 = butterworthHP(order, fc, f - df);
    if (std::abs(h1) < 1e-100 || std::abs(h2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(h2) * h1);
    const double domega = 2.0 * 3.14159265358979323846 * 2.0 * df;
    return -dphi / domega * 1000.0;
}

} // namespace filt
