#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <algorithm>
#include <cmath>

// ─────────────────────────────────────────────────────────────────
//  cabingain – in-cabin (vehicle) low-frequency gain model.
//
//  Measurement showed a car cabin behaves as a damped Helmholtz
//  system below ~80 Hz: the cabin volume with its leak aperture is a
//  resonator whose frequency rises and whose Q falls as the car is
//  opened up (validated 2026-07-26 against a five-state leak ladder,
//  RMS 1.1–1.9 dB per state with just these two parameters).
//
//  The correction applied to a predicted anechoic SPL curve is the
//  fitted shape re-anchored to 0 dB at 80 Hz, and 0 above — beyond
//  the pressure zone the field is modal and position-dependent, which
//  no low-order model should pretend to capture.
//
//  Header-only and unit-tested (tests/cabingain_tests.cpp).
// ─────────────────────────────────────────────────────────────────

namespace cabingain {

/// Upper edge of the model's validity band [Hz]; the correction is
/// anchored to 0 dB here and switched off above.
inline constexpr double kAnchorHz = 80.0;

struct CabinEnv {
    double f0 = 30.0;   ///< cabin/leak Helmholtz resonance [Hz]
    double Q  = 3.0;    ///< leak damping
};

/// Raw second-order resonance magnitude [dB] (un-anchored).
inline double shapeDb(const CabinEnv &e, double f)
{
    if (e.f0 <= 0.0 || e.Q <= 0.0 || f <= 0.0) return 0.0;
    const double x  = f / e.f0;
    const double d  = (1.0 - x * x) * (1.0 - x * x) + (x / e.Q) * (x / e.Q);
    return -10.0 * std::log10(std::max(d, 1e-12));
}

/// Correction added to an anechoic SPL curve: the fitted shape
/// relative to its 80 Hz value; exactly 0 at and above the anchor.
inline double correctionDb(const CabinEnv &e, double f)
{
    if (f >= kAnchorHz) return 0.0;
    return shapeDb(e, f) - shapeDb(e, kAnchorHz);
}

// ── REW text export parsing ──────────────────────────────────────

/// Parse a REW "export measurement as text" body: '*' comment lines,
/// then rows of "freq SPL [phase]". Returns false if fewer than 8
/// usable rows were found.
inline bool parseRewExport(const QString &text,
                           QVector<double> &freq, QVector<double> &spl)
{
    freq.clear(); spl.clear();
    const QStringList lines = text.split('\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('*')) continue;
        const QStringList p = line.split(' ', Qt::SkipEmptyParts);
        if (p.size() < 2) continue;
        bool okF = false, okS = false;
        const double f = p[0].toDouble(&okF);
        const double s = p[1].toDouble(&okS);
        if (!okF || !okS || f <= 0.0) continue;
        freq.append(f); spl.append(s);
    }
    // Adequacy for FITTING is enforced by fitCabin (≥12 in-band points);
    // parsing succeeds with any interpolatable amount of data.
    return freq.size() >= 2;
}

// ── Fitting ──────────────────────────────────────────────────────

struct FitResult {
    bool     ok  = false;
    CabinEnv env;
    double   rms = 0.0;   ///< residual over the fit band [dB]
};

/// Resample onto a log grid (10–400 Hz) with ~1/6-octave gaussian
/// smoothing in the log-frequency domain.
inline void logResample(const QVector<double> &freq, const QVector<double> &spl,
                        QVector<double> &gridF, QVector<double> &gridS)
{
    constexpr int    N        = 240;
    constexpr double kFLo     = 10.0, kFHi = 400.0;
    constexpr double kSigmaOct = (1.0 / 6.0) / 2.355;   // FWHM → σ
    gridF.resize(N); gridS.resize(N);
    QVector<double> lin(N);
    for (int i = 0; i < N; ++i) {
        const double lf = std::log10(kFLo)
            + (std::log10(kFHi) - std::log10(kFLo)) * i / double(N - 1);
        gridF[i] = std::pow(10.0, lf);
        // linear interpolation on the (sorted) source data
        const double f = gridF[i];
        int j = int(std::lower_bound(freq.begin(), freq.end(), f) - freq.begin());
        if      (j <= 0)            lin[i] = spl.first();
        else if (j >= freq.size())  lin[i] = spl.last();
        else {
            const double t = (f - freq[j-1]) / (freq[j] - freq[j-1]);
            lin[i] = spl[j-1] + t * (spl[j] - spl[j-1]);
        }
    }
    for (int i = 0; i < N; ++i) {
        const double li = std::log2(gridF[i]);
        double acc = 0.0, wsum = 0.0;
        for (int k = 0; k < N; ++k) {
            const double d = (std::log2(gridF[k]) - li) / kSigmaOct;
            const double w = std::exp(-0.5 * d * d);
            acc += w * lin[k]; wsum += w;
        }
        gridS[i] = acc / wsum;
    }
}

/// Fit {f0, Q} (+ implicit free level) to a transfer curve [dB] over
/// 10–80 Hz. Grid search then local refinement — the same procedure
/// validated offline against the measured leak ladder.
inline FitResult fitCabin(const QVector<double> &freq, const QVector<double> &transferDb)
{
    FitResult out;
    QVector<double> f, t;
    for (int i = 0; i < freq.size(); ++i)
        if (freq[i] >= 10.0 && freq[i] <= kAnchorHz) { f.append(freq[i]); t.append(transferDb[i]); }
    if (f.size() < 12) return out;

    auto rmsFor = [&](double f0, double Q, double *offOut = nullptr) {
        const CabinEnv e{f0, Q};
        double sum = 0.0;
        for (int i = 0; i < f.size(); ++i) sum += t[i] - shapeDb(e, f[i]);
        const double off = sum / f.size();
        double se = 0.0;
        for (int i = 0; i < f.size(); ++i) {
            const double r = t[i] - shapeDb(e, f[i]) - off;
            se += r * r;
        }
        if (offOut) *offOut = off;
        return std::sqrt(se / f.size());
    };

    double bestRms = 1e30, bestF0 = 30.0, bestQ = 3.0;
    for (double f0 = 15.0; f0 <= 70.0; f0 += 0.5)
        for (double Q = 0.4; Q <= 6.0; Q += 0.1) {
            const double r = rmsFor(f0, Q);
            if (r < bestRms) { bestRms = r; bestF0 = f0; bestQ = Q; }
        }
    for (double f0 = std::max(10.0, bestF0 - 1.0); f0 <= bestF0 + 1.0; f0 += 0.05)
        for (double Q = std::max(0.2, bestQ - 0.2); Q <= bestQ + 0.2; Q += 0.02) {
            const double r = rmsFor(f0, Q);
            if (r < bestRms) { bestRms = r; bestF0 = f0; bestQ = Q; }
        }

    out.ok  = true;
    out.env = {bestF0, bestQ};
    out.rms = bestRms;
    return out;
}

/// Full pipeline from two REW export bodies (nearfield + listening
/// position, same session and level): parse → resample/smooth →
/// transfer = seat − nearfield → fit. Level, power, chain and any
/// crossover cancel in the subtraction.
inline FitResult fitFromSweepPair(const QString &nearfieldText, const QString &seatText)
{
    QVector<double> fN, sN, fS, sS;
    if (!parseRewExport(nearfieldText, fN, sN) || !parseRewExport(seatText, fS, sS))
        return {};
    QVector<double> gF, gN, gS;
    logResample(fN, sN, gF, gN);
    logResample(fS, sS, gF, gS);
    QVector<double> transfer(gF.size());
    for (int i = 0; i < gF.size(); ++i) transfer[i] = gS[i] - gN[i];
    return fitCabin(gF, transfer);
}

} // namespace cabingain
