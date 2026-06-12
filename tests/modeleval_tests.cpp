#include "plots/modeleval.h"
#include <cstdio>
#include <cmath>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// Sealed reference: precomputed Fc/Qtc/spl the way recalculate() fills them.
static BoxModel makeSealed()
{
    BoxModel m;
    m.encType = BoxModel::EncType::Sealed;
    m.fs = 30.0; m.Qms = 4.0; m.Qes = 0.4; m.Qts = 0.36;
    m.Re = 6.0;  m.mms_g = 60.0; m.BL = 12.0; m.Sd_cm2 = 220.0;
    m.Vas_L = 80.0; m.volumeL = 40.0;
    m.alpha = 2.0;                       // Vas/Vb
    m.Fc  = m.fs  * std::sqrt(3.0);      // fs*sqrt(alpha+1)
    m.Qtc = m.Qts * std::sqrt(3.0);
    m.eta = 0.005; m.spl = 89.0;
    m.xmax_mm = 8.0;
    m.numDrivers = 1;
    return m;
}

static BoxModel makeVented()
{
    BoxModel m = makeSealed();
    m.encType = BoxModel::EncType::Vented;
    m.fb = 32.0; m.QL = 7.0;
    m.portShape = 0; m.portWidth_mm = 75.0; m.numPorts = 1; m.portFlare = 0;
    return m;
}

int main()
{
    // ── solveMaxPower: bisection basics ───────────────────────────
    {
        // value = sqrt(P): hits limit 10 at P = 100
        const double p = solveMaxPower([](double P){ return std::sqrt(P); }, 10.0);
        check(std::abs(p - 100.0) < 1.0, "solveMaxPower: sqrt law inverts to P=100");

        const double cap = solveMaxPower([](double P){ return P * 1e-9; }, 10.0);
        check(cap >= 1.9e4, "solveMaxPower: unreachable limit returns power cap");

        const double zero = solveMaxPower([](double){ return 50.0; }, 10.0);
        check(zero == 0.0, "solveMaxPower: limit exceeded everywhere returns 0");
    }

    // ── Sealed: bisection matches the analytic sqrt(P) law ────────
    {
        const BoxModel m = makeSealed();
        const double f = 25.0;
        const double x1 = coneDisplacement_mm(m, f, 1.0);
        check(x1 > 0.0, "sealed: 1 W displacement positive");
        // Sealed impedance is power-independent -> x(P) = x1*sqrt(P) exactly.
        const double analytic = (m.xmax_mm / x1) * (m.xmax_mm / x1);
        const auto mp = maxPowerAt(m, f);
        check(mp.limiter == MaxSplPoint::Xmax, "sealed: Xmax is the limiter");
        check(std::abs(mp.pMax - analytic) <= 0.02 * analytic,
              "sealed: bisection matches analytic max power");
    }

    // ── Small-signal SPL curve ────────────────────────────────────
    {
        const BoxModel m = makeSealed();
        const SplContext c = makeSplContext(m);
        // Far above Fc the 2nd-order highpass approaches the rated SPL.
        const double dbHigh = splSmallSignalDb(m, c, 1000.0);
        check(std::isfinite(dbHigh) && std::abs(dbHigh - m.spl) < 0.5,
              "sealed: small-signal SPL approaches rated level at 1 kHz");
        // Far below Fc it must be strongly attenuated.
        const double dbLow = splSmallSignalDb(m, c, 10.0);
        check(dbLow < m.spl - 20.0, "sealed: strong rolloff below Fc");
    }
    {
        const BoxModel m = makeVented();
        const SplContext c = makeSplContext(m);
        const double db = splSmallSignalDb(m, c, 1000.0);
        check(std::isfinite(db) && std::abs(db - m.spl) < 0.5,
              "vented: small-signal SPL anchored at rated level at 1 kHz");
    }

    // ── Vented: max power finite, limiter identified, monotone ────
    {
        const BoxModel m = makeVented();
        const auto below = maxPowerAt(m, 15.0);   // well below fb: excursion rises
        check(below.pMax > 0.0 && std::isfinite(below.pMax),
              "vented: finite max power below fb");
        check(below.limiter == MaxSplPoint::Xmax || below.limiter == MaxSplPoint::Port,
              "vented: limiter identified");

        BoxModel bigger = m; bigger.xmax_mm = 16.0;
        const auto b2 = maxPowerAt(bigger, 15.0);
        check(b2.pMax >= below.pMax,
              "vented: doubling Xmax never lowers max power");

        const SplContext c = makeSplContext(m);
        const double db = maxSplDb(m, c, 40.0, maxPowerAt(m, 40.0));
        check(std::isfinite(db) && db > m.spl,
              "vented: max SPL above rated 1 W level when P_max > 1 W");
    }

    // ── Gating ────────────────────────────────────────────────────
    {
        BoxModel m = makeSealed();
        check(hasMaxSplData(m), "gating: sealed with Xmax qualifies");
        m.xmax_mm = 0.0;
        check(!hasMaxSplData(m), "gating: missing Xmax disqualifies");
    }

    // ── Thermal (rated RMS power) limit ───────────────────────────
    {
        BoxModel m = makeSealed();
        m.pe_W = 200.0;
        m.numDrivers = 2;
        check(thermalPowerLimit(m) == 400.0,
              "thermal: capacity scales with driver count");
        m.numDrivers = 1;

        const SplContext c = makeSplContext(m);
        const double base = splSmallSignalDb(m, c, 1000.0);
        const double th   = thermalSplDb(m, c, 1000.0);
        check(std::isfinite(th)
              && std::abs(th - (base + 10.0 * std::log10(200.0))) < 1e-9,
              "thermal: SPL ceiling = small-signal + 10*log10(Pe)");

        m.pe_W = 0.0;
        check(thermalPowerLimit(m) == 0.0, "thermal: unknown Pe -> no capacity");
        check(!std::isfinite(thermalSplDb(m, c, 1000.0)),
              "thermal: unknown Pe yields NaN ceiling");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll modeleval tests passed.\n");
    return g_failures;
}
