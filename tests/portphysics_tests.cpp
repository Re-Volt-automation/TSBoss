#include "portphysics.h"
#include "bandpassphysics.h"
#include "diagrams/portdiagram.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <string>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// A known, fully-populated vented model used by every test.
static BoxModel makeModel() {
    BoxModel m;
    m.encType = BoxModel::EncType::Vented;
    m.fs = 30.0; m.Qms = 4.0; m.Qes = 0.4; m.Qts = 0.36;
    m.Re = 6.0;  m.mms_g = 60.0; m.BL = 12.0; m.Sd_cm2 = 220.0;
    m.volumeL = 40.0; m.fb = 32.0; m.QL = 7.0;
    m.portShape = 0; m.portWidth_mm = 75.0; m.numPorts = 1;
    m.portFlare = 0; m.numDrivers = 1;
    m.wiringMode = BoxModel::WiringMode::Series;
    return m;
}

static const std::vector<double> kFreqs = {20, 25, 32, 40, 60, 100, 1000};

static BoxModel makeBP4() {
    BoxModel m = makeModel();
    m.encType = BoxModel::EncType::Bandpass4;
    m.volumeL = 30.0;                                   // rear sealed
    m.volumeFront_L = 40.0; m.fbFront = 60.0; m.QLFront = 7.0;
    m.portFrontShape = 0; m.portFrontWidth_mm = 100.0; m.numPortsFront = 1; m.portFrontFlare = 0;
    return m;
}
static BoxModel makeBP6() {
    BoxModel m = makeBP4();
    m.encType = BoxModel::EncType::Bandpass6;
    m.fb = 35.0; m.QL = 7.0;                            // rear vented
    m.portShape = 0; m.portWidth_mm = 80.0; m.numPorts = 1; m.portFlare = 0;
    return m;
}
static const std::vector<double> kBPFreqs = {25, 40, 60, 80, 120};

