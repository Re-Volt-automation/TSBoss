#include "paramcheck.h"
#include <cstdio>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// A real, self-consistent measured record (JBL Stage82).
static paramcheck::DriverParams good()
{
    paramcheck::DriverParams p;
    p.fs = 41.6; p.Qms = 5.46; p.Qes = 0.37; p.Qts = 0.35;
    p.Re = 3.6;  p.mms_g = 56.2; p.BL = 11.96;
    p.Sd_cm2 = 208.7; p.Vas_L = 15.9;
    return p;
}

int main()
{
    // ── Self-consistent record: silent ────────────────────────────
    {
        const auto w = paramcheck::driverParamWarnings(good());
        check(w.isEmpty(), "consistent record produces no warnings");
    }

    // ── Placeholder Re (the 0.1 ohm sentinel) ─────────────────────
    {
        auto p = good();
        p.Re = 0.1;
        const auto w = paramcheck::driverParamWarnings(p);
        check(!w.isEmpty(), "placeholder Re is flagged");
        bool low = false, qes = false;
        for (const auto &s : w) {
            if (s.contains("Rₑ")) low = true;
            if (s.contains("Qes"))     qes = true;
        }
        check(low, "low-Re warning present");
        check(qes, "Qes identity mismatch also flagged for bad Re");
    }

    // ── Missing Re (0 = never entered) ────────────────────────────
    {
        auto p = good();
        p.Re = 0.0;
        const auto w = paramcheck::driverParamWarnings(p);
        check(!w.isEmpty(), "missing Re is flagged");
    }

    // ── fs typed 10x too high (the eaten-decimal case) ────────────
    {
        auto p = good();
        p.fs = 416.0;
        const auto w = paramcheck::driverParamWarnings(p);
        bool vas = false;
        for (const auto &s : w) if (s.contains("Vas")) vas = true;
        check(vas, "10x fs breaks the Vas identity and is flagged");
    }

    // ── Qts that contradicts Qms/Qes ──────────────────────────────
    {
        auto p = good();
        p.Qts = 0.9;
        const auto w = paramcheck::driverParamWarnings(p);
        bool qts = false;
        for (const auto &s : w) if (s.contains("Qts")) qts = true;
        check(qts, "contradictory Qts is flagged");
    }

    // ── Sparse datasheet record: no spurious warnings ─────────────
    {
        paramcheck::DriverParams p{};   // all zeros except a few
        p.fs = 41.6; p.Qts = 0.35; p.Vas_L = 15.9; p.Re = 3.6;
        const auto w = paramcheck::driverParamWarnings(p);
        check(w.isEmpty(), "sparse record with missing fields stays silent");
    }

    // ── Tolerance: datasheet-grade rounding stays silent ──────────
    {
        auto p = good();
        p.Qes *= 1.15;   // 15% off — within datasheet slop
        const auto w = paramcheck::driverParamWarnings(p);
        check(w.isEmpty(), "15% deviation is tolerated");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll paramcheck tests passed.\n");
    return g_failures;
}
