#include "filters.h"
#include <cstdio>
#include <cmath>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

static double magDb(int order, double fc, double f)
{
    return 20.0 * std::log10(std::abs(filt::butterworthHP(order, fc, f)));
}

int main()
{
    constexpr double FC = 25.0;

    // ── Butterworth defining property: |H| = x^n / sqrt(1 + x^2n) ──
    for (int order : {2, 4}) {
        bool allMatch = true;
        for (double x : {0.25, 0.5, 1.0, 2.0, 4.0}) {
            const double expect = 20.0 * std::log10(
                std::pow(x, order) / std::sqrt(1.0 + std::pow(x, 2.0 * order)));
            if (std::abs(magDb(order, FC, x * FC) - expect) > 0.01) allMatch = false;
        }
        char buf[80];
        std::snprintf(buf, sizeof(buf),
                      "order %d magnitude is maximally flat Butterworth", order);
        check(allMatch, buf);
    }

    // ── −3 dB exactly at the corner ───────────────────────────────
    check(std::abs(magDb(2, FC, FC) + 3.0103) < 0.01, "order 2: -3 dB at fc");
    check(std::abs(magDb(4, FC, FC) + 3.0103) < 0.01, "order 4: -3 dB at fc");

    // ── Passband flat, stopband slopes 12 / 24 dB per octave ──────
    check(std::abs(magDb(2, FC, 20.0 * FC)) < 0.05, "order 2: flat well above fc");
    check(std::abs(magDb(4, FC, 20.0 * FC)) < 0.05, "order 4: flat well above fc");
    const double slope2 = magDb(2, FC, FC / 4.0) - magDb(2, FC, FC / 8.0);
    const double slope4 = magDb(4, FC, FC / 4.0) - magDb(4, FC, FC / 8.0);
    check(std::abs(slope2 - 12.0) < 0.5, "order 2: 12 dB/oct in the stopband");
    check(std::abs(slope4 - 24.0) < 0.5, "order 4: 24 dB/oct in the stopband");

    // ── Group delay: positive near fc, vanishing far above ────────
    check(filt::groupDelayMs(2, FC, FC) > 0.0,        "order 2: positive GD at fc");
    check(filt::groupDelayMs(4, FC, FC) > filt::groupDelayMs(2, FC, FC),
          "order 4 delays more than order 2 at fc");
    check(filt::groupDelayMs(4, FC, 40.0 * FC) < 0.2, "GD vanishes far above fc");

    // ── Degenerate inputs are bypass ──────────────────────────────
    check(std::abs(filt::butterworthHP(0, FC, 30.0) - std::complex<double>(1.0, 0.0)) < 1e-12,
          "order 0 is unity");
    check(std::abs(filt::butterworthHP(2, 0.0, 30.0) - std::complex<double>(1.0, 0.0)) < 1e-12,
          "fc <= 0 is unity");

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll filters tests passed.\n");
    return g_failures;
}
