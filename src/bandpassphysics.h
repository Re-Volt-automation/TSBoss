#pragma once
#include "portphysics.h"
#include <complex>
#include <cmath>

namespace pp {

// Bandpass enclosure type test (avoids a dependency on enclosurewidget's isBP6).
inline bool bpIsBP6(const BoxModel &m) { return m.encType == BoxModel::EncType::Bandpass6; }

inline bool hasBP4Data(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0
        && m.volumeL > 0                         // rear sealed
        && m.volumeFront_L > 0 && m.fbFront > 0  // front vented
        && m.QLFront > 0;
}

inline bool hasBP6Data(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0
        && m.volumeL > 0 && m.fb > 0 && m.QL > 0  // rear vented
        && m.volumeFront_L > 0 && m.fbFront > 0 && m.QLFront > 0;
}

struct BPAmps { Cpx cone, rearPort, frontPort; };

// Velocity-aware per-chamber acoustic impedance. u = port air velocity [m/s];
// Ap = per-port area [m²]; flare selects the turbulent loss coefficient.
inline Cpx chamberAcousticZ(double Vb_L, double fb, double QL_, double omega,
                            double u, double Ap, int flare)
{
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    if (fb <= 0.0) return Zcab;                        // sealed chamber
    const double omegab  = 2.0 * PI * fb;
    const double Map     = 1.0 / (omegab*omegab*Cab);
    const double Rp_visc = omegab * Map / std::max(QL_, 0.1);
    const double Rp_turb = 0.5 * RHO * flareK(flare) * std::abs(u) / std::max(Ap, 1e-9);
    const Cpx Zport(Rp_visc + Rp_turb, omega*Map);
    return Zcab * Zport / (Zcab + Zport);
}

inline Cpx chamberPortFlow(const Cpx &Sdv, double Vb_L, double fb, double QL_, double omega,
                           double u, double Ap, int flare)
{
    if (fb <= 0.0 || Vb_L <= 0.0) return Cpx(0.0, 0.0);
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const double omegab  = 2.0 * PI * fb;
    const double Map     = 1.0 / (omegab*omegab*Cab);
    const double Rp_visc = omegab * Map / std::max(QL_, 0.1);
    const double Rp_turb = 0.5 * RHO * flareK(flare) * std::abs(u) / std::max(Ap, 1e-9);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Zport(Rp_visc + Rp_turb, omega*Map);
    const Cpx Za = Zcab * Zport / (Zcab + Zport);
    return (Sdv * Za) / Zport;
}

struct BPSolution {
    Cpx    cone, rearPort, frontPort;   // 1 V-normalized acoustic volume velocities
    double Zin;                         // |input electrical impedance| [Ω]
    double uRear, uFront;               // peak port air velocities [m/s] at `power`
};

// One core evaluation of the circuit at fixed port velocities (uRear,uFront).
struct BPCore { Cpx cone, rearPort, frontPort; double Zin; };
inline BPCore bandpassCore(const BoxModel &m, double f, bool bp6,
                           double uRear, double uFront)
{
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms = ds.mms, Sd = ds.Sd, BL = ds.BL, Re_eff = ds.Re_eff;
    const double Cms = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms = 2.0*PI * m.fs * mms / m.Qms;

    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const double ApRear  = std::max(portArea_m2(m),      1e-9);
    const double ApFront = std::max(portFrontArea_m2(m), 1e-9);

    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,
                                         omega, uRear,  ApRear,  m.portFlare);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront,
                                         omega, uFront, ApFront, m.portFrontFlare);
    const Cpx Za_total = ZaRear + ZaFront;

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za_total);
    if (std::abs(denom) < 1e-100) return { Cpx(0.0), Cpx(0.0), Cpx(0.0), m.Re };
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Sdv = Sd * v;
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za_total;
    const double Zin = std::abs(Ze + Cpx(BL*BL) / Zmot);

    BPCore c;
    c.cone      = Sdv;
    c.rearPort  = chamberPortFlow(Sdv, m.volumeL,       rearFb,    rearQL,
                                  omega, uRear,  ApRear,  m.portFlare);
    c.frontPort = chamberPortFlow(Sdv, m.volumeFront_L, m.fbFront, m.QLFront,
                                  omega, uFront, ApFront, m.portFrontFlare);
    c.Zin = Zin;
    return c;
}

