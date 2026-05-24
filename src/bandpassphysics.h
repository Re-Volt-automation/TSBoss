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

// Per-chamber acoustic impedance: compliance ∥ (port mass+leakage). Sealed → fb=0 → just compliance.
inline Cpx chamberAcousticZ(double Vb_L, double fb, double QL_, double omega)
{
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    if (fb <= 0.0) return Zcab;
    const double omegab = 2.0 * PI * fb;
    const double Map    = 1.0 / (omegab*omegab*Cab);
    const double Rp     = omegab * Map / std::max(QL_, 0.1);
    const Cpx Zport(Rp, omega*Map);
    return Zcab * Zport / (Zcab + Zport);
}

inline Cpx chamberPortFlow(const Cpx &Sdv, double Vb_L, double fb, double QL_, double omega)
{
    if (fb <= 0.0 || Vb_L <= 0.0) return Cpx(0.0, 0.0);
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const double omegab = 2.0 * PI * fb;
    const double Map    = 1.0 / (omegab*omegab*Cab);
    const double Rp     = omegab * Map / std::max(QL_, 0.1);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Zport(Rp, omega*Map);
    const Cpx Za = Zcab * Zport / (Zcab + Zport);
    return (Sdv * Za) / Zport;
}

inline BPAmps bandpassAmplitudes(const BoxModel &m, double f, bool bp6)
{
    if ((bp6 && !hasBP6Data(m)) || (!bp6 && !hasBP4Data(m))) return {};
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms    = ds.mms;
    const double Sd     = ds.Sd;
    const double BL     = ds.BL;
    const double Re_eff = ds.Re_eff;
    const double Cms    = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms    = 2.0*PI * m.fs * mms / m.Qms;

    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,    omega);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront, omega);
    const Cpx Za_total = ZaRear + ZaFront;

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za_total);
    if (std::abs(denom) < 1e-100) return {};
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Sdv = Sd * v;

    BPAmps a;
    a.cone      = Sdv;
    a.rearPort  = chamberPortFlow(Sdv, m.volumeL,       rearFb,   rearQL,   omega);
    a.frontPort = chamberPortFlow(Sdv, m.volumeFront_L, m.fbFront, m.QLFront, omega);
    return a;
}

inline Cpx bandpassUa(const BoxModel &m, double f)
{
    auto a = bandpassAmplitudes(m, f, bpIsBP6(m));
    return bpIsBP6(m) ? (a.frontPort - a.rearPort) : a.frontPort;
}

inline double bandpassSplRaw(const BoxModel &m, double f)
{ return (2.0*PI*f) * std::abs(bandpassUa(m, f)); }

inline double bandpassConeSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, bpIsBP6(m)); return (2.0*PI*f) * std::abs(a.cone); }

inline double bandpassRearPortSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, bpIsBP6(m)); return (2.0*PI*f) * std::abs(a.rearPort); }

inline double bandpassFrontPortSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, bpIsBP6(m)); return (2.0*PI*f) * std::abs(a.frontPort); }

inline double bandpassGroupDelay(const BoxModel &m, double f)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = bandpassUa(m, f + df);
    const Cpx ua2 = bandpassUa(m, f - df);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;
}

inline double bandpassImpedance(const BoxModel &m, double f)
{
    const bool bp6 = bpIsBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return m.Re;
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms    = ds.mms;
    const double Sd     = ds.Sd;
    const double BL     = ds.BL;
    const double Re_eff = ds.Re_eff;
    const double Cms    = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms    = 2.0*PI * m.fs * mms / m.Qms;
    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,    omega);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront, omega);
    const Cpx Za_total = ZaRear + ZaFront;
    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za_total;
    return std::abs(Ze + Cpx(BL*BL) / Zmot);
}

} // namespace pp
