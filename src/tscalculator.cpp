#include "tscalculator.h"
#include <cmath>

// ── All formulae taken directly from SB Acoustics technical note ──

double TSCalculator::driverVoltage(double V_source, double R_s, double Z)
{
    if (R_s + Z <= 0.0) return 0.0;
    return V_source * Z / (R_s + Z);
}

void TSCalculator::calculate(DriverRecord &r)
{
    // Z₁₂ = √(Re · Zmax)
    r.Z12 = std::sqrt(r.Re * r.Zmax);

    // Verification: √(f1 · f2) should equal fs
    if (r.f1 > 0.0 && r.f2 > 0.0)
        r.fsVerify = std::sqrt(r.f1 * r.f2);

    // R₀ = Zmax / Re
    r.R0 = (r.Re > 0.0) ? (r.Zmax / r.Re) : 0.0;

    // Qms = fs/(f2-f1) · √R₀
    const double bw = r.f2 - r.f1;
    if (bw > 0.0 && r.R0 > 0.0)
        r.Qms = (r.fs / bw) * std::sqrt(r.R0);

    // Qes = Qms / (R₀ - 1)
    if (r.R0 > 1.0 && r.Qms > 0.0)
        r.Qes = r.Qms / (r.R0 - 1.0);

    // Qts = Qms · Qes / (Qms + Qes)
    if (r.Qms + r.Qes > 0.0)
        r.Qts = (r.Qms * r.Qes) / (r.Qms + r.Qes);

    // mms = Δm / ((fs/fo)² − 1)
    if (r.fo > 0.0 && r.fs > 0.0) {
        const double ratio = r.fs / r.fo;
        const double denom = ratio * ratio - 1.0;
        if (denom > 0.0)
            r.mms = r.deltaM / denom;
    }

    // Rms = 2π · fs · mms / Qms
    if (r.Qms > 0.0 && r.mms > 0.0)
        r.Rms = (2.0 * PI * r.fs * r.mms) / r.Qms;

    // BL = √((Zmax − Re) · Rms)
    if (r.Zmax > r.Re && r.Rms > 0.0)
        r.BL = std::sqrt((r.Zmax - r.Re) * r.Rms);

    // Cms = 1 / ((2π·fs)² · mms)
    if (r.mms > 0.0 && r.fs > 0.0) {
        const double ws = 2.0 * PI * r.fs;
        r.Cms = 1.0 / (ws * ws * r.mms);
    }

    // Sd = π · (Dd/2)²
    r.Sd = PI * (r.Dd / 2.0) * (r.Dd / 2.0);

    // Vas = Cms · ρ · c² · Sd²
    if (r.Cms > 0.0 && r.Sd > 0.0)
        r.Vas = r.Cms * RHO * C * C * r.Sd * r.Sd;

    // Le = [(Re · 20000 / (2π·f3)) + 0.5] · 1e-3 / 20
    //      (empirical model – SB Acoustics technical note p.6)
    if (r.f3 > 0.0 && r.Re > 0.0) {
        const double inner = (r.Re * 20000.0) / (2.0 * PI * r.f3) + 0.5;
        r.Le = inner * 1e-3 / 20.0;
    }
}

bool TSCalculator::validateInputs(const DriverRecord &r, QString &msg)
{
    if (r.Re   <= 0.0)            { msg = "Re must be > 0 Ω";                       return false; }
    if (r.fs   <= 0.0)            { msg = "fs must be > 0 Hz";                       return false; }
    if (r.Zmax <= r.Re)           { msg = "Zmax must be > Re";                       return false; }
    if (r.f1   <= 0.0)            { msg = "f1 must be > 0 Hz";                       return false; }
    if (r.f2   <= 0.0)            { msg = "f2 must be > 0 Hz";                       return false; }
    if (r.f1   >= r.f2)           { msg = "f1 must be < f2";                         return false; }
    if (r.f1   >= r.fs)           { msg = "fs must be between f1 and f2";            return false; }
    if (r.f2   <= r.fs)           { msg = "fs must be between f1 and f2";            return false; }
    if (r.deltaM <= 0.0)          { msg = "Added mass (Δm) must be > 0 g";           return false; }
    if (r.fo   <= 0.0)            { msg = "fo must be > 0 Hz";                       return false; }
    if (r.fo   >= r.fs)           { msg = "fo must be < fs (mass shifts resonance down)"; return false; }
    if (r.Dd   <= 0.0)            { msg = "Piston diameter Dd must be > 0 mm";       return false; }
    if (r.Zmin <= 0.0)            { msg = "Zmin must be > 0 Ω";                      return false; }
    if (r.f3   <= 0.0)            { msg = "f3 must be > 0 Hz";                       return false; }
    if (r.f3   <= r.fs)           { msg = "f3 must be > fs (it is above resonance)"; return false; }
    return true;
}
