#pragma once
#include "boxmodel.h"
#include "tscalculator.h"
#include <complex>
#include <cmath>

namespace pp {

using Cpx = std::complex<double>;
inline constexpr double PI  = TSCalculator::PI;
inline constexpr double RHO = 1.2;            // kg/m³ air density

inline double g_C = TSCalculator::C;          // speed of sound [m/s], adjustable
inline void   setSpeedOfSound(double c) { g_C = c; }

// ─────────────────────────────────────────────────────────────────
//  Ported-box numerical simulation helpers
// ─────────────────────────────────────────────────────────────────

// Per-port cross-sectional area [m²]
inline double portArea_m2(const BoxModel &m)
{
    if (m.portShape == 0) {
        const double Dp = m.portWidth_mm / 1000.0;
        return PI * (Dp / 2.0) * (Dp / 2.0);
    }
    return (m.portWidth_mm / 1000.0) * (m.portHeight_mm / 1000.0);
}

// True when the model has the full set of driver + box data needed
// for ported simulation.
inline bool hasPortedData(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0 && m.volumeL > 0
        && m.fb > 0 && m.QL > 0;
}

// Effective driver parameters scaled for multi-driver wiring mode.
// Series  (default): BL×N, Re×N — impedances add, force adds.
// Parallel:          BL×1, Re/N — all coils see same voltage.
// Separate:          BL×N, Re×1 — each driver driven by its own amp channel.
// mms and Sd always scale by N (mechanical loads add coherently).
struct DriverScaling { double BL, Re_eff, mms, Sd; };
inline DriverScaling driverScaling(const BoxModel &m)
{
    const double N   = m.numDrivers;
    const double mms = m.mms_g * 1e-3 * N;
    const double Sd  = m.Sd_cm2 * 1e-4 * N;
    switch (m.wiringMode) {
        case BoxModel::WiringMode::Parallel: return { m.BL,       m.Re / N, mms, Sd };
        case BoxModel::WiringMode::Separate: return { m.BL * N,   m.Re,     mms, Sd };
        default:                             return { m.BL * N,   m.Re * N, mms, Sd };
    }
}

// Turbulent end-loss coefficient from BoxModel::portFlare
// (0 = straight, 1 = one end flared, 2 = both ends flared).
// Stub returns 0 (inert) until Task 6 activates the turbulent term.
inline double flareK(int /*portFlare*/) { return 0.0; }

// Velocity-aware complex port impedance. u = port particle velocity [m/s].
//   Zport(u) = (Rp_visc + Rp_turb(u)) + jω·Map(u)
inline Cpx portZ(const BoxModel &m, double f, double u)
{
    const double omega  = 2.0 * PI * f;
    const double omegab = 2.0 * PI * m.fb;
    const double Vb     = m.volumeL * 1e-3;
    // Note: Cab is derived here and again in portCore (both from volumeL); kept
    // independent so portZ stays a self-contained impedance model.
    const double Cab    = Vb / (RHO * g_C * g_C);
    const double Map0   = 1.0 / (omegab * omegab * Cab);
    const double Rp_visc = omegab * Map0 / m.QL;

    const double Ap      = std::max(portArea_m2(m), 1e-9);
    const double Rp_turb = 0.5 * RHO * flareK(m.portFlare) * std::abs(u) / Ap;
    // mapVelCoeff is a BoxModel field defaulting to 0 (tuning-shift hook, off in v1),
    // so Map == Map0 today; wired in for later validation.
    const double Map     = Map0 * (1.0 - m.mapVelCoeff * std::abs(u));
    return Cpx(Rp_visc + Rp_turb, omega * Map);
}

struct PortCoreResult { Cpx cone, port; double Zin; };

// Solve the driver/box/port circuit for a 1 V drive given a fixed port impedance.
inline PortCoreResult portCore(const BoxModel &m, double f, const Cpx &Zport)
{
    const auto   ds   = driverScaling(m);
    const double omega = 2.0 * PI * f;
    const double mms   = ds.mms, Sd = ds.Sd, BL = ds.BL, Re_eff = ds.Re_eff;
    const double Vb    = m.volumeL * 1e-3;
    const double Cms   = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms   = 2.0*PI * m.fs * mms / m.Qms;
    const double Cab   = Vb / (RHO * g_C * g_C);

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Za    = Zcab * Zport / (Zcab + Zport);
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za);
    // Degenerate solve: no motion. Zin falls back to Re (unloaded voice-coil impedance).
    if (std::abs(denom) < 1e-100) return { Cpx(0.0), Cpx(0.0), m.Re };
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Up  = (Sd * v * Za) / Zport;
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za;
    const double Zin = std::abs(Ze + Cpx(BL*BL) / Zmot);
    return { Sd * v, Up, Zin };
}

