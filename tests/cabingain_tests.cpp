#include "cabingain.h"
#include <cstdio>
#include <cmath>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

int main()
{
    using namespace cabingain;

    // ── Correction shape: anchored 0 dB at/above the validity edge ──
    {
        CabinEnv env{23.2, 4.6};
        check(std::fabs(correctionDb(env, kAnchorHz)) < 1e-9,
              "correction is 0 dB at the 80 Hz anchor");
        check(correctionDb(env, 200.0) == 0.0,
              "correction is 0 above the validity band");
        const double g20 = correctionDb(env, 20.0);
        check(g20 > 29.0 && g20 < 32.0,
              "closed-car env gives ~+30 dB at 20 Hz (measured ladder)");
        check(correctionDb(env, 10.0) > 15.0,
              "strong boost persists at 10 Hz");
    }

    // ── Fit recovers known parameters from a synthetic transfer ─────
    {
        CabinEnv truth{27.0, 3.2};
        QVector<double> f, t;
        for (double lf = std::log10(10.0); lf <= std::log10(400.0); lf += 0.01) {
            const double fr = std::pow(10.0, lf);
            // arbitrary level offset + fast log-periodic ripple (modal-ish,
            // several cycles across the fit band so it averages out)
            const double db = -18.0 + shapeDb(truth, fr)
                              + 0.8 * std::sin(20.0 * lf);
            f.append(fr); t.append(db);
        }
        const FitResult r = fitCabin(f, t);
        check(std::fabs(r.env.f0 - truth.f0) < 1.0, "fit recovers f0 within 1 Hz");
        check(std::fabs(r.env.Q  - truth.Q)  < 0.3, "fit recovers Q within 0.3");
        check(r.rms < 0.8, "fit residual below the injected ripple");
    }

    // ── REW export parsing ──────────────────────────────────────────
    {
        const QString text =
            "* Measurement data measured by REW V5.40\n"
            "* Freq(Hz) SPL(dB) Phase(degrees)\n"
            "9.887695 91.959 -21.4769\n"
            "10.253906 93.015 -35.5354\n"
            "\n"
            "10.620117 93.920 -49.3229\n";
        QVector<double> f, s;
        check(parseRewExport(text, f, s), "REW text parses");
        check(f.size() == 3 && s.size() == 3, "three data rows read");
        check(std::fabs(f[1] - 10.253906) < 1e-6 && std::fabs(s[1] - 93.015) < 1e-6,
              "values land in the right columns");
    }

    // ── End-to-end: two synthetic sweeps -> transfer -> fit ─────────
    {
        CabinEnv truth{31.0, 4.0};
        QString nf, seat;
        for (double lf = std::log10(10.0); lf <= std::log10(400.0); lf += 0.005) {
            const double fr = std::pow(10.0, lf);
            const double base = 95.0 - 3.0 * std::pow(std::log10(fr / 10.0), 2.0);
            nf   += QString("%1 %2 0\n").arg(fr, 0, 'f', 4).arg(base, 0, 'f', 3);
            seat += QString("%1 %2 0\n").arg(fr, 0, 'f', 4)
                        .arg(base - 22.0 + shapeDb(truth, fr), 0, 'f', 3);
        }
        const FitResult r = fitFromSweepPair(nf, seat);
        check(r.ok, "sweep-pair pipeline succeeds");
        check(std::fabs(r.env.f0 - truth.f0) < 1.0,
              "pipeline recovers f0 (level/chain cancels)");
        check(std::fabs(r.env.Q - truth.Q) < 0.4, "pipeline recovers Q");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll cabingain tests passed.\n");
    return g_failures;
}
