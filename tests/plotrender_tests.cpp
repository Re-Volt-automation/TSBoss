// Renders every plot class offscreen with fixed models and verifies the
// output is non-blank. Run with --hashes to print per-plot image hashes —
// used to prove refactors of the paint code are pixel-identical.
// Run with --dump <dir> to also write the rendered PNGs for visual review.
#include "enclosurewidget.h"
#include "boxmodel.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QImage>
#include <QPainter>
#include <cstdio>
#include <cstring>
#include <cmath>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

static BoxModel makeVented()
{
    BoxModel m;
    m.name = "Vented ref";
    m.encType = BoxModel::EncType::Vented;
    m.fs = 30.0; m.Qms = 4.0; m.Qes = 0.4; m.Qts = 0.36;
    m.Re = 6.0;  m.mms_g = 60.0; m.BL = 12.0; m.Sd_cm2 = 220.0;
    m.Vas_L = 80.0;
    m.volumeL = 40.0; m.fb = 32.0; m.QL = 7.0;
    m.portShape = 0; m.portWidth_mm = 75.0; m.numPorts = 1;
    m.portFlare = 0; m.numDrivers = 1;
    m.xmax_mm = 8.0; m.xlim_mm = 12.0;
    // Precomputed reference levels the plots normalize against.
    m.eta = 0.005; m.spl = 89.0; m.f3 = 28.0;
    m.color = QColor("#d97706");
    return m;
}

static BoxModel makeSealed()
{
    BoxModel m = makeVented();
    m.name = "Sealed ref";
    m.encType = BoxModel::EncType::Sealed;
    m.volumeL = 25.0;
    // Plots read the precomputed sealed results; fill the ones they use.
    const double alpha = m.Vas_L / m.volumeL;
    m.alpha = alpha;
    m.Fc    = m.fs  * std::sqrt(alpha + 1.0);
    m.Qtc   = m.Qts * std::sqrt(alpha + 1.0);
    m.f3    = m.Fc;          // close enough for rendering purposes
    m.color = QColor("#2563eb");
    return m;
}

template <typename Plot>
static QImage renderPlot(const char *name, const QList<BoxModel> &models)
{
    Plot p;
    p.setModels(models, 0);
    p.resize(640, 320);
    QImage img(p.size(), QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    p.render(&img);

    char buf[80];
    std::snprintf(buf, sizeof(buf), "%s renders non-blank", name);
    // Non-blank: at least 200 distinct-ish pixels differ from the first pixel.
    const QRgb base = img.pixel(0, 0);
    int diff = 0;
    for (int y = 0; y < img.height() && diff < 200; y += 3)
        for (int x = 0; x < img.width() && diff < 200; x += 3)
            if (img.pixel(x, y) != base) ++diff;
    check(diff >= 200, buf);
    return img;
}

static void printHash(const char *name, const QImage &img)
{
    QCryptographicHash h(QCryptographicHash::Sha1);
    h.addData(QByteArrayView(reinterpret_cast<const char*>(img.constBits()),
                             img.sizeInBytes()));
    std::printf("hash %-18s %s\n", name, h.result().toHex().constData());
}

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    const bool wantHashes = argc > 1 && std::strcmp(argv[1], "--hashes") == 0;
    const QString dumpDir = (argc > 2 && std::strcmp(argv[1], "--dump") == 0)
                            ? QString::fromLocal8Bit(argv[2]) : QString();

    const QList<BoxModel> models = { makeVented(), makeSealed() };

    const QImage spl = renderPlot<ResponsePlot>    ("ResponsePlot",     models);
    const QImage gd  = renderPlot<GroupDelayPlot>  ("GroupDelayPlot",   models);
    const QImage vp  = renderPlot<VoltagePlot>     ("VoltagePlot",      models);
    const QImage exc = renderPlot<ExcursionPlot>   ("ExcursionPlot",    models);
    const QImage pv  = renderPlot<PortVelocityPlot>("PortVelocityPlot", models);
    const QImage mx  = renderPlot<MaxSplPlot>      ("MaxSplPlot",       models);

    // Same models with a 24 dB/oct subsonic filter at 25 Hz engaged —
    // SPL/GD/excursion must still render (and differ from the unfiltered set).
    QList<BoxModel> filtered = models;
    for (auto &m : filtered) { m.hpfOrder = 4; m.hpfFreq = 25.0; }
    const QImage fspl = renderPlot<ResponsePlot> ("ResponsePlot+HPF",  filtered);
    const QImage fgd  = renderPlot<GroupDelayPlot>("GroupDelayPlot+HPF", filtered);
    const QImage fexc = renderPlot<ExcursionPlot>("ExcursionPlot+HPF", filtered);
    check(fspl != spl, "HPF visibly changes the SPL plot");
    check(fexc != exc, "HPF visibly changes the excursion plot");

    if (wantHashes) {
        printHash("ResponsePlot",     spl);
        printHash("GroupDelayPlot",   gd);
        printHash("VoltagePlot",      vp);
        printHash("ExcursionPlot",    exc);
        printHash("PortVelocityPlot", pv);
        printHash("MaxSplPlot",       mx);
    }
    if (!dumpDir.isEmpty()) {
        spl.save(dumpDir + "/ResponsePlot.png");
        gd .save(dumpDir + "/GroupDelayPlot.png");
        vp .save(dumpDir + "/VoltagePlot.png");
        exc.save(dumpDir + "/ExcursionPlot.png");
        pv .save(dumpDir + "/PortVelocityPlot.png");
        mx .save(dumpDir + "/MaxSplPlot.png");
        fspl.save(dumpDir + "/ResponsePlot_HPF.png");
        fgd .save(dumpDir + "/GroupDelayPlot_HPF.png");
        fexc.save(dumpDir + "/ExcursionPlot_HPF.png");
        std::printf("PNGs written to %s\n", qPrintable(dumpDir));
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll plotrender tests passed.\n");
    return g_failures;
}