int main() {
    using namespace pp;
    BoxModel m = makeModel();

    // spl = portedSplRaw = raw far-field proxy ω·|Ua| (linear, NOT dB); Z = |Zin| [ohm].
    struct Golden { double f, spl, Z; };
    const std::vector<Golden> golden = {
        {   20, 0.1196436595, 29.32942691 },
        {   25, 0.2944007142, 14.43456184 },
        {   32, 0.7195503445,  8.273506682},
        {   40, 0.9105795037, 15.00972248 },
        {   60, 0.7963812178, 15.78208455 },
        {  100, 0.7473100746,  7.876039923},
        { 1000, 0.7334199631,  6.015052579},
    };
    // Golden now evaluated at the linear limit (power -> 0):
    for (auto &g : golden) {
        const double spl = portedSplRaw(m, g.f, 1e-12);
        const double Z   = portedImpedance(m, g.f, 1e-12);
        char msgSpl[64], msgZ[64];
        std::snprintf(msgSpl, sizeof(msgSpl), "golden SPL @ %.0f Hz (power->0)", g.f);
        std::snprintf(msgZ,   sizeof(msgZ),   "golden Z   @ %.0f Hz (power->0)", g.f);
        check(std::abs(spl - g.spl) <= 1e-6 * std::max(std::abs(g.spl), 1e-12), msgSpl);
        check(std::abs(Z   - g.Z)   <= 1e-6 * std::max(std::abs(g.Z),   1e-12), msgZ);
    }
    // Convergence: solve returns finite u across the sweep at high power.
    for (double f : kFreqs) {
        const double u = portAirVelocity(m, f, 1000.0);
        check(std::isfinite(u) && u >= 0.0, "portedSolve converges to finite u");
    }

    // Small-port / high-power convergence: the returned u must actually satisfy the
    // fixed-point equation u = |port(Rp(u))|·sqrt(power·Zin)/(N·Ap), not just be the
    // arbitrary value left after the iteration cap. Verify the residual is small.
    {
        BoxModel sp = makeModel();
        sp.portWidth_mm = 20.0;          // small round port — provokes divergence
        const double power = 1000.0, f = sp.fb;
        const auto   s  = pp::portedSolve(sp, f, power);
        const double Ap = std::max(pp::portArea_m2(sp), 1e-9);
        const int    N  = std::max(1, sp.numPorts);
        const auto   c  = pp::portCore(sp, f, pp::portZ(sp, f, s.u));
        const double V  = std::sqrt(power * c.Zin);
        const double u_check = std::abs(c.port) * V / (N * Ap);
        check(std::isfinite(s.u) && s.u >= 0.0, "small-port solve returns finite u");
        check(std::abs(u_check - s.u) <= 1e-2 * std::max(s.u, 1.0),
              "small-port high-power solve converges (fixed-point residual small)");
    }

    // Compression: at high power the port-region output gains less than the
    // small-signal case (normalised to 1 kHz, where the cone dominates).
    {
        auto gainAtFb = [&](double power) {
            const double ref = portedSplRaw(m, 1000.0, power);
            return 20.0 * std::log10(portedSplRaw(m, m.fb, power) / ref);
        };
        const double lowP  = gainAtFb(1e-3);
        const double highP = gainAtFb(2000.0);
        check(highP <= lowP + 1e-6, "port-region gain compresses at high power");
        check(lowP - highP > 0.1,   "compression is non-trivial (>0.1 dB)");
    }
    // Saddle fill: the impedance minimum between the two ported peaks rises with power.
    {
        auto saddleZ = [&](double power) {
            double zmin = 1e9;
            for (double f = m.fb*0.7; f <= m.fb*1.4; f += 0.5)
                zmin = std::min(zmin, portedImpedance(m, f, power));
            return zmin;
        };
        check(saddleZ(2000.0) >= saddleZ(1e-3) - 1e-6, "impedance saddle fills with power");
    }
    // Flare ranking: straight ports lose more (lower gain at fb) than flared.
    {
        BoxModel straight = m; straight.portFlare = 0;
        BoxModel flared   = m; flared.portFlare   = 2;
        auto relGain = [](const BoxModel &mm, double power) {
            return 20.0*std::log10(pp::portedSplRaw(mm, mm.fb, power)
                                 / pp::portedSplRaw(mm, 1000.0, power));
        };
        check(relGain(flared, 2000.0) >= relGain(straight, 2000.0) - 1e-6,
              "flared port compresses less than straight");
    }
    // Chuffing limit rises with flare (sharp 17 < one-end 22 < both-ends 28).
    {
        check(chuffLimit(0) == 17.0, "chuffLimit(straight) == 17 m/s");
        check(chuffLimit(1) == 22.0, "chuffLimit(one-end flared) == 22 m/s");
        check(chuffLimit(2) == 28.0, "chuffLimit(both-ends flared) == 28 m/s");
        check(chuffLimit(2) > chuffLimit(0), "flared chuffing limit exceeds sharp");
    }

    // Port-diagram pure helpers.
    {
        check(!portFlareOuter(0) && portFlareOuter(1) && portFlareOuter(2),
              "portFlareOuter: flares for one-end and both-ends");
        check(!portFlareInner(0) && !portFlareInner(1) && portFlareInner(2),
              "portFlareInner: inner flares only for both-ends");
        BoxModel rnd; rnd.portShape = 0; rnd.portWidth_mm = 75.0;
        check(portFaceLabel(rnd) == "Ø 75 mm", "round face label");
        BoxModel rect; rect.portShape = 1; rect.portWidth_mm = 80.0; rect.portHeight_mm = 50.0;
        check(portFaceLabel(rect) == "80 × 50 mm", "rect face label");
    }

    // Bandpass diagram helpers.
    {
        BoxModel bp4; bp4.encType = BoxModel::EncType::Bandpass4;
        BoxModel bp6; bp6.encType = BoxModel::EncType::Bandpass6;
        BoxModel v;   v.encType   = BoxModel::EncType::Vented;
        check(bandpassRearSealed(bp4),  "BP4 rear is sealed");
        check(!bandpassRearSealed(bp6), "BP6 rear is vented");
        check(!bandpassRearSealed(v),   "vented is not bandpass-sealed");
        BoxModel fr;  fr.portFrontShape = 0;  fr.portFrontWidth_mm = 100.0;
        check(portFaceLabelFront(fr) == "Ø 100 mm", "front round face label");
        BoxModel frr; frr.portFrontShape = 1; frr.portFrontWidth_mm = 90.0; frr.portFrontHeight_mm = 40.0;
        check(portFaceLabelFront(frr) == "90 × 40 mm", "front rect face label");
    }

    // portFrontArea_m2: front-port area from the portFront* fields.
    {
        BoxModel fr;  fr.portFrontShape = 0; fr.portFrontWidth_mm = 100.0;   // round Ø100mm
        const double aRound = pp::PI * 0.05 * 0.05;                          // π r², r=0.05 m
        check(std::abs(pp::portFrontArea_m2(fr) - aRound) <= 1e-12, "front round area = πr²");
        BoxModel frr; frr.portFrontShape = 1; frr.portFrontWidth_mm = 80.0; frr.portFrontHeight_mm = 50.0;
        check(std::abs(pp::portFrontArea_m2(frr) - (0.08 * 0.05)) <= 1e-12, "front rect area = w·h");
    }

    // Bandpass golden: power=0 (linear limit) must equal the pre-rewrite linear model.
    struct BPGolden { const char *kind; double f, spl, Z; };
    const std::vector<BPGolden> bpGolden = {
        {"BP4", 25, /*spl*/0.2581972151, /*Z*/11.45103385}, {"BP4", 40, 0.6766645385, 31.47770956}, {"BP4", 60, 1.965624925, 10.03944402}, {"BP4", 80, 0.8888582925, 13.01865426}, {"BP4", 120, 0.243480655, 7.441158685},
        {"BP6", 25, /*spl*/0.1579857001, /*Z*/13.88499792}, {"BP6", 40, 1.06044503, 10.29991499}, {"BP6", 60, 1.948257759, 10.07235777}, {"BP6", 80, 0.7327091872, 13.82830906}, {"BP6", 120, 0.1762961827, 7.46249356},
    };
    for (auto &g : bpGolden) {
        BoxModel bm = (std::string(g.kind) == "BP6") ? makeBP6() : makeBP4();
        const double spl = pp::bandpassSplRaw(bm, g.f, 0.0);     // power=0 -> linear
        const double Z   = pp::bandpassImpedance(bm, g.f, 0.0);
        char ms[80], mz[80];
        std::snprintf(ms, sizeof ms, "%s golden SPL @ %.0f Hz (power=0==linear)", g.kind, g.f);
        std::snprintf(mz, sizeof mz, "%s golden Z   @ %.0f Hz (power=0==linear)", g.kind, g.f);
        check(std::abs(spl - g.spl) <= 1e-6 * std::max(std::abs(g.spl), 1e-12), ms);
        check(std::abs(Z   - g.Z)   <= 1e-6 * std::max(std::abs(g.Z),   1e-12), mz);
    }

    // Velocity-aware bandpass: power-aware fixed-point convergence + flare ranking.
    {
        BoxModel bp6 = makeBP6();
        for (double f : kBPFreqs) {
            const auto s = pp::bandpassSolve(bp6, f, 500.0);
            check(std::isfinite(s.uFront) && s.uFront >= 0.0, "BP6 uFront finite");
            check(std::isfinite(s.uRear)  && s.uRear  >= 0.0, "BP6 uRear finite");
        }
        // BP4 rear chamber is sealed -> no rear port -> uRear == 0.
        BoxModel bp4 = makeBP4();
        const auto s4 = pp::bandpassSolve(bp4, bp4.fbFront, 500.0);
        check(s4.uRear == 0.0, "BP4 uRear == 0 (rear sealed)");
        check(std::isfinite(s4.uFront) && s4.uFront > 0.0, "BP4 uFront > 0");
        // power=0 -> velocities zero (linear limit, no drive).
        const auto s0 = pp::bandpassSolve(bp6, bp6.fbFront, 0.0);
        check(s0.uFront == 0.0 && s0.uRear == 0.0, "BP6 power=0 -> zero velocity");
        // Flare ranking: flared front port has lower turbulent resistance -> lower port
        // impedance -> higher port flow -> higher velocity than sharp at the same power.
        BoxModel sharp = makeBP6();  sharp.portFrontFlare = 0;
        BoxModel flared = makeBP6(); flared.portFrontFlare = 2;
        const double us = pp::bandpassSolve(sharp,  sharp.fbFront,  2000.0).uFront;
        const double uf = pp::bandpassSolve(flared, flared.fbFront, 2000.0).uFront;
        check(uf >= us - 1e-9, "flared front port velocity >= sharp at high power");
    }

    return g_failures == 0 ? 0 : 1;
}
