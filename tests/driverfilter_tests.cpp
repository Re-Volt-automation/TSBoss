#include "driverfilter.h"
#include <cstdio>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

static DriverRecord makeDriver()
{
    DriverRecord r;
    r.make = "Acme"; r.model = "W8";
    r.fs = 32.0; r.Qts = 0.42; r.Vas = 0.065 /* m³ = 65 L */; r.Re = 3.4;
    return r;
}

int main()
{
    const DriverRecord d = makeDriver();

    {   // No bounds: everything passes, filter reports inactive
        DriverFilter f;
        check(!f.isActive(),  "empty filter is inactive");
        check(f.matches(d),   "empty filter matches everything");
    }
    {   // In-range bounds pass
        DriverFilter f;
        f.fsMin = 25.0; f.fsMax = 40.0;
        f.qtsMin = 0.3; f.qtsMax = 0.5;
        f.vasLMin = 50.0; f.vasLMax = 80.0;
        f.reMin = 3.0; f.reMax = 4.0;
        check(f.isActive(),  "bounded filter is active");
        check(f.matches(d),  "record inside all ranges matches");
    }
    {   // Each bound individually rejects
        DriverFilter f; f.fsMin = 35.0;
        check(!f.matches(d), "fs below min rejected");
        f = {}; f.fsMax = 30.0;
        check(!f.matches(d), "fs above max rejected");
        f = {}; f.qtsMin = 0.5;
        check(!f.matches(d), "Qts below min rejected");
        f = {}; f.qtsMax = 0.4;
        check(!f.matches(d), "Qts above max rejected");
        f = {}; f.vasLMin = 70.0;
        check(!f.matches(d), "Vas below min rejected (litre conversion)");
        f = {}; f.vasLMax = 60.0;
        check(!f.matches(d), "Vas above max rejected (litre conversion)");
        f = {}; f.reMin = 4.0;
        check(!f.matches(d), "Re below min rejected");
        f = {}; f.reMax = 3.0;
        check(!f.matches(d), "Re above max rejected");
    }
    {   // One-sided bounds work alone
        DriverFilter f; f.fsMax = 40.0;
        check(f.matches(d),  "single upper bound passes in-range record");
    }
    {   // Records with the parameter missing (0) are not excluded by bounds
        DriverRecord blank = makeDriver();
        blank.Qts = 0.0;                  // datasheet row without measured Qts
        DriverFilter f; f.qtsMin = 0.3;
        check(!f.matches(blank), "missing parameter fails a lower bound");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll driverfilter tests passed.\n");
    return g_failures;
}
