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

    // ── Correction: measured gain + shape, tapered out by 160 Hz ────
    {
        CabinEnv env{23.2, 4.6, 11.5};   // the measured closed-car state
        check(std::fabs(correctionDb(env, 20.0) - 21.4) < 0.3,
              "closed car: ~+21 dB at 20 Hz vs midband (measured)");
        check(std::fabs(correctionDb(env, 12.0)
                        - (env.gain + shapeDb(env, 12.0))) < 1e-9,
              "below the fit edge: correction = gain + shape");
        check(correctionDb(env, 200.0) == 0.0,
              "correction is 0 above the taper");
        const double cEdge = env.gain + shapeDb(env, kFitEdgeHz);
        check(std::fabs(correctionDb(env, kFitEdgeHz) - cEdge) < 1e-9,
              "continuous at the fit edge");
        const double mid = std::sqrt(kFitEdgeHz * kTaperEndHz);
        check(std::fabs(correctionDb(env, mid) - 0.5 * cEdge)
              < 0.15 * std::fabs(cEdge) + 1e-9,
              "taper decays smoothly through 80-160 Hz");
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
    // Realistic cabin: gain+shape below the fit edge, flat modal midband
    // above 150 Hz, log-blended between; arbitrary -22 dB geometric level.
    {
        CabinEnv truth{31.0, 4.0, 9.0};
        auto cabinAdd = [&](double fr) {
            const double below = truth.gain + shapeDb(truth, fr);
            if (fr <= 80.0)  return below;
            if (fr >= 150.0) return 0.0;
            const double edge = truth.gain + shapeDb(truth, 80.0);
            const double t = std::log(150.0 / fr) / std::log(150.0 / 80.0);
            return edge * t;
        };
        QString nf, seat;
        for (double lf = std::log10(10.0); lf <= std::log10(400.0); lf += 0.005) {
            const double fr = std::pow(10.0, lf);
            const double base = 95.0 - 3.0 * std::pow(std::log10(fr / 10.0), 2.0);
            nf   += QString("%1 %2 0\n").arg(fr, 0, 'f', 4).arg(base, 0, 'f', 3);
            seat += QString("%1 %2 0\n").arg(fr, 0, 'f', 4)
                        .arg(base - 22.0 + cabinAdd(fr), 0, 'f', 3);
        }
        const FitResult r = fitFromSweepPair(nf, seat);
        check(r.ok, "sweep-pair pipeline succeeds");
        check(std::fabs(r.env.f0 - truth.f0) < 1.0,
              "pipeline recovers f0 (level/chain cancels)");
        check(std::fabs(r.env.Q - truth.Q) < 0.4, "pipeline recovers Q");
        check(std::fabs(r.env.gain - truth.gain) < 0.8,
              "pipeline recovers the gain vs the midband baseline");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll cabingain tests passed.\n");
    return g_failures;
}
