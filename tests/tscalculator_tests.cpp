#include "tscalculator.h"
#include "driverrecord.h"
#include <QString>
#include <cstdio>
#include <cmath>
#include <algorithm>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}
static bool near(double a, double b, double rel = 1e-6) {
    return std::abs(a - b) <= rel * std::max({std::abs(a), std::abs(b), 1e-12});
}

// Synthetic driver chosen so every intermediate is hand-checkable:
//   Z12 = √(6·54) = 18      R0 = 9        √R0 = 3
//   √(f1·f2) = √(25·36) = 30 = fs         bw = 11
//   Qms = (30/11)·3 = 90/11               Qes = Qms/8 = 90/88
//   Qts = 10/11
//   mms = 0.010 / ((30/25)² − 1) = 0.010/0.44 kg
static DriverRecord makeReference()
{
    DriverRecord r;
    r.make = "Ref"; r.model = "Driver";
    r.Re = 6.0;  r.fs = 30.0; r.Zmax = 54.0;
    r.f1 = 25.0; r.f2 = 36.0;
    r.deltaM = 0.010; r.fo = 25.0;
    r.Dd = 0.2;
    r.Zmin = 6.5; r.f3 = 600.0;
    return r;
}

int main()
{
    constexpr double PI = TSCalculator::PI;

    // ── Full calculation against hand-derived values ──────────────
    {
        DriverRecord r = makeReference();
        QString msg;
        check(TSCalculator::validateInputs(r, msg), "reference inputs validate");

        TSCalculator::calculate(r);
        check(near(r.Z12, 18.0),          "Z12 = sqrt(Re*Zmax)");
        check(near(r.fsVerify, 30.0),     "fsVerify = sqrt(f1*f2)");
        check(near(r.R0, 9.0),            "R0 = Zmax/Re");
        check(near(r.Qms, 90.0/11.0),     "Qms = fs/bw*sqrt(R0)");
        check(near(r.Qes, 90.0/88.0),     "Qes = Qms/(R0-1)");
        check(near(r.Qts, 10.0/11.0),     "Qts = Qms*Qes/(Qms+Qes)");
        check(near(r.mms, 0.010/0.44),    "mms = dm/((fs/fo)^2-1)");

        const double rmsExpect = 2.0*PI*30.0*(0.010/0.44)/(90.0/11.0);  // = pi/6
        check(near(r.Rms, rmsExpect) && near(r.Rms, PI/6.0),
              "Rms = 2*pi*fs*mms/Qms (= pi/6 here)");
        check(near(r.BL, std::sqrt(48.0*PI/6.0)),  "BL = sqrt((Zmax-Re)*Rms)");

        const double cmsExpect = 1.0/(std::pow(2.0*PI*30.0, 2.0)*(0.010/0.44));
        check(near(r.Cms, cmsExpect),     "Cms = 1/((2*pi*fs)^2*mms)");
        check(near(r.Sd, PI*0.01),        "Sd = pi*(Dd/2)^2");

        const double vasExpect = cmsExpect * 1.2 * 344.0*344.0
                               * (PI*0.01) * (PI*0.01);
        check(near(r.Vas, vasExpect),     "Vas = Cms*rho*c^2*Sd^2");

        const double leExpect = ((6.0*20000.0)/(2.0*PI*600.0) + 0.5) * 1e-3/20.0;
        check(near(r.Le, leExpect),       "Le empirical model");
        check(r.hasResults(),             "reference record has results");
    }

    // ── Guard behavior: garbage in, zeros out (never NaN) ─────────
    {
        DriverRecord r;                   // all zeros
        TSCalculator::calculate(r);
        const double outs[] = { r.Z12, r.fsVerify, r.R0, r.Qms, r.Qes, r.Qts,
                                r.mms, r.Rms, r.BL, r.Cms, r.Sd, r.Vas, r.Le };
        bool allZero = true, anyNan = false;
        for (double v : outs) { allZero &= (v == 0.0); anyNan |= std::isnan(v); }
        check(allZero, "all-zero inputs produce all-zero outputs");
        check(!anyNan, "all-zero inputs produce no NaN");
    }
    {
        DriverRecord r = makeReference();
        r.fo = 35.0;                      // fo > fs: added mass cannot raise fs
        TSCalculator::calculate(r);
        check(r.mms == 0.0, "fo > fs leaves mms unset instead of negative");
    }

    // ── validateInputs: every guard fires with a message ──────────
    {
        struct Case { const char *what; void (*mutate)(DriverRecord &); };
        const Case cases[] = {
            { "Re <= 0",       [](DriverRecord &r){ r.Re = 0.0; } },
            { "fs <= 0",       [](DriverRecord &r){ r.fs = 0.0; } },
            { "Zmax <= Re",    [](DriverRecord &r){ r.Zmax = r.Re; } },
            { "f1 <= 0",       [](DriverRecord &r){ r.f1 = 0.0; } },
            { "f2 <= 0",       [](DriverRecord &r){ r.f2 = 0.0; } },
            { "f1 >= f2",      [](DriverRecord &r){ r.f1 = r.f2; } },
            { "fs below f1",   [](DriverRecord &r){ r.fs = 20.0; } },
            { "fs above f2",   [](DriverRecord &r){ r.fs = 40.0; } },
            { "deltaM <= 0",   [](DriverRecord &r){ r.deltaM = 0.0; } },
            { "fo <= 0",       [](DriverRecord &r){ r.fo = 0.0; } },
            { "fo >= fs",      [](DriverRecord &r){ r.fo = r.fs; } },
            { "Dd <= 0",       [](DriverRecord &r){ r.Dd = 0.0; } },
            { "Zmin <= 0",     [](DriverRecord &r){ r.Zmin = 0.0; } },
            { "f3 <= 0",       [](DriverRecord &r){ r.f3 = 0.0; } },
            { "f3 <= fs",      [](DriverRecord &r){ r.f3 = r.fs; } },
        };
        for (const auto &c : cases) {
            DriverRecord r = makeReference();
            c.mutate(r);
            QString msg;
            char buf[80];
            std::snprintf(buf, sizeof(buf), "validate rejects %s", c.what);
            check(!TSCalculator::validateInputs(r, msg) && !msg.isEmpty(), buf);
        }
    }

    // ── driverVoltage: series divider ─────────────────────────────
    check(near(TSCalculator::driverVoltage(2.0, 50.0, 50.0), 1.0),
          "driverVoltage: equal R halves the source");
    check(TSCalculator::driverVoltage(2.0, 0.0, 0.0) == 0.0,
          "driverVoltage: degenerate circuit returns 0");
    check(near(TSCalculator::driverVoltage(1.0, 0.0, 8.0), 1.0),
          "driverVoltage: no series R passes source through");

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll tscalculator tests passed.\n");
    return g_failures;
}
