#include "portphysics.h"
#include "diagrams/portdiagram.h"
#include <cstdio>
#include <cmath>
#include <vector>

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

    return g_failures == 0 ? 0 : 1;
}
