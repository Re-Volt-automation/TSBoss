#include "mounting.h"
#include "portphysics.h"
#include "plots/modeleval.h"
#include <cstdio>
#include <cmath>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// Self-consistent measured driver (JBL Stage82 numbers).
static BoxModel makeDriver()
{
    BoxModel m;
    m.fs = 41.6; m.Qms = 5.46; m.Qes = 0.37; m.Qts = 0.35;
    m.Re = 3.6;  m.mms_g = 56.2; m.BL = 11.96;
    m.Sd_cm2 = 208.7; m.Vas_L = 15.9;
    m.xmax_mm = 6.2; m.pe_W = 200.0;
    m.numDrivers = 1;
    return m;
}

int main()
{
    // ── The transform itself ──────────────────────────────────────
    {
        BoxModel m = makeDriver();
        m.mounting = BoxModel::Mounting::Isobaric;
        const BoxModel p = mounting::withMounting(m);
        check(std::fabs(p.mms_g  - 2*56.2)  < 1e-9, "pair: mms doubled");
        check(std::fabs(p.BL     - 2*11.96) < 1e-9, "pair: BL doubled (series)");
        check(std::fabs(p.Re     - 2*3.6)   < 1e-9, "pair: Re doubled (series)");
        check(std::fabs(p.Vas_L  - 15.9/2)  < 1e-9, "pair: Vas halved");
        check(std::fabs(p.pe_W   - 400.0)   < 1e-9, "pair: Pe doubled");
        check(p.fs == m.fs && p.Qts == m.Qts && p.Qes == m.Qes && p.Qms == m.Qms,
              "pair: fs and Q factors unchanged");
        check(p.Sd_cm2 == m.Sd_cm2 && p.xmax_mm == m.xmax_mm,
              "pair: Sd and Xmax unchanged");
        check(p.mounting == BoxModel::Mounting::Normal,
              "transform is idempotent (flag cleared)");
        const BoxModel n = mounting::withMounting(makeDriver());
        check(n.mms_g == 56.2 && n.Vas_L == 15.9,
              "normal mounting passes through untouched");
    }

    // ── Classic equivalence: iso pair in Vb == single in 2·Vb,
    //    −3 dB at equal power, 2× electrical impedance ─────────────
    {
        BoxModel iso = makeDriver();
        iso.mounting = BoxModel::Mounting::Isobaric;
        iso.encType  = BoxModel::EncType::Vented;
        iso.volumeL  = 20.0; iso.fb = 30.0; iso.QL = 7.0;
        iso.portShape = 0; iso.portWidth_mm = 75.0; iso.numPorts = 1;
        const BoxModel pair = mounting::withMounting(iso);

        BoxModel single = makeDriver();
        single.encType  = BoxModel::EncType::Vented;
        single.volumeL  = 40.0; single.fb = 30.0; single.QL = 7.0;
        single.portShape = 0; single.portWidth_mm = 75.0; single.numPorts = 1;

        // portedSplRaw is per-VOLT: the series pair moves at exactly half
        // the velocity, so the raw ratio is 1/2 at every frequency — i.e.
        // the normalized response SHAPES are identical. The −3 dB at equal
        // POWER comes from the η/2 SPL anchor (halved Vas), asserted below.
        bool shapeOk = true, zOk = true;
        for (double f : {12., 20., 30., 45., 70., 100., 200., 500., 1000.}) {
            const double rIso = pp::portedSplRaw(pair,   f, 0.0);
            const double rSgl = pp::portedSplRaw(single, f, 0.0);
            if (rSgl <= 0) { shapeOk = false; continue; }
            if (std::fabs(rIso / rSgl - 0.5) > 1e-6) shapeOk = false;

            const double zIso = pp::portedImpedance(pair,   f, 0.0);
            const double zSgl = pp::portedImpedance(single, f, 0.0);
            if (std::fabs(zIso / zSgl - 2.0) > 1e-6) zOk = false;
        }
        check(shapeOk, "equivalence: identical response shape (raw ratio 1/2 per volt)");
        check(zOk,     "equivalence: impedance is exactly 2x across the range");

        // η ∝ fs³·Vas_eff/Qes with fs and Qes unchanged: the pair's halved
        // Vas puts the SPL anchor exactly 3 dB below the single driver.
        check(std::fabs(pair.Vas_L / single.Vas_L - 0.5) < 1e-12,
              "equivalence: eta anchor gives -3 dB at equal power");
    }

    // ── Thermal capacity flows through the baked Pe ───────────────
    {
        BoxModel iso = makeDriver();
        iso.mounting = BoxModel::Mounting::Isobaric;
        iso.numDrivers = 2;   // two pairs
        const BoxModel pair = mounting::withMounting(iso);
        check(std::fabs(thermalPowerLimit(pair) - 800.0) < 1e-9,
              "thermal: 2 pairs x 2 coils x 200 W = 800 W");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll isobaric tests passed.\n");
    return g_failures;
}