// Power-aware coupled fixed-point on (uRear,uFront). Mirrors pp::portedSolve:
// cold start u=0, under-relaxation alpha=0.5, 24 iterations, 1e-4 relative tol.
inline BPSolution bandpassSolve(const BoxModel &m, double f, double power)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m))
        return { Cpx(0.0), Cpx(0.0), Cpx(0.0), m.Re, 0.0, 0.0 };

    const double ApRear  = std::max(portArea_m2(m),      1e-9);
    const double ApFront = std::max(portFrontArea_m2(m), 1e-9);
    const int    NRear   = std::max(1, m.numPorts);
    const int    NFront  = std::max(1, m.numPortsFront);
    constexpr double alpha = 0.5;

    double uRear = 0.0, uFront = 0.0;
    BPCore c{};
    for (int i = 0; i < 24; ++i) {
        c = bandpassCore(m, f, bp6, uRear, uFront);
        const double V = (c.Zin > 0.0 && power > 0.0) ? std::sqrt(power * c.Zin) : 0.0;
        const double tgtR = bp6 ? std::abs(c.rearPort) * V / (NRear * ApRear) : 0.0;
        const double tgtF =        std::abs(c.frontPort) * V / (NFront * ApFront);
        const double nR = uRear  + alpha * (tgtR - uRear);
        const double nF = uFront + alpha * (tgtF - uFront);
        const bool done = std::abs(nR - uRear) <= 1e-4 * std::max(nR, 1.0)
                       && std::abs(nF - uFront) <= 1e-4 * std::max(nF, 1.0);
        uRear = nR; uFront = nF;
        if (done) break;
    }
    c = bandpassCore(m, f, bp6, uRear, uFront);   // final self-consistent recompute
    return { c.cone, c.rearPort, c.frontPort, c.Zin, uRear, uFront };
}

inline BPAmps bandpassAmplitudes(const BoxModel &m, double f, double power)
{ auto s = bandpassSolve(m, f, power); return { s.cone, s.rearPort, s.frontPort }; }

inline Cpx bandpassUa(const BoxModel &m, double f, double power)
{ auto s = bandpassSolve(m, f, power);
  return bpIsBP6(m) ? (s.frontPort - s.rearPort) : s.frontPort; }

inline double bandpassSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassUa(m, f, power)); }

inline double bandpassConeSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassSolve(m, f, power).cone); }

inline double bandpassRearPortSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassSolve(m, f, power).rearPort); }

inline double bandpassFrontPortSplRaw(const BoxModel &m, double f, double power)
{ return (2.0*PI*f) * std::abs(bandpassSolve(m, f, power).frontPort); }

inline double bandpassGroupDelay(const BoxModel &m, double f, double power)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = bandpassUa(m, f + df, power);
    const Cpx ua2 = bandpassUa(m, f - df, power);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;
}

inline double bandpassImpedance(const BoxModel &m, double f, double power)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return m.Re;
    return bandpassSolve(m, f, power).Zin;
}

// Peak air velocity [m/s] of one bandpass chamber port.
inline double bandpassPortVelocity(const BoxModel &m, double f, double power, bool front)
{ auto s = bandpassSolve(m, f, power); return front ? s.uFront : s.uRear; }

// True when a bandpass model can produce a velocity curve for the given port.
inline bool hasBandpassPortVelocityData(const BoxModel &m, bool front)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return false;
    if (front) return portFrontArea_m2(m) > 0.0;
    return bp6 && portArea_m2(m) > 0.0;             // BP4 rear is sealed -> no rear port
}

} // namespace pp
