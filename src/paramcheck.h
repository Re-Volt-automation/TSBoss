#pragma once
#include "boxmodel.h"
#include <QString>
#include <QStringList>
#include <cmath>

// ─────────────────────────────────────────────────────────────────
//  paramcheck – driver-parameter sanity checks shared by the
//  enclosure page and the driver detail view. A self-consistent T/S
//  set must satisfy the physics identities below, so large deviations
//  reliably indicate data-entry mistakes (placeholder Rₑ, slipped
//  decimal points, …). Header-only and unit-tested
//  (see tests/paramcheck_tests.cpp).
// ─────────────────────────────────────────────────────────────────

namespace paramcheck {

inline constexpr double PI  = 3.14159265358979323846;
inline constexpr double RHO = 1.2;    // air density [kg/m³]
inline constexpr double C   = 344.0;  // speed of sound [m/s]

/// Identity tolerance: datasheet rounding commonly disagrees by
/// 10–20%, so only clearly-broken values (>35%) are flagged.
inline constexpr double kIdentityTol = 0.35;
/// Qts = Qms·Qes/(Qms+Qes) is exact math, so it gets a tighter band.
inline constexpr double kQtsTol      = 0.20;
/// Below this the voice coil is a near-short: electrical damping
/// collapses and every circuit-based simulation becomes meaningless.
inline constexpr double kMinRe       = 0.5;

struct DriverParams {
    double fs     = 0.0;   // [Hz]
    double Qms    = 0.0;
    double Qes    = 0.0;
    double Qts    = 0.0;
    double Re     = 0.0;   // [Ω]
    double mms_g  = 0.0;   // [g]
    double BL     = 0.0;   // [Tm]
    double Sd_cm2 = 0.0;   // [cm²]
    double Vas_L  = 0.0;   // [L]
};

inline DriverParams fromModel(const BoxModel &m)
{
    DriverParams p;
    p.fs = m.fs; p.Qms = m.Qms; p.Qes = m.Qes; p.Qts = m.Qts;
    p.Re = m.Re; p.mms_g = m.mms_g; p.BL = m.BL;
    p.Sd_cm2 = m.Sd_cm2; p.Vas_L = m.Vas_L;
    return p;
}

inline double relDev(double a, double b)
{
    const double n = std::max(std::fabs(a), std::fabs(b));
    return n > 0.0 ? std::fabs(a - b) / n : 0.0;
}

/// Human-readable warnings; empty when nothing looks suspicious.
/// Missing fields (≤ 0) skip their identity checks — sparse datasheet
/// records stay silent except for a missing Rₑ, which the simulations
/// genuinely cannot do without.
inline QStringList driverParamWarnings(const DriverParams &p)
{
    QStringList w;

    // ── Rₑ: missing or placeholder ────────────────────────────────
    if (p.Re <= 0.0) {
        w << QStringLiteral(
            "Rₑ is missing — vented, impedance, power and max-SPL "
            "results need the driver's DC resistance.");
    } else if (p.Re < kMinRe) {
        w << QString(
            "Rₑ = %1 Ω looks like a placeholder — below ≈%2 Ω electrical "
            "damping collapses and simulation results become meaningless.")
            .arg(p.Re, 0, 'f', 2).arg(kMinRe, 0, 'f', 1);
    }

    // ── Identity: Qes = 2π·fs·mms·Rₑ / BL² ────────────────────────
    if (p.fs > 0 && p.mms_g > 0 && p.Re > 0 && p.BL > 0 && p.Qes > 0) {
        const double qesImp =
            2.0 * PI * p.fs * (p.mms_g * 1e-3) * p.Re / (p.BL * p.BL);
        if (relDev(p.Qes, qesImp) > kIdentityTol)
            w << QString(
                "Qes %1 disagrees with the value implied by fs, mms, Rₑ "
                "and BL (≈ %2) — one of them is likely mistyped.")
                .arg(p.Qes, 0, 'f', 3).arg(qesImp, 0, 'f', 3);
    }

    // ── Identity: Vas = ρ·c²·Sd² / (4π²·fs²·mms) ──────────────────
    if (p.fs > 0 && p.mms_g > 0 && p.Sd_cm2 > 0 && p.Vas_L > 0) {
        const double Sd  = p.Sd_cm2 * 1e-4;
        const double vasImp_L =
            RHO * C * C * Sd * Sd
            / (4.0 * PI * PI * p.fs * p.fs * (p.mms_g * 1e-3)) * 1000.0;
        if (relDev(p.Vas_L, vasImp_L) > kIdentityTol)
            w << QString(
                "Vas %1 L disagrees with the value implied by fs, mms and "
                "Sd (≈ %2 L) — check fs and mms for slipped decimals.")
                .arg(p.Vas_L, 0, 'f', 1).arg(vasImp_L, 0, 'f', 1);
    }

    // ── Identity: Qts = Qms·Qes / (Qms+Qes) ───────────────────────
    if (p.Qms > 0 && p.Qes > 0 && p.Qts > 0) {
        const double qtsImp = p.Qms * p.Qes / (p.Qms + p.Qes);
        if (relDev(p.Qts, qtsImp) > kQtsTol)
            w << QString("Qts %1 doesn't match Qms·Qes/(Qms+Qes) ≈ %2.")
                .arg(p.Qts, 0, 'f', 3).arg(qtsImp, 0, 'f', 3);
    }

    return w;
}

} // namespace paramcheck