// Decomposed acoustic output: cone = Sd·v,  port = Up
struct PortedAmps { Cpx cone, port; };

inline PortedAmps portedAmplitudes(const BoxModel &m, double f)
{
    if (!hasPortedData(m)) return {};
    const PortCoreResult c = portCore(m, f, portZ(m, f, 0.0));
    return { c.cone, c.port };
}

// Total radiated acoustic volume velocity.
// Cone and port are acoustically anti-phase: positive cone velocity (inward)
// compresses the box, driving the port outward. From the listener the cone
// contribution is −Sd·v and the port is +Up, so the net is proportional to
// Sd·v − Up.  Using the opposite sign would give a null at ≈√2·fb and only
// a 12 dB/oct rolloff below fb instead of the correct 24 dB/oct (4th order).
inline Cpx portedUa(const BoxModel &m, double f)
{
    auto a = portedAmplitudes(m, f);
    return a.cone - a.port;
}

// ω·|Ua|  (total, cone-only, port-only) — unnormalised far-field pressure proxy
inline double portedSplRaw     (const BoxModel &m, double f)
{ return (2.0*PI*f)*std::abs(portedUa(m,f)); }

inline double portedConeSplRaw (const BoxModel &m, double f)
{ auto a=portedAmplitudes(m,f); return (2.0*PI*f)*std::abs(a.cone); }

inline double portedPortSplRaw (const BoxModel &m, double f)
{ auto a=portedAmplitudes(m,f); return (2.0*PI*f)*std::abs(a.port); }

// Group delay [ms] via numerical phase derivative of Ua
inline double portedGroupDelay(const BoxModel &m, double f)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = portedUa(m, f + df);
    const Cpx ua2 = portedUa(m, f - df);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    // Group delay τ = −dφ/dω.  Central-difference: dφ ≈ arg(conj(ua2)·ua1)
    // which equals φ(f+df)−φ(f−df).  For a causal system the phase decreases
    // with frequency, so dφ/dω < 0 and τ = −dφ/dω > 0.
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;   // negate: τ = −dφ/dω, result in ms
}

// Input electrical impedance magnitude [Ω] for a ported box
inline double portedImpedance(const BoxModel &m, double f)
{
    if (!hasPortedData(m)) return m.Re;
    return portCore(m, f, portZ(m, f, 0.0)).Zin;
}

// Peak port air velocity [m/s] at given input power [W]
inline double portAirVelocity(const BoxModel &m, double f, double power)
{
    if (!hasPortedData(m) || m.Re <= 0) return 0.0;
    const double Ap = portArea_m2(m);
    if (Ap <= 0) return 0.0;
    const int    N  = std::max(1, m.numPorts);
    const double Z  = portedImpedance(m, f);
    if (Z <= 0) return 0.0;
    const double V  = std::sqrt(power * Z);
    const auto   a  = portedAmplitudes(m, f);
    return std::abs(a.port) * V / (N * Ap);
}

inline bool hasPortVelocityData(const BoxModel &m)
{
    return m.encType == BoxModel::EncType::Vented && hasPortedData(m) && m.Re > 0 && portArea_m2(m) > 0;
}

} // namespace pp
