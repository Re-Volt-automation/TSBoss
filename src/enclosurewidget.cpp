#include "enclosurewidget.h"
#include "portphysics.h"
#include "tscalculator.h"
#include "noscrollspinbox.h"
#include "driverdetailwidget.h"
#include "theme.h"
#include "pdfreport.h"
#include <complex>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QSplitter>
#include <QFrame>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QColor>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QFileDialog>
#include <QColorDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QPixmap>
#include <QEvent>
#include <QLineEdit>
#include <QTabWidget>
#include <QRegularExpression>
#include <QButtonGroup>
#include <QCheckBox>
#include <QRadioButton>
#include <QSpinBox>
#include <QSlider>
#include <QMouseEvent>
#include <cmath>

using namespace pp;

static constexpr double SPL_REF_DB = 112.02;

static constexpr double F_MIN = 10.0;
static constexpr double F_MAX = 1000.0;

// ─────────────────────────────────────────────────────────────────
//  Enclosure-type helpers — replace the old isVented / isInfiniteBaffle
//  bool pair.  isPorted = "uses port acoustic math" (Vented or BP).
// ─────────────────────────────────────────────────────────────────
static inline bool isVented   (const BoxModel &m) { return m.encType == BoxModel::EncType::Vented;    }
static inline bool isIB       (const BoxModel &m) { return m.encType == BoxModel::EncType::IB;        }
static inline bool isBP4      (const BoxModel &m) { return m.encType == BoxModel::EncType::Bandpass4; }
static inline bool isBP6      (const BoxModel &m) { return m.encType == BoxModel::EncType::Bandpass6; }
static inline bool isBandpass (const BoxModel &m) { return isBP4(m) || isBP6(m); }
static inline bool isPorted   (const BoxModel &m) { return isVented(m) || isBandpass(m); }
static inline bool isSealed   (const BoxModel &m) { return m.encType == BoxModel::EncType::Sealed;    }

// ─────────────────────────────────────────────────────────────────
//  Plot palette — dark instrumentation theme.
//  Outer page sits on QMainWindow bg (#0E1116). Plot canvas is a
//  slightly lifted panel. Axis lines & titles use cream; tick
//  labels use a muted parchment; gridlines are near-invisible at
//  rest and only assert themselves with the curves on top.
// ─────────────────────────────────────────────────────────────────
// Plot colour accessors — read live from the global Theme so a theme
// switch immediately changes the canvas without restarting.
static inline QColor CLR_PAGE_BG() { return Theme::instance().pageBg(); }
static inline QColor CLR_PLOT_BG() { return Theme::instance().plotBg(); }
static inline QColor CLR_ACCENT()  { return Theme::instance().accent(); }
static inline QColor CLR_GREY_DK() { return Theme::instance().plotAxis(); }
static inline QColor CLR_GREY()    { return Theme::instance().plotTickLabel(); }
static inline QColor CLR_GREY_LT() { return Theme::instance().plotDimLine(); }
static inline QColor CLR_GRID()    { return Theme::instance().gridLine(); }

// Curve palette delegates to Theme so light mode gets paper-friendly hues.
static QColor paletteColor(int i) {
    const auto pal = Theme::instance().curvePalette();
    return pal.isEmpty() ? QColor("#D97706") : pal[i % pal.size()];
}
static int paletteSize() {
    const int n = Theme::instance().curvePalette().size();
    return n > 0 ? n : 1;
}

// ─────────────────────────────────────────────────────────────────
//  Added-mass transform — bake an addedMass_g value into the bare
//  T/S so the rest of the simulation can stay mass-agnostic.
//  Cms / Re / BL / Sd / Vas are unchanged.  Idempotent: returns a
//  copy with addedMass_g cleared so applying twice is a no-op.
// ─────────────────────────────────────────────────────────────────
static BoxModel withEffectiveMass(BoxModel m)
{
    if (m.addedMass_g <= 0.0 || m.mms_g <= 0.0) {
        m.addedMass_g = 0.0;
        return m;
    }
    const double r    = (m.mms_g + m.addedMass_g) / m.mms_g;   // > 1
    const double sr   = std::sqrt(r);
    if (m.fs  > 0) m.fs  /= sr;
    if (m.Qms > 0) m.Qms *= sr;
    if (m.Qes > 0) m.Qes *= sr;
    m.mms_g += m.addedMass_g;
    if (m.Qms > 0 && m.Qes > 0)
        m.Qts = (m.Qms * m.Qes) / (m.Qms + m.Qes);
    m.addedMass_g = 0.0;
    return m;
}

// ─────────────────────────────────────────────────────────────────
//  Ported-box numerical simulation helpers
// ─────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────
//  Bandpass numerical simulation helpers
//  BP4: sealed REAR (Vr) + vented FRONT (Vf, fb_f).  Output = front port.
//  BP6: vented REAR (Vr, fb_r) + vented FRONT (Vf, fb_f).  Output = rear+front ports.
//
//  Acoustic load on the cone Za = Zrear + Zfront (cone has air on both
//  faces; loads add).  For each chamber the load is the chamber
//  compliance in parallel with its port (or just the compliance for a
//  sealed chamber).  Front-port volume velocity Up_f = (Sd·v · Za_f) / Zport_f.
// ─────────────────────────────────────────────────────────────────
static bool hasBP4Data(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0
        && m.volumeL > 0                         // rear sealed
        && m.volumeFront_L > 0 && m.fbFront > 0  // front vented
        && m.QLFront > 0;
}

static bool hasBP6Data(const BoxModel &m)
{
    return m.fs > 0 && m.Qms > 0 && m.Re > 0 && m.mms_g > 0
        && m.BL > 0 && m.Sd_cm2 > 0
        && m.volumeL > 0 && m.fb > 0 && m.QL > 0  // rear vented
        && m.volumeFront_L > 0 && m.fbFront > 0 && m.QLFront > 0;
}

struct BPAmps { Cpx cone, rearPort, frontPort; };

// Common per-chamber acoustic impedance: parallel of compliance and (optional) port mass+leakage.
// For a sealed chamber pass fb=0 and the result is just the compliance.
static Cpx chamberAcousticZ(double Vb_L, double fb, double QL_, double omega)
{
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    if (fb <= 0.0) return Zcab;                // sealed
    const double omegab = 2.0 * PI * fb;
    const double Map    = 1.0 / (omegab*omegab*Cab);
    const double Rp     = omegab * Map / std::max(QL_, 0.1);
    const Cpx Zport(Rp, omega*Map);
    return Zcab * Zport / (Zcab + Zport);
}

// Pull both port volume velocities (zero for a sealed chamber).
static Cpx chamberPortFlow(const Cpx &Sdv, double Vb_L, double fb, double QL_, double omega)
{
    if (fb <= 0.0 || Vb_L <= 0.0) return Cpx(0.0, 0.0);
    const double Vb  = Vb_L * 1e-3;
    const double Cab = Vb / (RHO * g_C * g_C);
    const double omegab = 2.0 * PI * fb;
    const double Map    = 1.0 / (omegab*omegab*Cab);
    const double Rp     = omegab * Map / std::max(QL_, 0.1);
    const Cpx Zcab(0.0, -1.0/(omega*Cab));
    const Cpx Zport(Rp, omega*Map);
    const Cpx Za = Zcab * Zport / (Zcab + Zport);
    return (Sdv * Za) / Zport;
}

static BPAmps bandpassAmplitudes(const BoxModel &m, double f, bool bp6)
{
    if ((bp6 && !hasBP6Data(m)) || (!bp6 && !hasBP4Data(m))) return {};
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms    = ds.mms;
    const double Sd     = ds.Sd;
    const double BL     = ds.BL;
    const double Re_eff = ds.Re_eff;
    const double Cms    = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms    = 2.0*PI * m.fs * mms / m.Qms;

    // Rear chamber: BP4 sealed → fb=0, BP6 vented
    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,        rearFb, rearQL, omega);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L,  m.fbFront, m.QLFront, omega);
    const Cpx Za_total = ZaRear + ZaFront;

    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx denom = Ze*Zm + Cpx(BL*BL) + Ze*(Sd*Sd*Za_total);
    if (std::abs(denom) < 1e-100) return {};
    const Cpx v   = Cpx(BL) / denom;
    const Cpx Sdv = Sd * v;

    BPAmps a;
    a.cone      = Sdv;
    a.rearPort  = chamberPortFlow(Sdv, m.volumeL,       rearFb,   rearQL,   omega);
    a.frontPort = chamberPortFlow(Sdv, m.volumeFront_L, m.fbFront, m.QLFront, omega);
    return a;
}

// Total radiated volume velocity (only ports radiate; cone is enclosed).
// BP4: front port only.
// BP6: cone moving forward compresses front chamber (+Sdv) but EXPANDS rear chamber
//      (-Sdv), so the rear port draws air IN when the front port pushes air OUT —
//      they are in anti-phase.  Total = frontPort − rearPort.
static Cpx bandpassUa(const BoxModel &m, double f)
{
    auto a = bandpassAmplitudes(m, f, isBP6(m));
    return isBP6(m) ? (a.frontPort - a.rearPort) : a.frontPort;
}

static double bandpassSplRaw(const BoxModel &m, double f)
{ return (2.0*PI*f) * std::abs(bandpassUa(m, f)); }

static double bandpassConeSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, isBP6(m)); return (2.0*PI*f) * std::abs(a.cone); }

static double bandpassRearPortSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, isBP6(m)); return (2.0*PI*f) * std::abs(a.rearPort); }

static double bandpassFrontPortSplRaw(const BoxModel &m, double f)
{ auto a = bandpassAmplitudes(m, f, isBP6(m)); return (2.0*PI*f) * std::abs(a.frontPort); }

static double bandpassGroupDelay(const BoxModel &m, double f)
{
    const double df = std::max(f * 0.002, 0.02);
    const Cpx ua1 = bandpassUa(m, f + df);
    const Cpx ua2 = bandpassUa(m, f - df);
    if (std::abs(ua1) < 1e-100 || std::abs(ua2) < 1e-100) return 0.0;
    const double dphi   = std::arg(std::conj(ua2) * ua1);
    const double domega = 2.0*PI * 2.0*df;
    return -dphi / domega * 1000.0;
}

static double bandpassImpedance(const BoxModel &m, double f)
{
    const bool bp6 = isBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return m.Re;
    const auto   ds     = driverScaling(m);
    const double omega  = 2.0 * PI * f;
    const double mms    = ds.mms;
    const double Sd     = ds.Sd;
    const double BL     = ds.BL;
    const double Re_eff = ds.Re_eff;
    const double Cms    = 1.0 / (4.0*PI*PI * m.fs*m.fs * mms);
    const double Rms    = 2.0*PI * m.fs * mms / m.Qms;
    const double rearFb = bp6 ? m.fb : 0.0;
    const double rearQL = bp6 ? m.QL : 0.0;
    const Cpx ZaRear  = chamberAcousticZ(m.volumeL,       rearFb,    rearQL,    omega);
    const Cpx ZaFront = chamberAcousticZ(m.volumeFront_L, m.fbFront, m.QLFront, omega);
    const Cpx Za_total = ZaRear + ZaFront;
    const Cpx Ze(Re_eff, 0.0);
    const Cpx Zm(Rms, omega*mms - 1.0/(omega*Cms));
    const Cpx Zmot = Zm + Cpx(Sd*Sd)*Za_total;
    return std::abs(Ze + Cpx(BL*BL) / Zmot);
}

// Bandpass passband boundaries: find the SPL peak, then -3 dB points
// below and above.  Returns {f3Low, f3High, peakDb, ripple}.
struct BPMetrics { double f3Low, f3High, peakDb, ripple; };
static BPMetrics bandpassMetrics(const BoxModel &m)
{
    BPMetrics out{0,0,0,0};
    const bool bp6 = isBP6(m);
    if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return out;
    // Sweep log-spaced search to locate the peak
    const int N = 400;
    double peak = 0.0; double fPeak = 1.0;
    double minInBand = 1e30;
    for (int i = 0; i < N; ++i) {
        const double t = double(i) / (N - 1);
        const double f = std::pow(10.0, std::log10(1.0) + t * (std::log10(500.0) - std::log10(1.0)));
        const double s = bandpassSplRaw(m, f);
        if (s > peak) { peak = s; fPeak = f; }
    }
    if (peak <= 0.0) return out;
    out.peakDb = 20.0 * std::log10(peak);
    const double target = peak * std::pow(10.0, -3.0/20.0);
    // Search f3Low: below fPeak, going up from 1 Hz
    {
        double lo = 1.0, hi = fPeak;
        if (bandpassSplRaw(m, lo) >= target) { out.f3Low = lo; }
        else {
            for (int i = 0; i < 64; ++i) {
                const double mid = std::sqrt(lo*hi);
                (bandpassSplRaw(m, mid) < target) ? lo = mid : hi = mid;
            }
            out.f3Low = std::sqrt(lo*hi);
        }
    }
    // Search f3High: above fPeak, up to 500 Hz
    {
        double lo = fPeak, hi = 500.0;
        if (bandpassSplRaw(m, hi) >= target) { out.f3High = hi; }
        else {
            for (int i = 0; i < 64; ++i) {
                const double mid = std::sqrt(lo*hi);
                (bandpassSplRaw(m, mid) >= target) ? lo = mid : hi = mid;
            }
            out.f3High = std::sqrt(lo*hi);
        }
    }
    // Ripple: scan within passband for max-min
    if (out.f3Low > 0 && out.f3High > out.f3Low) {
        double mn = 1e30, mx = 0.0;
        for (int i = 0; i < N; ++i) {
            const double t = double(i)/(N-1);
            const double f = std::pow(10.0, std::log10(out.f3Low) + t * (std::log10(out.f3High) - std::log10(out.f3Low)));
            const double s = bandpassSplRaw(m, f);
            if (s > 0) { mn = std::min(mn, s); mx = std::max(mx, s); }
        }
        if (mn > 0 && mx > 0) out.ripple = 20.0 * std::log10(mx / mn);
    }
    (void)minInBand;
    return out;
}

// Numerically find system f₋₃ for a ported box
static double portedF3(const BoxModel &m, double power)
{
    if (!hasPortedData(m)) return 0.0;
    const double ref    = portedSplRaw(m, 1000.0, power);
    if (ref <= 0) return 0.0;
    const double target = ref * std::pow(10.0, -3.0/20.0);
    // Binary search: response < target below f3, ≥ target above
    double lo = 1.0, hi = 500.0;
    if (portedSplRaw(m, hi, power) < target) return 0.0; // degenerate
    for (int i = 0; i < 64; ++i) {
        const double mid = std::sqrt(lo * hi);
        (portedSplRaw(m, mid, power) < target) ? lo = mid : hi = mid;
    }
    return std::sqrt(lo * hi);
}

// ─────────────────────────────────────────────────────────────────
//  Cursor helpers (used by all three plot types)
// ─────────────────────────────────────────────────────────────────
static constexpr int PLT_ML = 58, PLT_MR = 16, PLT_MT = 16, PLT_MB = 42;

static void plotUpdateCursor(QWidget *w, QMouseEvent *e, double &cursorFreq)
{
    const QRectF area(PLT_ML, PLT_MT,
                      w->width() - PLT_ML - PLT_MR,
                      w->height() - PLT_MT - PLT_MB);
    if (!area.contains(e->position())) {
        if (cursorFreq >= 0.0) { cursorFreq = -1.0; w->update(); }
        return;
    }
    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);
    const double lf = lfMin + (e->position().x() - area.left()) / area.width() * (lfMax - lfMin);
    const double freq = std::pow(10.0, std::clamp(lf, lfMin, lfMax));
    if (std::abs(freq - cursorFreq) > 0.05) { cursorFreq = freq; w->update(); }
}

struct CursorEntry { QColor color; bool active; QString name; double yPix; QString valStr; };

static void drawCursorOverlay(QPainter &p, const QRectF &area,
                               double cursorPx, double freqHz,
                               const QVector<CursorEntry> &entries)
{
    if (cursorPx < area.left() || cursorPx > area.right()) return;

    // Vertical crosshair — amber, more present than the gridlines
    QColor crosshair = Theme::instance().accent(); crosshair.setAlpha(160);
    p.setPen(QPen(crosshair, 1.0, Qt::DashLine));
    p.drawLine(QPointF(cursorPx, area.top()), QPointF(cursorPx, area.bottom()));

    // Dots on curves — filled with curve colour, page-bg hairline rim
    for (const auto &e : entries) {
        if (e.yPix < area.top() || e.yPix > area.bottom()) continue;
        p.setPen(QPen(Theme::instance().pageBg(), 1.0));
        p.setBrush(e.color);
        p.drawEllipse(QPointF(cursorPx, e.yPix), 4.0, 4.0);
    }

    // Annotation box — IBM Plex Mono on a dark panel
    QFont af("IBM Plex Mono"); af.setPointSize(8); p.setFont(af);
    const QFontMetrics fm(af);
    const QString freqStr = freqHz >= 1000.0
        ? QString("f = %1 kHz").arg(freqHz/1000.0, 0, 'f', 2)
        : QString("f = %1 Hz").arg(freqHz, 0, 'f', 1);

    QStringList lines; lines << freqStr;
    for (const auto &e : entries)
        lines << QString("  %1: %2").arg(e.name, e.valStr);

    int boxW = fm.horizontalAdvance(freqStr);
    for (int i = 0; i < entries.size(); ++i)
        boxW = std::max(boxW, fm.horizontalAdvance(lines[i + 1]));
    boxW += 16;
    const int lineH = fm.height() + 2;
    const int boxH  = lines.size() * lineH + 10;

    int bx = int(cursorPx) + 10;
    if (bx + boxW > int(area.right()) - 2)
        bx = int(cursorPx) - boxW - 10;
    const int by = int(area.top()) + 8;

    QColor boxBg = Theme::instance().plotCursorBg(); boxBg.setAlpha(230);
    p.setPen(QPen(Theme::instance().borderStrong(), 1.0));
    p.setBrush(boxBg);
    p.drawRoundedRect(bx, by, boxW, boxH, 1, 1);
    // Amber accent rule on the left edge
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::instance().accent());
    p.drawRect(bx, by, 2, boxH);

    QFont bf = af; bf.setBold(true); p.setFont(bf);
    p.setPen(Theme::instance().accentLight());
    p.drawText(bx + 10, by + 6 + fm.ascent(), freqStr);

    p.setFont(af);
    for (int i = 0; i < entries.size(); ++i) {
        p.setPen(entries[i].color);
        p.drawText(bx + 10, by + 6 + (i + 1)*lineH + fm.ascent(), lines[i + 1]);
    }
}

// ════════════════════════════════════════════════════════════════════
//  ResponsePlot – multi-curve
// ════════════════════════════════════════════════════════════════════
ResponsePlot::ResponsePlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void ResponsePlot::setModels(const QList<BoxModel> &models, int activeIndex)
{
    m_models    = models;
    m_activeIdx = activeIndex;
    update();
}

void ResponsePlot::clear()
{
    m_models.clear();
    m_activeIdx = -1;
    update();
}

void ResponsePlot::setPower(double watts) { m_power = watts; update(); }

void ResponsePlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 16, mt = 16, mb = 42;
    const QRectF area(ml, mt, width() - ml - mr, height() - mt - mb);
    p.fillRect(area, CLR_PLOT_BG());

    // Determine Y range by scanning the actual curve data for every model
    constexpr int SPL_SCAN = 300;
    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);

    double yMax = -1e9, yMin = 1e9;
    bool anyValid = false;
    for (const auto &m : m_models) {
        const double mp = modelPower(m);
        const double pwrOffset = mp > 0.0 ? 10.0 * std::log10(mp) : 0.0;
        if (isVented(m)) {
            if (!hasPortedData(m) || m.spl <= 0) continue;
            const double ref = portedSplRaw(m, 1000.0, mp);
            if (ref <= 0) continue;
            for (int i = 0; i <= SPL_SCAN; ++i) {
                const double f   = std::pow(10.0, lfMin + (lfMax - lfMin)*i/double(SPL_SCAN));
                const double raw = portedSplRaw(m, f, mp);
                if (raw <= 0) continue;
                const double db  = m.spl + pwrOffset + 20.0*std::log10(raw / ref);
                if (std::isfinite(db)) { yMax = std::max(yMax, db); yMin = std::min(yMin, db); }
            }
            anyValid = true;
        } else if (isBandpass(m)) {
            const bool bp6 = isBP6(m);
            if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) continue;
            double ref = 0.0;
            for (int i = 0; i <= SPL_SCAN; ++i) {
                const double f = std::pow(10.0, lfMin + (lfMax - lfMin)*i/double(SPL_SCAN));
                ref = std::max(ref, bandpassSplRaw(m, f));
            }
            if (ref <= 0) continue;
            const double peakDb = (m.peakSpl > 0) ? m.peakSpl : 100.0;
            for (int i = 0; i <= SPL_SCAN; ++i) {
                const double f   = std::pow(10.0, lfMin + (lfMax - lfMin)*i/double(SPL_SCAN));
                const double raw = bandpassSplRaw(m, f);
                if (raw <= 0) continue;
                const double db  = peakDb + pwrOffset + 20.0*std::log10(raw / ref);
                if (std::isfinite(db) && db > -100.0) { yMax = std::max(yMax, db); yMin = std::min(yMin, db); }
            }
            anyValid = true;
        } else {
            if (m.Fc <= 0 || m.Qtc <= 0 || m.spl <= 0) continue;
            for (int i = 0; i <= SPL_SCAN; ++i) {
                const double f   = std::pow(10.0, lfMin + (lfMax - lfMin)*i/double(SPL_SCAN));
                const double x   = f / m.Fc;
                const double xsq = x*x;
                const double d1  = 1.0 - xsq;
                const double den = d1*d1 + (x/m.Qtc)*(x/m.Qtc);
                if (den <= 0) continue;
                const double db  = m.spl + pwrOffset + 10.0*std::log10(xsq*xsq/den);
                if (std::isfinite(db) && db > -200.0) { yMax = std::max(yMax, db); yMin = std::min(yMin, db); }
            }
            anyValid = true;
        }
    }
    if (!anyValid) { yMax = 100.0; yMin = 70.0; }

    if (m_yMin.has_value() && m_yMax.has_value()) {
        yMin = *m_yMin; yMax = *m_yMax;
    } else {
        // Add headroom and snap to 5 dB grid
        yMax = std::ceil((yMax + 3.0) / 5.0) * 5.0;
        yMin = std::floor((yMin - 3.0) / 5.0) * 5.0;
        if (yMax - yMin < 10.0) yMax = yMin + 10.0;
    }

    auto xPx = [&](double f) {
        return area.left() + area.width() * (std::log10(f) - lfMin) / (lfMax - lfMin);
    };
    auto yPx = [&](double db) {
        return area.top() + area.height() * (yMax - db) / (yMax - yMin);
    };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = { 20, 30, 50, 70, 100, 200, 300, 500, 700, 1000 };
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    for (double db = yMin; db <= yMax + 0.01; db += 5.0)
        p.drawLine(QPointF(area.left(), yPx(db)), QPointF(area.right(), yPx(db)));

    // Axis labels
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = (f >= 1000.0) ? QString("%1k").arg(f/1000.0,0,'f',0)
                                    : QString::number(int(f));
        p.drawText(QRectF(xPx(f) - 20, area.bottom() + 2, 40, 14),
                   Qt::AlignHCenter, lbl);
    }
    for (double db = yMin; db <= yMax + 0.01; db += 5.0)
        p.drawText(QRectF(0, yPx(db) - 8, area.left() - 4, 16),
                   Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(int(db)));

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter,
               QString("SPL  (dB, %1 %2 / 1 m)").arg(m_power, 0, 'g', 3).arg(m_perDriverMode ? "W/driver" : "W"));
    p.restore();
    p.drawText(QRectF(area.left(), area.bottom() + 20, area.width(), 16),
               Qt::AlignHCenter, "Frequency (Hz)");

    // Axes
    p.setPen(QPen(CLR_GREY_DK(), 1.5));
    p.drawLine(QPointF(area.left(), area.top()), QPointF(area.left(), area.bottom()));
    p.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));

    if (!anyValid) {
        p.setPen(CLR_GREY_LT());
        QFont f; f.setPointSize(12); f.setItalic(true); p.setFont(f);
        p.drawText(area, Qt::AlignCenter,
                   "Add a model to see its frequency response.");
        return;
    }

    // Draw all curves (inactive first, then active on top)
    auto drawCurve = [&](const BoxModel &m, bool active) {
        constexpr int N = 500;
        const double mp = modelPower(m);
        const double pwrOffset = mp > 0.0 ? 10.0 * std::log10(mp) : 0.0;

        if (isVented(m)) {
            if (!hasPortedData(m) || m.spl <= 0) return;
            const double ref = portedSplRaw(m, 1000.0, mp);
            if (ref <= 0) return;

            // Helper: build a path from a raw-SPL function
            auto buildPortedPath = [&](auto rawFn) -> QPainterPath {
                QPainterPath path;
                bool first = true;
                for (int i = 0; i <= N; ++i) {
                    const double lf  = lfMin + (lfMax-lfMin)*i/double(N);
                    const double f   = std::pow(10.0, lf);
                    const double raw = rawFn(m, f);
                    if (raw <= 0) continue;
                    const double db  = m.spl + pwrOffset + 20.0*std::log10(raw / ref);
                    const QPointF pt(xPx(f), yPx(db));
                    if (first) { path.moveTo(pt); first = false; }
                    else       { path.lineTo(pt); }
                }
                return path;
            };

            // Total — solid, full weight
            {
                QPainterPath path = buildPortedPath([mp](const BoxModel &mm, double ff){ return portedSplRaw(mm, ff, mp); });
                QColor c = m.color; if (!active) c.setAlpha(100);
                p.setPen(QPen(c, active ? 3.0 : 1.8, Qt::SolidLine));
                p.setBrush(Qt::NoBrush);
                p.drawPath(path);
            }
            // Cone contribution — dashed
            {
                QPainterPath path = buildPortedPath([mp](const BoxModel &mm, double ff){ return portedConeSplRaw(mm, ff, mp); });
                QColor c = m.color; c.setAlpha(active ? 160 : 70);
                QPen pen(c, active ? 1.8 : 1.2, Qt::DashLine);
                p.setPen(pen); p.setBrush(Qt::NoBrush);
                p.drawPath(path);
            }
            // Port contribution — dotted
            {
                QPainterPath path = buildPortedPath([mp](const BoxModel &mm, double ff){ return portedPortSplRaw(mm, ff, mp); });
                QColor c = m.color; c.setAlpha(active ? 160 : 70);
                QPen pen(c, active ? 1.8 : 1.2, Qt::DotLine);
                p.setPen(pen); p.setBrush(Qt::NoBrush);
                p.drawPath(path);
            }
        } else if (isBandpass(m)) {
            const bool bp6 = isBP6(m);
            if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return;
            // Find raw peak; normalise so peak == m.peakSpl
            double ref = 0.0;
            for (int i = 0; i <= N; ++i) {
                const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/double(N));
                ref = std::max(ref, bandpassSplRaw(m, f));
            }
            if (ref <= 0) return;
            const double peakDb = (m.peakSpl > 0) ? m.peakSpl : 100.0;
            auto buildBPath = [&](auto rawFn) -> QPainterPath {
                QPainterPath path; bool first = true;
                for (int i = 0; i <= N; ++i) {
                    const double lf  = lfMin + (lfMax-lfMin)*i/double(N);
                    const double f   = std::pow(10.0, lf);
                    const double raw = rawFn(m, f);
                    if (raw <= 0) continue;
                    const double db  = peakDb + pwrOffset + 20.0*std::log10(raw / ref);
                    const QPointF pt(xPx(f), yPx(db));
                    if (first) { path.moveTo(pt); first = false; }
                    else       { path.lineTo(pt); }
                }
                return path;
            };
            // Total
            {
                QPainterPath path = buildBPath(bandpassSplRaw);
                QColor c = m.color; if (!active) c.setAlpha(100);
                p.setPen(QPen(c, active ? 3.0 : 1.8, Qt::SolidLine));
                p.setBrush(Qt::NoBrush); p.drawPath(path);
            }
            // For BP6: show individual port contributions (total = front − rear,
            // so the decomposition is meaningful).  For BP4: total == front port,
            // no useful decomposition to draw.
            if (bp6) {
                // Front port — dotted
                QPainterPath fpPath = buildBPath(bandpassFrontPortSplRaw);
                QColor cf = m.color; cf.setAlpha(active ? 160 : 70);
                p.setPen(QPen(cf, active ? 1.8 : 1.2, Qt::DotLine));
                p.setBrush(Qt::NoBrush); p.drawPath(fpPath);
                // Rear port — dashed
                QPainterPath rpPath = buildBPath(bandpassRearPortSplRaw);
                QColor cr = m.color; cr.setAlpha(active ? 160 : 70);
                p.setPen(QPen(cr, active ? 1.8 : 1.2, Qt::DashLine));
                p.setBrush(Qt::NoBrush); p.drawPath(rpPath);
            }
        } else {
            if (m.Fc <= 0 || m.Qtc <= 0) return;
            QPainterPath curve;
            bool first = true;
            for (int i = 0; i <= N; ++i) {
                const double lf = lfMin + (lfMax-lfMin)*i/double(N);
                const double f  = std::pow(10.0, lf);
                const double x  = f / m.Fc;
                const double xsq = x*x;
                const double d1  = 1.0-xsq;
                const double den = d1*d1 + (x/m.Qtc)*(x/m.Qtc);
                if (den <= 0.0) continue;
                const double db = m.spl + pwrOffset + 10.0*std::log10(xsq*xsq/den);
                const QPointF pt(xPx(f), yPx(db));
                if (first) { curve.moveTo(pt); first = false; }
                else       { curve.lineTo(pt); }
            }
            QColor c = m.color;
            if (!active) c.setAlpha(100);
            p.setPen(QPen(c, active ? 3.0 : 1.8));
            p.setBrush(Qt::NoBrush);
            p.drawPath(curve);
        }
    };

    p.setClipRect(area);
    // Inactive curves first
    for (int i = 0; i < m_models.size(); ++i)
        if (i != m_activeIdx) drawCurve(m_models[i], false);
    // Active curve on top
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        drawCurve(m_models[m_activeIdx], true);
    p.setClipping(false);

    // Legend (top-right of plot area)
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = static_cast<int>(area.right()) - 180;
        int ly = static_cast<int>(area.top()) + 8;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            const bool bp = isBandpass(m);
            const bool vt = isVented(m);
            if (vt ? (!hasPortedData(m) || m.spl <= 0)
                : bp ? ((isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) || m.peakSpl <= 0)
                : (m.Fc <= 0 || m.Qtc <= 0)) continue;
            QColor c = m.color;
            bool active = (i == m_activeIdx);
            if (!active) c.setAlpha(140);
            // Model name row (solid line)
            p.setPen(QPen(c, active ? 2.5 : 1.5, Qt::SolidLine));
            p.drawLine(QPoint(lx, ly + 6), QPoint(lx + 20, ly + 6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx + 24, ly, 150, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       m.name);
            ly += 16;
            // Sub-entries for ported models
            if (isVented(m)) {
                QFont sf; sf.setPointSize(7); p.setFont(sf);
                QColor cs = m.color; cs.setAlpha(active ? 160 : 90);
                // Cone (dashed)
                p.setPen(QPen(cs, active ? 1.6 : 1.1, Qt::DashLine));
                p.drawLine(QPoint(lx + 8, ly + 5), QPoint(lx + 24, ly + 5));
                p.setPen(active ? CLR_GREY() : CLR_GREY_LT());
                p.drawText(QRect(lx + 28, ly, 140, 12), Qt::AlignLeft | Qt::AlignVCenter, "Cone");
                ly += 13;
                // Port (dotted)
                p.setPen(QPen(cs, active ? 1.6 : 1.1, Qt::DotLine));
                p.drawLine(QPoint(lx + 8, ly + 5), QPoint(lx + 24, ly + 5));
                p.setPen(active ? CLR_GREY() : CLR_GREY_LT());
                p.setFont(sf);
                p.drawText(QRect(lx + 28, ly, 140, 12), Qt::AlignLeft | Qt::AlignVCenter, "Port");
                ly += 13;
            }
        }
    }

    // Key frequency markers for active model
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size()) {
        const auto &am = m_models[m_activeIdx];
        // Fc marker (sealed) or fb marker (vented)
        const double markerFreq = isVented(am) ? am.fb : am.Fc;
        const QString markerLabel = isVented(am)
            ? QString("fb = %1 Hz").arg(am.fb,0,'f',1)
            : QString("Fc = %1 Hz").arg(am.Fc,0,'f',1);
        if (markerFreq > F_MIN && markerFreq < F_MAX) {
            const double x = xPx(markerFreq);
            QColor fc_clr = am.color; fc_clr.setAlpha(180);
            p.setPen(QPen(fc_clr, 1.2, Qt::DashLine));
            p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
            p.setPen(fc_clr);
            QFont bf; bf.setPointSize(8); bf.setBold(true); p.setFont(bf);
            p.drawText(QRectF(x + 4, area.top() + 4, 100, 14), Qt::AlignLeft, markerLabel);
        }
        if (am.f3 > F_MIN && am.f3 < F_MAX) {
            const double x = xPx(am.f3);
            QColor f3_clr = am.color; f3_clr.setAlpha(140);
            p.setPen(QPen(f3_clr, 1.2, Qt::DashLine));
            p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
            p.setPen(f3_clr);
            QFont bf; bf.setPointSize(8); bf.setBold(true); p.setFont(bf);
            p.drawText(QRectF(x + 4, area.top() + 20, 100, 14),
                       Qt::AlignLeft, QString("f₋₃ = %1 Hz").arg(am.f3,0,'f',1));
        }
        if (isVented(am) && am.portF2H > F_MIN && am.portF2H < F_MAX) {
            const double x = xPx(am.portF2H);
            QColor h2_clr = am.color; h2_clr.setAlpha(110);
            p.setPen(QPen(h2_clr, 1.0, Qt::DotLine));
            p.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
            p.setPen(h2_clr);
            QFont bf; bf.setPointSize(8); bf.setBold(true); p.setFont(bf);
            p.drawText(QRectF(x + 4, area.top() + 36, 120, 14),
                       Qt::AlignLeft,
                       QString("f₂H = %1 Hz").arg(am.portF2H, 0, 'f', 1));
        }
        if (am.spl > 0) {
            QFont sf; sf.setPointSize(7); p.setFont(sf);
            QColor lnClr = am.color; lnClr.setAlpha(110);
            const double mp = modelPower(am);
            const double pwrOffset = mp > 0.0 ? 10.0 * std::log10(mp) : 0.0;

            // +3 dB line
            const double yp3 = am.spl + pwrOffset + 3.0;
            if (yp3 >= yMin && yp3 <= yMax) {
                p.setPen(QPen(lnClr, 1.0, Qt::DashLine));
                p.drawLine(QPointF(area.left(), yPx(yp3)),
                           QPointF(area.right(), yPx(yp3)));
                p.setPen(lnClr);
                p.drawText(QRectF(area.right() - 42, yPx(yp3) - 13, 40, 12),
                           Qt::AlignRight | Qt::AlignVCenter, "+3 dB");
            }

            // Reference (0 dB = rated SPL)
            p.setPen(QPen(CLR_GREY_LT(), 1.0, Qt::DashLine));
            p.drawLine(QPointF(area.left(), yPx(am.spl + pwrOffset)),
                       QPointF(area.right(), yPx(am.spl + pwrOffset)));
            p.setPen(CLR_GREY());
            p.drawText(QRectF(area.right() - 42, yPx(am.spl + pwrOffset) - 13, 40, 12),
                       Qt::AlignRight | Qt::AlignVCenter, "ref");

            // −3 dB line
            const double ym3 = am.spl + pwrOffset - 3.0;
            if (ym3 >= yMin && ym3 <= yMax) {
                p.setPen(QPen(lnClr, 1.0, Qt::DashLine));
                p.drawLine(QPointF(area.left(), yPx(ym3)),
                           QPointF(area.right(), yPx(ym3)));
                p.setPen(lnClr);
                p.drawText(QRectF(area.right() - 42, yPx(ym3) + 2, 40, 12),
                           Qt::AlignRight | Qt::AlignVCenter, "−3 dB");
            }
        }
    }

    // Cursor overlay
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        const double cf = m_cursorFreq;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            bool active = (i == m_activeIdx);
            const double mp = modelPower(m);
            const double pwrOffset = mp > 0.0 ? 10.0 * std::log10(mp) : 0.0;
            if (isVented(m)) {
                if (!hasPortedData(m) || m.spl <= 0) continue;
                const double ref = portedSplRaw(m, 1000.0, mp);
                if (ref <= 0) continue;
                // Total
                const double dbTotal = m.spl + pwrOffset + 20.0*std::log10(portedSplRaw(m, cf, mp) / ref);
                entries.append({m.color, active,
                                m.name + " (total)",
                                yPx(dbTotal), QString("%1 dB").arg(dbTotal, 0, 'f', 1)});
                // Cone
                QColor cc = m.color; cc.setAlpha(active ? 200 : 120);
                const double rawCone = portedConeSplRaw(m, cf, mp);
                if (rawCone > 0) {
                    const double dbCone = m.spl + pwrOffset + 20.0*std::log10(rawCone / ref);
                    entries.append({cc, false,
                                    "  cone",
                                    yPx(dbCone), QString("%1 dB").arg(dbCone, 0, 'f', 1)});
                }
                // Port
                const double rawPort = portedPortSplRaw(m, cf, mp);
                if (rawPort > 0) {
                    const double dbPort = m.spl + pwrOffset + 20.0*std::log10(rawPort / ref);
                    entries.append({cc, false,
                                    "  port",
                                    yPx(dbPort), QString("%1 dB").arg(dbPort, 0, 'f', 1)});
                }
            } else {
                if (m.Fc <= 0 || m.Qtc <= 0 || m.spl <= 0) continue;
                const double x   = cf / m.Fc;
                const double xsq = x*x;
                const double d1  = 1.0 - xsq;
                const double den = d1*d1 + (x/m.Qtc)*(x/m.Qtc);
                if (den <= 0) continue;
                const double db = m.spl + pwrOffset + 10.0*std::log10(xsq*xsq/den);
                entries.append({m.color, active, m.name,
                                yPx(db), QString("%1 dB").arg(db, 0, 'f', 1)});
            }
        }
        drawCursorOverlay(p, area, xPx(cf), cf, entries);
    }
}

// ════════════════════════════════════════════════════════════════════
//  GroupDelayPlot
// ════════════════════════════════════════════════════════════════════
GroupDelayPlot::GroupDelayPlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void GroupDelayPlot::setModels(const QList<BoxModel> &models, int activeIndex)
{
    m_models = models; m_activeIdx = activeIndex; update();
}
void GroupDelayPlot::clear() { m_models.clear(); m_activeIdx = -1; update(); }

void GroupDelayPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 16, mt = 16, mb = 42;
    const QRectF area(ml, mt, width()-ml-mr, height()-mt-mb);
    p.fillRect(area, CLR_PLOT_BG());

    // Y range: scan the full frequency range to find the true peak group delay
    double yMax = 10.0;
    bool anyValid = false;
    {
        constexpr int GD_SCAN = 200;
        const double lfLo = std::log10(F_MIN), lfHi = std::log10(F_MAX);
        for (const auto &m : m_models) {
            if (isVented(m)) {
                if (!hasPortedData(m)) continue;
                for (int i = 0; i <= GD_SCAN; ++i) {
                    const double f  = std::pow(10.0, lfLo + (lfHi-lfLo)*i/double(GD_SCAN));
                    const double gd = portedGroupDelay(m, f, 0.0 /* power: no GD power control; linear limit (Task 6: revisit) */);
                    if (std::isfinite(gd) && gd > 0) yMax = std::max(yMax, gd);
                }
                anyValid = true;
            } else if (isBandpass(m)) {
                if (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) continue;
                for (int i = 0; i <= GD_SCAN; ++i) {
                    const double f  = std::pow(10.0, lfLo + (lfHi-lfLo)*i/double(GD_SCAN));
                    const double gd = bandpassGroupDelay(m, f);
                    if (std::isfinite(gd) && gd > 0) yMax = std::max(yMax, gd);
                }
                anyValid = true;
            } else if (m.Fc > 0 && m.Qtc > 0) {
                const double peak_ms = 1000.0 / (2.0*PI*m.Fc*m.Qtc);
                yMax = std::max(yMax, peak_ms);
                anyValid = true;
            }
        }
    }
    double yMin = 0.0;
    double niceStep;
    if (m_yMin.has_value() && m_yMax.has_value()) {
        yMin = *m_yMin; yMax = *m_yMax;
        const double rawStep = (yMax - yMin) / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(std::max(rawStep, 1e-9))));
        niceStep = std::ceil(rawStep / mag) * mag;
    } else {
        yMax *= 1.15;
        const double rawStep = yMax / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(rawStep)));
        niceStep = std::ceil(rawStep / mag) * mag;
        yMax = std::ceil(yMax / niceStep) * niceStep;
    }

    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);
    auto xPx = [&](double f)  { return area.left() + area.width()*(std::log10(f)-lfMin)/(lfMax-lfMin); };
    auto yPx = [&](double ms) { return area.top()  + area.height()*(yMax-ms)/(yMax-yMin); };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = {20,30,50,70,100,200,300,500,700,1000};
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    for (double ms = 0; ms <= yMax+0.01; ms += niceStep)
        p.drawLine(QPointF(area.left(), yPx(ms)), QPointF(area.right(), yPx(ms)));

    // Axis labels
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = f>=1000 ? QString("%1k").arg(f/1000,0,'f',0) : QString::number(int(f));
        p.drawText(QRectF(xPx(f)-20, area.bottom()+2, 40, 14), Qt::AlignHCenter, lbl);
    }
    for (double ms = 0; ms <= yMax+0.01; ms += niceStep)
        p.drawText(QRectF(0, yPx(ms)-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, QString::number(int(ms)));

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Group Delay  (ms)");
    p.restore();
    p.drawText(QRectF(area.left(), area.bottom()+20, area.width(), 16),
               Qt::AlignHCenter, "Frequency (Hz)");

    // Axes
    p.setPen(QPen(CLR_GREY_DK(), 1.5));
    p.drawLine(QPointF(area.left(), area.top()), QPointF(area.left(), area.bottom()));
    p.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));

    if (!anyValid) {
        p.setPen(CLR_GREY_LT());
        QFont f; f.setPointSize(12); f.setItalic(true); p.setFont(f);
        p.drawText(area, Qt::AlignCenter, "Add a model to see group delay.");
        return;
    }

    auto drawCurve = [&](const BoxModel &m, bool active) {
        QPainterPath curve; bool first = true;
        if (isVented(m)) {
            if (!hasPortedData(m)) return;
            for (int i = 0; i <= 500; ++i) {
                const double lf = lfMin + (lfMax-lfMin)*i/500.0;
                const double f  = std::pow(10.0, lf);
                const double gd = portedGroupDelay(m, f, 0.0 /* power: no GD power control; linear limit (Task 6: revisit) */);
                if (gd < 0 || gd > yMax*2) continue;
                const QPointF pt(xPx(f), yPx(gd));
                if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
            }
        } else if (isBandpass(m)) {
            if (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) return;
            for (int i = 0; i <= 500; ++i) {
                const double lf = lfMin + (lfMax-lfMin)*i/500.0;
                const double f  = std::pow(10.0, lf);
                const double gd = bandpassGroupDelay(m, f);
                if (gd < 0 || gd > yMax*2) continue;
                const QPointF pt(xPx(f), yPx(gd));
                if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
            }
        } else {
            if (m.Fc <= 0 || m.Qtc <= 0) return;
            const double wc = 2.0*PI*m.Fc;
            for (int i = 0; i <= 500; ++i) {
                const double lf = lfMin + (lfMax-lfMin)*i/500.0;
                const double f  = std::pow(10.0, lf);
                const double x  = f/m.Fc;
                const double x2 = x*x;
                const double den = (x/m.Qtc)*(x/m.Qtc) + (1.0-x2)*(1.0-x2);
                if (den <= 0) continue;
                const double gd = 1000.0/wc * (1.0/m.Qtc) * (1.0+x2)/den;
                if (gd < 0 || gd > yMax*2) continue;
                const QPointF pt(xPx(f), yPx(gd));
                if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
            }
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setPen(QPen(c, active ? 3.0 : 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);
    };

    p.setClipRect(area);
    for (int i = 0; i < m_models.size(); ++i)
        if (i != m_activeIdx) drawCurve(m_models[i], false);
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        drawCurve(m_models[m_activeIdx], true);
    p.setClipping(false);

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-180; int ly = int(area.top())+8;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (isVented(m) ? !hasPortedData(m)
                : isBandpass(m) ? (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m))
                : (m.Fc <= 0 || m.Qtc <= 0)) continue;
            bool active = (i == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx+24, ly, 150, 14), Qt::AlignLeft|Qt::AlignVCenter, m.name);
            ly += 16;
        }
    }

    // Cursor overlay
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        const double cf = m_cursorFreq;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            double gd;
            if (isVented(m)) {
                if (!hasPortedData(m)) continue;
                gd = portedGroupDelay(m, cf, 0.0 /* power: no GD power control; linear limit (Task 6: revisit) */);
            } else if (isBandpass(m)) {
                if (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) continue;
                gd = bandpassGroupDelay(m, cf);
            } else {
                if (m.Fc <= 0 || m.Qtc <= 0) continue;
                const double x  = cf/m.Fc;
                const double x2 = x*x;
                const double den = (x/m.Qtc)*(x/m.Qtc) + (1.0-x2)*(1.0-x2);
                if (den <= 0) continue;
                gd = 1000.0/(2.0*PI*m.Fc) * (1.0/m.Qtc) * (1.0+x2)/den;
            }
            if (gd < 0) continue;
            entries.append({m.color, i == m_activeIdx, m.name,
                            yPx(gd), QString("%1 ms").arg(gd, 0, 'f', 2)});
        }
        drawCursorOverlay(p, area, xPx(cf), cf, entries);
    }
}

// ════════════════════════════════════════════════════════════════════
//  VoltagePlot
// ════════════════════════════════════════════════════════════════════
VoltagePlot::VoltagePlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void VoltagePlot::setModels(const QList<BoxModel> &models, int activeIndex)
{
    m_models = models; m_activeIdx = activeIndex; update();
}
void VoltagePlot::setPower(double watts) { m_power = watts; update(); }
void VoltagePlot::clear() { m_models.clear(); m_activeIdx = -1; update(); }

// Returns true if the model has enough data to compute impedance
static bool hasImpedanceData(const BoxModel &m)
{
    if (isVented(m)) return hasPortedData(m);
    if (isBP4(m))    return hasBP4Data(m);
    if (isBP6(m))    return hasBP6Data(m);
    return m.Re > 0 && m.BL > 0 && m.mms_g > 0 && m.Qms > 0
           && m.Fc > 0 && m.Qtc > 0 && m.alpha > 0;
}

// Impedance magnitude |Z(f)|
static double boxImpedance(const BoxModel &m, double f, double power)
{
    if (isVented(m))   return portedImpedance(m, f, power);
    if (isBandpass(m)) return bandpassImpedance(m, f);
    const double wc      = 2.0 * PI * m.Fc;
    const double mms     = m.mms_g / 1000.0;
    const double Cms_box = 1.0 / (wc * wc * mms);
    const double Qms_c   = m.Qms * std::sqrt(m.alpha + 1.0);
    const double Rms_box = wc * mms / Qms_c;
    const double w       = 2.0 * PI * f;
    const double Zm_i    = w * mms - 1.0 / (w * Cms_box);
    const double Zm2     = Rms_box * Rms_box + Zm_i * Zm_i;
    const double Zr      = m.Re + m.BL * m.BL * Rms_box / Zm2;
    const double Zi      = -m.BL * m.BL * Zm_i / Zm2;
    return std::sqrt(Zr*Zr + Zi*Zi);
}

// Impedance |Z(f)| for the whole system (all N drivers, correct for sealed/IB/vented).
static bool hasSystemZData(const BoxModel &m)
{
    if (isVented(m))         return hasPortedData(m);
    if (isBP4(m))            return hasBP4Data(m);
    if (isBP6(m))            return hasBP6Data(m);
    if (isIB(m)) return m.Re > 0 && m.BL > 0 && m.mms_g > 0 && m.Qms > 0 && m.Fc > 0;
    return m.Re > 0 && m.BL > 0 && m.mms_g > 0 && m.Qms > 0 && m.Fc > 0 && m.alpha > 0;
}

static double systemImpedance(const BoxModel &m, double f, double power)
{
    if (isVented(m))   return portedImpedance(m, f, power);   // portedImpedance already scales for N
    if (isBandpass(m)) return bandpassImpedance(m, f); // bandpassImpedance scales for N
    const double N = m.numDrivers;
    if (!hasSystemZData(m)) return m.Re * N;
    // Sealed / IB: use pre-computed Fc (which already incorporates box stiffness and N*Vas).
    // Single-driver equivalent circuit; final impedance scaled by wiring mode.
    const double wc      = 2.0 * PI * m.Fc;
    const double mms     = m.mms_g * 1e-3;
    const double Cms_box = 1.0 / (wc * wc * mms);
    const double Qms_c   = m.Qms * (isIB(m) ? 1.0 : std::sqrt(m.alpha + 1.0));
    const double Rms_box = wc * mms / Qms_c;
    const double w       = 2.0 * PI * f;
    const double Zm_i    = w * mms - 1.0 / (w * Cms_box);
    const double Zm2     = Rms_box * Rms_box + Zm_i * Zm_i;
    const double Zr      = m.Re + m.BL * m.BL * Rms_box / Zm2;
    const double Zi      = -m.BL * m.BL * Zm_i / Zm2;
    const double Zsingle = std::sqrt(Zr * Zr + Zi * Zi);
    // Series: N impedances in series. Parallel: N in parallel. Separate: per-channel = single.
    double scale = N;
    if      (m.wiringMode == BoxModel::WiringMode::Parallel)  scale = 1.0 / N;
    else if (m.wiringMode == BoxModel::WiringMode::Separate)  scale = 1.0;
    return scale * Zsingle;
}

// Cone displacement [mm peak] for a given applied power.
// Uses the same motional-impedance circuit as VoltagePlot / boxImpedance.
static double coneDisplacement_mm(const BoxModel &m, double f, double power)
{
    if (power <= 0) return 0.0;
    const double omega = 2.0 * PI * f;
    if (omega < 1e-10) return 0.0;

    if (isVented(m)) {
        if (!hasPortedData(m)) return 0.0;
        const double Sd = m.Sd_cm2 * 1e-4;
        if (Sd <= 0) return 0.0;
        auto a = portedAmplitudes(m, f, power); // cone = Sd·v  for 1 V input
        const double v_1V = std::abs(a.cone) / Sd;
        const double Z = portedImpedance(m, f, power);
        if (Z <= 0) return 0.0;
        // V = sqrt(P·Z),  |x| = |v_1V|·V / ω
        return v_1V * std::sqrt(power * Z) / omega * 1000.0;
    } else if (isBandpass(m)) {
        const bool bp6 = isBP6(m);
        if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return 0.0;
        const double Sd = m.Sd_cm2 * 1e-4;
        if (Sd <= 0) return 0.0;
        auto a = bandpassAmplitudes(m, f, bp6);
        const double v_1V = std::abs(a.cone) / Sd;
        const double Z = bandpassImpedance(m, f);
        if (Z <= 0) return 0.0;
        return v_1V * std::sqrt(power * Z) / omega * 1000.0;
    } else {
        if (!hasSystemZData(m)) return 0.0;
        const double wc      = 2.0 * PI * m.Fc;
        const double mms     = m.mms_g * 1e-3;
        const double Cms_box = 1.0 / (wc * wc * mms);
        const double Qms_c   = m.Qms * (isIB(m) ? 1.0 : std::sqrt(m.alpha + 1.0));
        const double Rms_box = wc * mms / Qms_c;
        const double Zm_i    = omega * mms - 1.0 / (omega * Cms_box);
        const double Zm_abs  = std::sqrt(Rms_box * Rms_box + Zm_i * Zm_i);
        const double Z_elec  = systemImpedance(m, f, power);
        if (Z_elec <= 0 || Zm_abs <= 0) return 0.0;
        // |x| = BL · |I| / (|Zm| · ω),  |I| = sqrt(P / Z)
        return m.BL * std::sqrt(power / Z_elec) / (Zm_abs * omega) * 1000.0;
    }
}

void VoltagePlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 54, mt = 16, mb = 42;
    const QRectF area(ml, mt, width()-ml-mr, height()-mt-mb);
    p.fillRect(area, CLR_PLOT_BG());

    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);

    // In Separate mode each driver runs on its own amp, so the voltage curve
    // represents what a single amp delivers — divide effective power by N.
    auto perAmpPower = [this](const BoxModel &m) {
        const double mp = modelPower(m);
        return (m.wiringMode == BoxModel::WiringMode::Separate)
            ? mp / std::max(1, m.numDrivers) : mp;
    };
    // Current delivered by one amp = V / Z_amp. For series/parallel this is
    // the single amp's draw; for separate it's the current per amp channel.
    auto ampCurrent = [&](const BoxModel &m, double f) {
        const double mp = perAmpPower(m);
        const double Z = systemImpedance(m, f, mp);
        return Z > 0 ? std::sqrt(mp / Z) : 0.0;
    };

    // Y ranges: scan all models for both V (left axis) and I (right axis).
    double yMax = 5.0;
    double iMax = 0.5;
    bool anyValid = false;
    for (const auto &m : m_models) {
        if (!hasSystemZData(m)) continue;
        anyValid = true;
        const double mp = perAmpPower(m);
        for (int i = 0; i <= 60; ++i) {
            const double lf = lfMin + (lfMax-lfMin)*i/60.0;
            const double f  = std::pow(10.0, lf);
            const double Z  = systemImpedance(m, f, mp);
            const double V  = std::sqrt(mp * Z);
            const double I  = Z > 0 ? std::sqrt(mp / Z) : 0.0;
            yMax = std::max(yMax, V);
            iMax = std::max(iMax, I);
        }
    }
    double yMin = 0.0;
    double iMin = 0.0;
    if (m_yMin.has_value() && m_yMax.has_value()) {
        yMin = *m_yMin; yMax = *m_yMax;
    } else {
        yMax = std::ceil(yMax * 1.1 / 5.0) * 5.0;
    }
    iMax = std::ceil(iMax * 1.1 * 4.0) / 4.0;  // round to 0.25 A
    if (iMax < 0.5) iMax = 0.5;

    auto xPx = [&](double f) { return area.left() + area.width()*(std::log10(f)-lfMin)/(lfMax-lfMin); };
    auto yPx = [&](double v) { return area.top()  + area.height()*(yMax-v)/(yMax-yMin); };
    auto iPx = [&](double i) { return area.top()  + area.height()*(iMax-i)/(iMax-iMin); };

    auto fmtI = [](double a) -> QString {
        return a < 1.0 ? QString::number(a, 'f', 2)
             : a < 10.0 ? QString::number(a, 'f', 1)
                        : QString::number(qRound(a));
    };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = {20,30,50,70,100,200,300,500,700,1000};
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    const double yStep = yMax > 50 ? 10.0 : yMax > 20 ? 5.0 : 2.0;
    for (double v = 0; v <= yMax+0.01; v += yStep)
        p.drawLine(QPointF(area.left(), yPx(v)), QPointF(area.right(), yPx(v)));

    // Left axis labels — Voltage
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = f>=1000 ? QString("%1k").arg(f/1000,0,'f',0) : QString::number(int(f));
        p.drawText(QRectF(xPx(f)-20, area.bottom()+2, 40, 14), Qt::AlignHCenter, lbl);
    }
    for (double v = 0; v <= yMax+0.01; v += yStep)
        p.drawText(QRectF(0, yPx(v)-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, QString::number(int(v)));

    // Right axis labels — Current (A) at convenient tick steps.
    const double iStep = iMax > 20 ? 5.0 : iMax > 8 ? 2.0 : iMax > 4 ? 1.0
                       : iMax > 2 ? 0.5 : iMax > 1 ? 0.25 : 0.1;
    for (double a = 0; a <= iMax + 1e-6; a += iStep) {
        p.drawText(QRectF(area.right()+4, iPx(a)-8, mr-8, 16),
                   Qt::AlignLeft|Qt::AlignVCenter, fmtI(a));
    }

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Voltage  (V RMS)");
    p.restore();

    // Right axis title
    p.save();
    p.translate(width()-12, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Current  (A RMS)");
    p.restore();

    p.drawText(QRectF(area.left(), area.bottom()+20, area.width(), 16),
               Qt::AlignHCenter, "Frequency (Hz)");

    // Axes — left, bottom, and right
    p.setPen(QPen(CLR_GREY_DK(), 1.5));
    p.drawLine(QPointF(area.left(),  area.top()),    QPointF(area.left(),  area.bottom()));
    p.drawLine(QPointF(area.left(),  area.bottom()), QPointF(area.right(), area.bottom()));
    p.drawLine(QPointF(area.right(), area.top()),    QPointF(area.right(), area.bottom()));

    if (!anyValid) {
        p.setPen(CLR_GREY_LT());
        QFont f; f.setPointSize(12); f.setItalic(true); p.setFont(f);
        p.drawText(area, Qt::AlignCenter,
                   "Set Rₑ, BL, mms, and Qms to see voltage demand.");
        return;
    }

    auto drawCurve = [&](const BoxModel &m, bool active) {
        if (!hasSystemZData(m)) return;
        const double mp = perAmpPower(m);
        QPainterPath vPath, iPath;
        bool vFirst = true, iFirst = true;
        for (int i = 0; i <= 500; ++i) {
            const double lf = lfMin + (lfMax-lfMin)*i/500.0;
            const double f  = std::pow(10.0, lf);
            const double Z  = systemImpedance(m, f, mp);
            if (Z <= 0) continue;
            const double V  = std::sqrt(mp * Z);
            const double I  = std::sqrt(mp / Z);
            if (V <= yMax * 1.5) {
                const QPointF pt(xPx(f), yPx(V));
                if (vFirst) { vPath.moveTo(pt); vFirst = false; } else vPath.lineTo(pt);
            }
            if (I <= iMax * 1.5) {
                const QPointF pt(xPx(f), iPx(I));
                if (iFirst) { iPath.moveTo(pt); iFirst = false; } else iPath.lineTo(pt);
            }
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setBrush(Qt::NoBrush);
        // Voltage — solid
        p.setPen(QPen(c, active ? 3.0 : 1.8));
        p.drawPath(vPath);
        // Current — dashed, slightly thinner
        p.setPen(QPen(c, active ? 2.0 : 1.2, Qt::DashLine));
        p.drawPath(iPath);
    };

    p.setClipRect(area);
    for (int i = 0; i < m_models.size(); ++i)
        if (i != m_activeIdx) drawCurve(m_models[i], false);
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        drawCurve(m_models[m_activeIdx], true);
    p.setClipping(false);

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.left())+8; int ly = int(area.top())+8;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasSystemZData(m)) continue;
            bool active = (i == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx+24, ly, 160, 14), Qt::AlignLeft|Qt::AlignVCenter, m.name);
            ly += 16;
        }
    }

    // Cursor overlay
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasSystemZData(m)) continue;
            const double mp = perAmpPower(m);
            const double Z = systemImpedance(m, m_cursorFreq, mp);
            if (Z <= 0) continue;
            const double V = std::sqrt(mp * Z);
            const double I = std::sqrt(mp / Z);
            const QString zStr = Z < 10.0 ? QString::number(Z, 'f', 1)
                                          : QString::number(qRound(Z));
            entries.append({m.color, i == m_activeIdx, m.name,
                            yPx(V),
                            QString("%1 V  |  %2 A  |  %3 \u03a9")
                                .arg(V, 0, 'f', 2).arg(I, 0, 'f', 2).arg(zStr)});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
}

// Mouse event handlers for each plot type
void ResponsePlot::mouseMoveEvent(QMouseEvent *e)  { plotUpdateCursor(this, e, m_cursorFreq); }
void ResponsePlot::leaveEvent(QEvent *)             { m_cursorFreq = -1.0; update(); }

void GroupDelayPlot::mouseMoveEvent(QMouseEvent *e) { plotUpdateCursor(this, e, m_cursorFreq); }
void GroupDelayPlot::leaveEvent(QEvent *)           { m_cursorFreq = -1.0; update(); }

void VoltagePlot::mouseMoveEvent(QMouseEvent *e)    { plotUpdateCursor(this, e, m_cursorFreq); }
void VoltagePlot::leaveEvent(QEvent *)              { m_cursorFreq = -1.0; update(); }

// ════════════════════════════════════════════════════════════════════
//  ExcursionPlot
// ════════════════════════════════════════════════════════════════════
ExcursionPlot::ExcursionPlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void ExcursionPlot::setModels(const QList<BoxModel> &models, int activeIndex)
{
    m_models = models; m_activeIdx = activeIndex; update();
}
void ExcursionPlot::setPower(double watts) { m_power = watts; update(); }
void ExcursionPlot::clear() { m_models.clear(); m_activeIdx = -1; update(); }

static bool hasExcursionData(const BoxModel &m)
{
    if (isVented(m)) return hasPortedData(m);
    if (isBP4(m))    return hasBP4Data(m);
    if (isBP6(m))    return hasBP6Data(m);
    return hasSystemZData(m);
}

void ExcursionPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 16, mt = 16, mb = 42;
    const QRectF area(ml, mt, width()-ml-mr, height()-mt-mb);
    p.fillRect(area, CLR_PLOT_BG());

    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);

    // Y range: scan all models
    double yMax = 1.0;
    bool anyValid = false;
    for (const auto &m : m_models) {
        if (!hasExcursionData(m)) continue;
        anyValid = true;
        const double mp = modelPower(m);
        for (int i = 0; i <= 80; ++i) {
            const double f  = std::pow(10.0, lfMin + (lfMax-lfMin)*i/80.0);
            const double x  = coneDisplacement_mm(m, f, mp);
            if (std::isfinite(x)) yMax = std::max(yMax, x);
        }
        if (m.xmax_mm > 0) yMax = std::max(yMax, m.xmax_mm);
        if (m.xlim_mm > 0) yMax = std::max(yMax, m.xlim_mm);
    }
    double yMin = 0.0;
    double niceStep;
    if (m_yMin.has_value() && m_yMax.has_value()) {
        yMin = *m_yMin; yMax = *m_yMax;
        const double rawStep = (yMax - yMin) / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(std::max(rawStep, 1e-9))));
        niceStep = std::ceil(rawStep / mag) * mag;
    } else {
        yMax = std::ceil(yMax * 1.15 / 1.0) * 1.0;
        if (yMax < 1.0) yMax = 1.0;
        const double rawStep = yMax / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(std::max(rawStep, 1e-9))));
        niceStep = std::ceil(rawStep / mag) * mag;
        yMax = std::ceil(yMax / niceStep) * niceStep;
    }

    auto xPx = [&](double f) { return area.left() + area.width()*(std::log10(f)-lfMin)/(lfMax-lfMin); };
    auto yPx = [&](double x) { return area.top()  + area.height()*(yMax-x)/(yMax-yMin); };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = {20,30,50,70,100,200,300,500,700,1000};
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    for (double v = 0; v <= yMax+1e-9; v += niceStep)
        p.drawLine(QPointF(area.left(), yPx(v)), QPointF(area.right(), yPx(v)));

    // Axis labels
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = f>=1000 ? QString("%1k").arg(f/1000,0,'f',0) : QString::number(int(f));
        p.drawText(QRectF(xPx(f)-20, area.bottom()+2, 40, 14), Qt::AlignHCenter, lbl);
    }
    // Y label: show 1 decimal if step < 1, else integer
    const bool showDec = niceStep < 1.0;
    for (double v = 0; v <= yMax+1e-9; v += niceStep) {
        const QString lbl = showDec ? QString::number(v, 'f', 1) : QString::number(int(std::round(v)));
        p.drawText(QRectF(0, yPx(v)-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, lbl);
    }

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Cone Displacement  (mm peak)");
    p.restore();
    p.drawText(QRectF(area.left(), area.bottom()+20, area.width(), 16),
               Qt::AlignHCenter, "Frequency (Hz)");

    // Axes
    p.setPen(QPen(CLR_GREY_DK(), 1.5));
    p.drawLine(QPointF(area.left(), area.top()),    QPointF(area.left(),  area.bottom()));
    p.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));

    if (!anyValid) {
        p.setPen(CLR_GREY_LT());
        QFont f; f.setPointSize(12); f.setItalic(true); p.setFont(f);
        p.drawText(area, Qt::AlignCenter,
                   "Set Rₑ, BL, mms, Qms, and Fc to see cone excursion.");
        return;
    }

    // Xmax / Xlim reference lines (dashed and dotted, per model)
    for (const auto &m : m_models) {
        if (!hasExcursionData(m)) continue;
        QColor c = m.color; c.setAlpha(130);
        p.setBrush(Qt::NoBrush);
        if (m.xmax_mm > 0 && m.xmax_mm <= yMax) {
            p.setPen(QPen(c, 1.2, Qt::DashLine));
            p.drawLine(QPointF(area.left(), yPx(m.xmax_mm)),
                       QPointF(area.right(), yPx(m.xmax_mm)));
        }
        if (m.xlim_mm > 0 && m.xlim_mm <= yMax) {
            p.setPen(QPen(c, 1.2, Qt::DotLine));
            p.drawLine(QPointF(area.left(), yPx(m.xlim_mm)),
                       QPointF(area.right(), yPx(m.xlim_mm)));
        }
    }

    // Curves
    auto drawCurve = [&](const BoxModel &m, bool active) {
        if (!hasExcursionData(m)) return;
        const double mp = modelPower(m);
        QPainterPath curve; bool first = true;
        for (int i = 0; i <= 500; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/500.0);
            const double x = coneDisplacement_mm(m, f, mp);
            if (!std::isfinite(x) || x > yMax * 1.5) continue;
            const QPointF pt(xPx(f), yPx(x));
            if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setPen(QPen(c, active ? 3.0 : 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);
    };

    p.setClipRect(area);
    for (int i = 0; i < m_models.size(); ++i)
        if (i != m_activeIdx) drawCurve(m_models[i], false);
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        drawCurve(m_models[m_activeIdx], true);
    p.setClipping(false);

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-185; int ly = int(area.top())+8;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasExcursionData(m)) continue;
            bool active = (i == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            QString label = m.name;
            if (m.xmax_mm > 0 || m.xlim_mm > 0) {
                label += "  [";
                if (m.xmax_mm > 0)
                    label += QString("Xmax %1").arg(m.xmax_mm, 0, 'f', 1);
                if (m.xmax_mm > 0 && m.xlim_mm > 0)
                    label += " / ";
                if (m.xlim_mm > 0)
                    label += QString("Xlim %1").arg(m.xlim_mm, 0, 'f', 1);
                label += " mm]";
            }
            p.drawText(QRect(lx+24, ly, 200, 14), Qt::AlignLeft|Qt::AlignVCenter, label);
            ly += 16;
        }
    }

    // Cursor overlay
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasExcursionData(m)) continue;
            const double x = coneDisplacement_mm(m, m_cursorFreq, modelPower(m));
            if (!std::isfinite(x)) continue;
            QString valStr = QString("%1 mm").arg(x, 0, 'f', 2);
            if (m.xmax_mm > 0 && x > 0)
                valStr += QString("  (%1% Xmax)").arg(x / m.xmax_mm * 100.0, 0, 'f', 0);
            if (m.xlim_mm > 0 && x > 0)
                valStr += QString("  (%1% Xlim)").arg(x / m.xlim_mm * 100.0, 0, 'f', 0);
            entries.append({m.color, i == m_activeIdx, m.name, yPx(x), valStr});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
}

void ExcursionPlot::mouseMoveEvent(QMouseEvent *e) { plotUpdateCursor(this, e, m_cursorFreq); }
void ExcursionPlot::leaveEvent(QEvent *)           { m_cursorFreq = -1.0; update(); }

// ════════════════════════════════════════════════════════════════════
//  PortVelocityPlot
// ════════════════════════════════════════════════════════════════════


PortVelocityPlot::PortVelocityPlot(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(400, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void PortVelocityPlot::setModels(const QList<BoxModel> &models, int activeIndex)
{ m_models = models; m_activeIdx = activeIndex; update(); }

void PortVelocityPlot::setPower(double watts) { m_power = watts; update(); }
void PortVelocityPlot::clear() { m_models.clear(); m_activeIdx = -1; update(); }

void PortVelocityPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 16, mt = 16, mb = 42;
    const QRectF area(ml, mt, width()-ml-mr, height()-mt-mb);
    p.fillRect(area, CLR_PLOT_BG());

    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);

    // Y range: scan all vented models
    double yMax = 1.0;
    bool anyValid = false;
    for (const auto &m : m_models) {
        if (!hasPortVelocityData(m)) continue;
        anyValid = true;
        const double mp = modelPower(m);
        for (int i = 0; i <= 80; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/80.0);
            const double v = portAirVelocity(m, f, mp);
            if (std::isfinite(v)) yMax = std::max(yMax, v);
        }
    }

    double yMin = 0.0;
    double niceStep;
    if (m_yMin.has_value() && m_yMax.has_value()) {
        yMin = *m_yMin; yMax = *m_yMax;
        const double rawStep = (yMax - yMin) / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(std::max(rawStep, 1e-9))));
        niceStep = std::ceil(rawStep / mag) * mag;
    } else {
        yMax *= 1.15;
        if (yMax < 1.0) yMax = 1.0;
        const double rawStep = yMax / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(std::max(rawStep, 1e-9))));
        niceStep = std::ceil(rawStep / mag) * mag;
        yMax = std::ceil(yMax / niceStep) * niceStep;
    }

    auto xPx = [&](double f) { return area.left() + area.width()*(std::log10(f)-lfMin)/(lfMax-lfMin); };
    auto yPx = [&](double v) { return area.top()  + area.height()*(yMax-v)/(yMax-yMin); };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = {20,30,50,70,100,200,300,500,700,1000};
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    const bool showDec = niceStep < 1.0;
    for (double v = yMin; v <= yMax+1e-9; v += niceStep)
        p.drawLine(QPointF(area.left(), yPx(v)), QPointF(area.right(), yPx(v)));

    // Axis labels
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = f>=1000 ? QString("%1k").arg(f/1000,0,'f',0) : QString::number(int(f));
        p.drawText(QRectF(xPx(f)-20, area.bottom()+2, 40, 14), Qt::AlignHCenter, lbl);
    }
    for (double v = yMin; v <= yMax+1e-9; v += niceStep) {
        const QString lbl = showDec ? QString::number(v,'f',1) : QString::number(int(std::round(v)));
        p.drawText(QRectF(0, yPx(v)-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, lbl);
    }

    // Reference line at the flare-dependent chuffing-onset velocity. Tracks the
    // active model's port flare (flared ports tolerate higher velocity); falls
    // back to the first valid model, then the sharp-port 17 m/s default.
    double chuffMs = 17.0;
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size()
        && hasPortVelocityData(m_models[m_activeIdx])) {
        chuffMs = chuffLimit(m_models[m_activeIdx].portFlare);
    } else {
        for (const auto &mm : m_models)
            if (hasPortVelocityData(mm)) { chuffMs = chuffLimit(mm.portFlare); break; }
    }
    if (chuffMs >= yMin && chuffMs <= yMax) {
        p.setPen(QPen(Theme::instance().statusError(), 1.0, Qt::DashLine));
        p.drawLine(QPointF(area.left(), yPx(chuffMs)), QPointF(area.right(), yPx(chuffMs)));
        QFont rf; rf.setPointSize(7); p.setFont(rf);
        p.setPen(Theme::instance().statusError());
        p.drawText(QRectF(area.left()+4, yPx(chuffMs)-13, 90, 12),
                   Qt::AlignLeft|Qt::AlignVCenter,
                   QString("%1 m/s limit").arg(int(std::round(chuffMs))));
    }

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Port Air Velocity  (m/s peak)");
    p.restore();
    p.drawText(QRectF(area.left(), area.bottom()+20, area.width(), 16),
               Qt::AlignHCenter, "Frequency (Hz)");

    // Axes
    p.setPen(QPen(CLR_GREY_DK(), 1.5));
    p.drawLine(QPointF(area.left(), area.top()),    QPointF(area.left(),  area.bottom()));
    p.drawLine(QPointF(area.left(), area.bottom()), QPointF(area.right(), area.bottom()));

    if (!anyValid) {
        p.setPen(CLR_GREY_LT());
        QFont f; f.setPointSize(12); f.setItalic(true); p.setFont(f);
        p.drawText(area, Qt::AlignCenter, "Select a vented enclosure to see port velocity.");
        return;
    }

    // Curves
    auto drawCurve = [&](const BoxModel &m, bool active) {
        if (!hasPortVelocityData(m)) return;
        const double mp = modelPower(m);
        QPainterPath curve; bool first = true;
        for (int i = 0; i <= 500; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/500.0);
            const double v = portAirVelocity(m, f, mp);
            if (!std::isfinite(v)) continue;
            const QPointF pt(xPx(f), yPx(v));
            if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setPen(QPen(c, active ? 3.0 : 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);
    };

    p.setClipRect(area);
    for (int i = 0; i < m_models.size(); ++i)
        if (i != m_activeIdx) drawCurve(m_models[i], false);
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        drawCurve(m_models[m_activeIdx], true);
    p.setClipping(false);

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-180; int ly = int(area.top())+8;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasPortVelocityData(m)) continue;
            bool active = (i == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx+24, ly, 150, 14), Qt::AlignLeft|Qt::AlignVCenter, m.name);
            ly += 16;
        }
    }

    // Cursor overlay
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasPortVelocityData(m)) continue;
            const double v = portAirVelocity(m, m_cursorFreq, modelPower(m));
            if (!std::isfinite(v)) continue;
            entries.append({m.color, i == m_activeIdx, m.name,
                            yPx(v), QString("%1 m/s").arg(v, 0, 'f', 2)});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
}

void PortVelocityPlot::mouseMoveEvent(QMouseEvent *e) { plotUpdateCursor(this, e, m_cursorFreq); }
void PortVelocityPlot::leaveEvent(QEvent *)           { m_cursorFreq = -1.0; update(); }

// ════════════════════════════════════════════════════════════════════
//  EnclosureWidget
// ════════════════════════════════════════════════════════════════════
static QLabel *mkFormLabel(const QString &t)
{
    auto *l = new QLabel(t);
    l->setObjectName("FormCaption");
    return l;
}

static QLabel *mkValue(const QString &init = QStringLiteral("–"))
{
    auto *l = new QLabel(init);
    l->setStyleSheet(themed(
        "color:%text%; font-family:'IBM Plex Mono',monospace;"
        "font-size:9.5pt;"));
    return l;
}

static QLabel *mkBigValue()
{
    auto *l = new QLabel("–");
    l->setTextFormat(Qt::RichText);
    l->setObjectName("ResultValue");
    return l;
}

static QDoubleSpinBox *mkParamSpin(double lo, double hi, int dec,
                                    double step, const QString &sfx)
{
    auto *s = new NoScrollSpinBox;
    s->setRange(lo, hi);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setSuffix(sfx);
    s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return s;
}

static QDoubleSpinBox *mkWheelSpin(double lo, double hi, int dec,
                                    double step, const QString &sfx)
{
    auto *s = new WheelWhenFocusedSpinBox;
    s->setRange(lo, hi);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setSuffix(sfx);
    s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return s;
}

static QPixmap colorSwatch(const QColor &c, int sz = 12)
{
    QPixmap px(sz, sz);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawEllipse(1, 1, sz - 2, sz - 2);
    return px;
}

EnclosureWidget::EnclosureWidget(DriverDatabase *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    buildUi();
    refreshDriverList();
    // T/S spinboxes carry an inline stylesheet for the lock/unlock visual,
    // so a live theme switch must re-run that styling.
    connect(&Theme::instance(), &Theme::themeChanged, this, [this] {
        if (m_tsLocked) lockTsFields(); else unlockTsFields();
    });
}

void EnclosureWidget::applyPerDriverMode(bool on)
{
    // Setting the "Per Driver" radio also drives the paired "Total" radio
    // (mutual exclusion inside the QButtonGroup), so a single setChecked
    // per pair is enough to keep both halves of each tab in sync.
    for (auto *rb : {m_perDriverPower, m_perDriverPowerVolt, m_perDriverPowerExc, m_perDriverPowerPV}) {
        if (rb) { rb->blockSignals(true); rb->setChecked(on); rb->blockSignals(false); }
    }
    if (m_splPlot) m_splPlot->setPerDriverMode(on);
    if (m_vpPlot)  m_vpPlot ->setPerDriverMode(on);
    if (m_excPlot) m_excPlot->setPerDriverMode(on);
    if (m_pvPlot)  m_pvPlot ->setPerDriverMode(on);
}

QColor EnclosureWidget::nextColor() const
{
    // Pick the first palette color not already in use
    const int n = paletteSize();
    for (int i = 0; i < n; ++i) {
        const QColor c = paletteColor(i);
        bool used = false;
        for (const auto &m : m_models)
            if (m.color == c) { used = true; break; }
        if (!used) return c;
    }
    return paletteColor(m_models.size());
}

void EnclosureWidget::buildUi()
{
    // ── LEFT PANEL: model list + preset controls ─────────────────
    auto *leftPanel = new QWidget;
    leftPanel->setObjectName("encLeftPanel");
    leftPanel->setMinimumWidth(150);
    leftPanel->setMaximumWidth(400);
    auto *leftVb = new QVBoxLayout(leftPanel);
    leftVb->setContentsMargins(8, 10, 8, 8);
    leftVb->setSpacing(8);

    auto *leftTitle = new QLabel("MODELS");
    leftTitle->setStyleSheet(themed(
        "font-family:'IBM Plex Sans',sans-serif;"
        "font-size:8.5pt; font-weight:600; color:%accent%;"
        "letter-spacing:2px; padding:2px 0 6px 0;"
        "border-bottom:1px solid %border%;"));
    leftVb->addWidget(leftTitle);

    m_modelList = new QListWidget;
    // Inherit theme QSS — no override
    m_modelList->setDragDropMode(QAbstractItemView::InternalMove);
    m_modelList->setDefaultDropAction(Qt::MoveAction);
    m_modelList->setSelectionMode(QAbstractItemView::SingleSelection);
    leftVb->addWidget(m_modelList, 1);

    // Model buttons — compact secondary style
    {
        auto *hb = new QHBoxLayout;
        hb->setSpacing(4);
        auto mkBtn = [](const QString &text, bool primary) {
            auto *b = new QPushButton(text);
            const QString accent = primary ? "#D97706" : "#3A4150";
            const QString hover  = primary ? "#F59E0B" : "#D97706";
            b->setStyleSheet(themed(QString(
                "QPushButton{background:transparent;color:%text%;"
                "border:1px solid %1;border-radius:0px;"
                "padding:5px 10px;font-size:8pt;letter-spacing:0.5px;}"
                "QPushButton:hover{border-color:%2;color:%accentLt%;}"
                "QPushButton:pressed{background:%1;color:%page%;}"
            ).arg(accent, hover)));
            return b;
        };
        auto *btnAdd    = mkBtn("+ ADD",     true);
        auto *btnRemove = mkBtn("− REMOVE",  false);
        auto *btnDup    = mkBtn("DUPLICATE", false);
        auto *btnRename = mkBtn("RENAME",    false);
        hb->addWidget(btnAdd);
        hb->addWidget(btnRemove);
        auto *hb2 = new QHBoxLayout;
        hb2->setSpacing(4);
        hb2->addWidget(btnDup);
        hb2->addWidget(btnRename);
        leftVb->addLayout(hb);
        leftVb->addLayout(hb2);

        connect(btnAdd,    &QPushButton::clicked, this, &EnclosureWidget::onAddModel);
        connect(btnDup,    &QPushButton::clicked, this, &EnclosureWidget::onDuplicateModel);
        connect(btnRemove, &QPushButton::clicked, this, &EnclosureWidget::onRemoveModel);
        connect(btnRename, &QPushButton::clicked, this, &EnclosureWidget::onRenameModel);
    }

    // Separator
    auto *sep = new QFrame; sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(themed("color:%border%; background:%border%; max-height:1px;"));
    leftVb->addWidget(sep);

    // File save/load section
    {
        auto *fileLbl = new QLabel("FILES");
        fileLbl->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif;"
            "font-size:8.5pt; font-weight:600; color:%accent%;"
            "letter-spacing:2px; padding:2px 0 4px 0;"));
        leftVb->addWidget(fileLbl);

        auto mkBtn = [](const QString &text) {
            auto *b = new QPushButton(text);
            b->setStyleSheet(themed(
                "QPushButton{background:transparent;color:%text2%;"
                "border:1px solid %border%;border-radius:0px;"
                "padding:5px 10px;font-size:8pt;text-align:left;}"
                "QPushButton:hover{border-color:%accent%;color:%text%;}"
                "QPushButton:pressed{background:%input%;}"));
            return b;
        };
        auto *btnSaveModel   = mkBtn("Save Model  ·  .tsbox");
        auto *btnLoadModel   = mkBtn("Load Model  ·  .tsbox");
        auto *btnSaveProject = mkBtn("Save Project  ·  .tsproj");
        auto *btnLoadProject = mkBtn("Load Project  ·  .tsproj");
        leftVb->addWidget(btnSaveModel);
        leftVb->addWidget(btnLoadModel);

        auto *sep2 = new QFrame; sep2->setFrameShape(QFrame::HLine);
        sep2->setStyleSheet(themed("color:%border%; background:%border%; max-height:1px;"));
        leftVb->addWidget(sep2);

        leftVb->addWidget(btnSaveProject);
        leftVb->addWidget(btnLoadProject);

        m_btnExportPdf = mkBtn("Export PDF  ·  .pdf");
        m_btnExportPdf->setToolTip("Export the current project as a PDF report");
        leftVb->addWidget(m_btnExportPdf);

        connect(btnSaveModel,   &QPushButton::clicked, this, &EnclosureWidget::onSaveModel);
        connect(btnLoadModel,   &QPushButton::clicked, this, &EnclosureWidget::onLoadModel);
        connect(btnSaveProject, &QPushButton::clicked, this, &EnclosureWidget::onSaveProject);
        connect(btnLoadProject, &QPushButton::clicked, this, &EnclosureWidget::onLoadProject);
        connect(m_btnExportPdf,  &QPushButton::clicked, this, &EnclosureWidget::onExportPdf);
    }

    leftVb->addStretch();

    m_statusLbl = new QLabel;
    m_statusLbl->setWordWrap(true);
    m_statusLbl->setStyleSheet(themed(
        "color:%error%; font-family:'IBM Plex Mono',monospace; font-size:8pt;"));
    leftVb->addWidget(m_statusLbl);

    // ── RIGHT PANEL: plot tabs (top) + params (bottom) ──────────
    m_splPlot  = new ResponsePlot;
    m_gdPlot   = new GroupDelayPlot;
    m_vpPlot   = new VoltagePlot;
    m_excPlot  = new ExcursionPlot;
    m_pvPlot   = new PortVelocityPlot;

    // Helper: make a power-spinbox row (shared across Voltage, Excursion, Port Velocity tabs)
    auto mkPowerSpin = [&]() -> QDoubleSpinBox * {
        auto *s = new NoScrollSpinBox;
        s->setRange(0.1, 10000.0);
        s->setDecimals(1);
        s->setSingleStep(1.0);
        s->setValue(100.0);
        s->setSuffix(" W");
        s->setFixedWidth(110);
        return s;
    };

    // Helper: build a "Per Driver" / "Total" radio pair, return the "Per Driver"
    // radio. The pair shares a QButtonGroup so they're mutually exclusive; the
    // returned pointer is the canonical "is per-driver mode on?" toggle source.
    auto mkPerDriverRadios = [this](QHBoxLayout *into) -> QRadioButton * {
        auto *perDrv = new QRadioButton("Per Driver");
        auto *total  = new QRadioButton("Total");
        perDrv->setChecked(true);
        auto *grp = new QButtonGroup(this);
        grp->addButton(perDrv, 1);
        grp->addButton(total,  0);
        const QString style = themed(
            "QRadioButton{color:%accent%; font-family:'IBM Plex Sans',sans-serif;"
            " font-size:8pt; padding-left:8px;}");
        perDrv->setStyleSheet(style);
        total ->setStyleSheet(style);
        into->addWidget(perDrv);
        into->addWidget(total);
        return perDrv;
    };

    // Voltage tab: power control + plot
    auto *voltTab = new QWidget;
    {
        auto *vb = new QVBoxLayout(voltTab);
        vb->setContentsMargins(6, 6, 6, 0);
        vb->setSpacing(4);
        auto *hb = new QHBoxLayout;
        auto *pwrLbl = new QLabel("APPLIED POWER");
        pwrLbl->setStyleSheet(themed(
            "color:%accent%; font-family:'IBM Plex Sans',sans-serif;"
            "font-weight:600; font-size:8.5pt; letter-spacing:1.5px;"
            "padding-right:8px;"));
        m_power = mkPowerSpin();
        hb->addWidget(pwrLbl); hb->addWidget(m_power);
        m_perDriverPowerVolt = mkPerDriverRadios(hb);
        hb->addStretch();
        vb->addLayout(hb);
        vb->addWidget(m_vpPlot, 1);
        connect(m_power, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            m_vpPlot->setPower(v);
            m_excPlot->setPower(v);
            m_pvPlot->setPower(v);
            m_splPlot->setPower(v);
            for (auto *s : {m_excPower, m_pvPower, m_splPower}) if (s) {
                s->blockSignals(true); s->setValue(v); s->blockSignals(false);
            }
        });
    }

    // Excursion tab: same power control + excursion plot
    auto *excTab = new QWidget;
    {
        auto *vb = new QVBoxLayout(excTab);
        vb->setContentsMargins(6, 6, 6, 0);
        vb->setSpacing(4);
        auto *hb = new QHBoxLayout;
        auto *pwrLbl = new QLabel("APPLIED POWER");
        pwrLbl->setStyleSheet(themed(
            "color:%accent%; font-family:'IBM Plex Sans',sans-serif;"
            "font-weight:600; font-size:8.5pt; letter-spacing:1.5px;"
            "padding-right:8px;"));
        m_excPower = mkPowerSpin();
        hb->addWidget(pwrLbl); hb->addWidget(m_excPower);
        m_perDriverPowerExc = mkPerDriverRadios(hb);
        hb->addStretch();
        vb->addLayout(hb);
        vb->addWidget(m_excPlot, 1);
        connect(m_excPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            m_excPlot->setPower(v);
            m_vpPlot->setPower(v);
            m_pvPlot->setPower(v);
            m_splPlot->setPower(v);
            for (auto *s : {m_power, m_pvPower, m_splPower}) if (s) {
                s->blockSignals(true); s->setValue(v); s->blockSignals(false);
            }
        });
    }

    // Port Velocity tab: same power control + port velocity plot
    auto *pvTab = new QWidget;
    {
        auto *vb = new QVBoxLayout(pvTab);
        vb->setContentsMargins(6, 6, 6, 0);
        vb->setSpacing(4);
        auto *hb = new QHBoxLayout;
        auto *pwrLbl = new QLabel("APPLIED POWER");
        pwrLbl->setStyleSheet(themed(
            "color:%accent%; font-family:'IBM Plex Sans',sans-serif;"
            "font-weight:600; font-size:8.5pt; letter-spacing:1.5px;"
            "padding-right:8px;"));
        m_pvPower = mkPowerSpin();
        hb->addWidget(pwrLbl); hb->addWidget(m_pvPower);
        m_perDriverPowerPV = mkPerDriverRadios(hb);
        hb->addStretch();
        vb->addLayout(hb);
        vb->addWidget(m_pvPlot, 1);
        connect(m_pvPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            m_pvPlot->setPower(v);
            m_vpPlot->setPower(v);
            m_excPlot->setPower(v);
            m_splPlot->setPower(v);
            for (auto *s : {m_power, m_excPower, m_splPower}) if (s) {
                s->blockSignals(true); s->setValue(v); s->blockSignals(false);
            }
        });
    }

    // SPL tab: power control + plot
    auto *splTab = new QWidget;
    {
        auto *vb = new QVBoxLayout(splTab);
        vb->setContentsMargins(6, 6, 6, 0);
        vb->setSpacing(4);
        auto *hb = new QHBoxLayout;
        auto *pwrLbl = new QLabel("APPLIED POWER");
        pwrLbl->setStyleSheet(themed(
            "color:%accent%; font-family:'IBM Plex Sans',sans-serif;"
            "font-weight:600; font-size:8.5pt; letter-spacing:1.5px;"
            "padding-right:8px;"));
        m_splPower = mkPowerSpin();
        hb->addWidget(pwrLbl); hb->addWidget(m_splPower);
        m_perDriverPower = mkPerDriverRadios(hb);
        hb->addStretch();
        vb->addLayout(hb);
        vb->addWidget(m_splPlot, 1);
        connect(m_splPower, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            m_splPlot->setPower(v);
            m_vpPlot->setPower(v);
            m_excPlot->setPower(v);
            m_pvPlot->setPower(v);
            for (auto *s : {m_power, m_excPower, m_pvPower}) if (s) {
                s->blockSignals(true); s->setValue(v); s->blockSignals(false);
            }
        });
    }

    // Push the spinboxes' initial 100 W reading into the plots —
    // their internal m_power defaults are inconsistent (1 W for
    // VoltagePlot/ExcursionPlot, 100 W for PortVelocityPlot), so
    // without this the curves don't match the displayed power
    // until the user nudges the spin.
    {
        const double w = m_power->value();  // all four spins agree; per-driver not active yet (no model loaded)
        m_vpPlot ->setPower(w);
        m_excPlot->setPower(w);
        m_pvPlot ->setPower(w);
        m_splPlot->setPower(w);
    }

    // All per-driver radios sync together and update plot mode. We listen on
    // the "Per Driver" half — when the user picks "Total", QButtonGroup
    // exclusivity flips this one off and the lambda fires with on=false.
    for (auto *rb : {m_perDriverPower, m_perDriverPowerVolt, m_perDriverPowerExc, m_perDriverPowerPV}) {
        connect(rb, &QRadioButton::toggled, this, [this](bool on) { applyPerDriverMode(on); });
    }

    // Set initial per-driver mode on plots (checkboxes default to checked)
    m_splPlot->setPerDriverMode(true);
    m_vpPlot ->setPerDriverMode(true);
    m_excPlot->setPerDriverMode(true);
    m_pvPlot ->setPerDriverMode(true);

    m_plotTabs = new QTabWidget;
    m_plotTabs->setObjectName("encPlotTabs");
    m_plotTabs->addTab(splTab,    "SPL");           // index 0
    m_plotTabs->addTab(m_gdPlot,  "GROUP DELAY");   // index 1
    m_plotTabs->addTab(voltTab,   "POWER");         // index 2 (voltage + current)
    m_plotTabs->addTab(excTab,    "EXCURSION");     // index 3
    m_plotTabs->addTab(pvTab,     "PORT V");        // index 4
    m_plotTabs->setTabEnabled(4, false);              // enabled only for vented

    // Bottom parameter strip
    auto *paramPanel = new QWidget;
    paramPanel->setObjectName("encParamPanel");
    auto *paramMain  = new QHBoxLayout(paramPanel);
    paramMain->setContentsMargins(8, 6, 8, 6);
    paramMain->setSpacing(12);

    // Driver selection column
    {
        auto *col = new QVBoxLayout;
        col->setSpacing(4);
        auto *lbl = new QLabel("DRIVER");
        lbl->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:%accent%; letter-spacing:1.5px;"));
        m_driverCombo = new QComboBox;
        m_driverCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_driverCombo->setMinimumWidth(180);
        auto *btnReset = new QPushButton("Reset");
        btnReset->setStyleSheet(themed(
            "QPushButton{background:transparent;color:%text2%;"
            "border:1px solid %border2%;border-radius:0px;"
            "padding:3px 10px;font-size:8pt;}"
            "QPushButton:hover{border-color:%accent%;color:%accentLt%;}"));

        m_btnViewDriver = new QPushButton("View Parameters…");
        m_btnViewDriver->setStyleSheet(themed(
            "QPushButton{background:%accent%;color:%page%;"
            "border:1px solid %accent%;border-radius:0px;"
            "padding:3px 10px;font-size:8pt;font-weight:600;}"
            "QPushButton:hover{background:%accentHov%;border-color:%accentHov%;}"
            "QPushButton:disabled{background:transparent;color:%disabled%;border-color:%border%;}"));
        m_btnViewDriver->setEnabled(false);

        // Number of drivers row
        auto *ndRow = new QHBoxLayout;
        ndRow->setSpacing(4);
        ndRow->addWidget(mkFormLabel("# Drivers:"));
        m_numDrivers = new NoScrollIntSpinBox;
        m_numDrivers->setRange(1, 16);
        m_numDrivers->setValue(1);
        m_numDrivers->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        m_numDrivers->setFixedWidth(55);
        ndRow->addWidget(m_numDrivers);
        m_wiringGroup = new QButtonGroup(this);
        m_wiringBtnSeries   = new QRadioButton("Series");
        m_wiringBtnParallel = new QRadioButton("Parallel");
        m_wiringBtnSeparate = new QRadioButton("Separate");
        m_wiringBtnSeries->setChecked(true);
        m_wiringGroup->addButton(m_wiringBtnSeries,   0);
        m_wiringGroup->addButton(m_wiringBtnParallel, 1);
        m_wiringGroup->addButton(m_wiringBtnSeparate, 2);
        connect(m_wiringGroup, &QButtonGroup::idClicked,
                this, &EnclosureWidget::onParamChanged);
        const QString kWiringStyle =
            "color:#8A8579; font-family:'IBM Plex Mono',monospace; font-size:8pt; padding-left:4px;";
        for (auto *btn : {m_wiringBtnSeries, m_wiringBtnParallel, m_wiringBtnSeparate}) {
            btn->setStyleSheet(kWiringStyle);
            ndRow->addWidget(btn);
        }
        ndRow->addStretch();

        m_vcTypeLbl = new QLabel("SVC");
        m_vcTypeLbl->setStyleSheet(themed(
            "color:%muted%; font-family:'IBM Plex Mono',monospace;"
            "font-size:8.5pt; letter-spacing:1px;"));

        // Added cone mass row (e.g. felt rings, mass-loading test)
        auto *amRow = new QHBoxLayout;
        amRow->setSpacing(4);
        auto *amLbl = mkFormLabel("Added m:");
        amLbl->setToolTip(
            "Extra cone mass (e.g. felt ring, mass-loading test).\n"
            "Shifts fₛ, Qts, Qes, Qms; Vas / Cms / BL / Sd / Rₑ unchanged.");
        amRow->addWidget(amLbl);
        m_inAddedMass = mkParamSpin(0.0, 999.0, 2, 0.5, " g");
        m_inAddedMass->setFixedWidth(82);
        m_inAddedMass->setToolTip(amLbl->toolTip());
        amRow->addWidget(m_inAddedMass);
        amRow->addStretch();

        m_addedMassEffLbl = new QLabel;
        m_addedMassEffLbl->setStyleSheet(
            "color:#7DD3FC; font-family:'IBM Plex Mono',monospace; font-size:8pt;");
        m_addedMassEffLbl->setVisible(false);

        // Alignment buttons (visible based on enclosure type)
        auto mkAlignBtn = [](const QString &t) {
            auto *b = new QPushButton(t);
            b->setFixedHeight(22);
            b->setStyleSheet(themed(
                "QPushButton{background:transparent;color:#C4B5FD;"
                "border:1px solid %grid%;border-radius:0px;"
                "font-family:'IBM Plex Sans',sans-serif;font-size:8pt;"
                "letter-spacing:0.5px;padding:0 8px;}"
                "QPushButton:hover{border-color:#C4B5FD;color:%text%;"
                "background:rgba(196,181,253,0.08);}"
                "QPushButton:pressed{background:#C4B5FD;color:%page%;}"));
            return b;
        };
        m_btnBessel = mkAlignBtn("Bessel");
        m_btnB2     = mkAlignBtn("Butterworth");
        m_btnB4     = mkAlignBtn("B4 vented");
        m_btnB4->setVisible(false);
        connect(m_btnBessel, &QPushButton::clicked, this, &EnclosureWidget::onAlignBessel);
        connect(m_btnB2,     &QPushButton::clicked, this, &EnclosureWidget::onAlignB2);
        connect(m_btnB4,     &QPushButton::clicked, this, &EnclosureWidget::onAlignB4);

        // Recommended sealed enclosure label (visible when sealed type selected)
        m_lblOptSealed = new QLabel;
        m_lblOptSealed->setStyleSheet(
            "color:#7DD3FC; font-family:'IBM Plex Mono',monospace; font-size:8pt;");
        m_lblOptSealed->setWordWrap(true);
        m_lblOptSealed->setVisible(false);

        col->addWidget(lbl);
        col->addWidget(m_driverCombo);
        col->addWidget(btnReset);
        col->addWidget(m_btnViewDriver);
        col->addLayout(ndRow);
        col->addWidget(m_vcTypeLbl);
        col->addLayout(amRow);
        col->addWidget(m_addedMassEffLbl);
        col->addWidget(m_btnBessel);
        col->addWidget(m_btnB2);
        col->addWidget(m_btnB4);
        col->addWidget(m_lblOptSealed);
        col->addStretch();
        paramMain->addLayout(col);

        connect(m_driverCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EnclosureWidget::onDriverChanged);
        connect(btnReset, &QPushButton::clicked,
                this, &EnclosureWidget::onResetToDriver);
        connect(m_btnViewDriver, &QPushButton::clicked,
                this, &EnclosureWidget::onViewDriverParams);
        connect(m_numDrivers, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int){ onParamChanged(); });
        connect(m_inAddedMass, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &EnclosureWidget::onParamChanged);
    }

    // Editable params column
    {
        auto *form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(3);
        form->setContentsMargins(0,0,0,0);

        auto addSpin = [&](const QString &lbl, QDoubleSpinBox *&out,
                           double lo, double hi, int dec, double step,
                           const QString &sfx) {
            out = mkParamSpin(lo, hi, dec, step, sfx);
            form->addRow(mkFormLabel(lbl), out);
        };

        addSpin("fₛ:",  m_inFs,  1.0,  2000.0, 2, 0.1,  " Hz");
        addSpin("Vas:", m_inVas, 0.01, 99999.0, 2, 0.1,  " L");
        addSpin("Qts:", m_inQts, 0.01, 50.0,    3, 0.01, "");
        addSpin("Qes:", m_inQes, 0.01, 50.0,    3, 0.01, "");

        paramMain->addLayout(form);

        for (auto *spin : {m_inFs, m_inVas, m_inQts, m_inQes})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &EnclosureWidget::onParamChanged);

        // Install event filter for double-click unlock on T/S params
        for (auto *s : {m_inFs, m_inVas, m_inQts, m_inQes})
            if (auto *le = s->findChild<QLineEdit*>())
                le->installEventFilter(this);
    }

    // Secondary driver params column (locked spinboxes)
    {
        auto *form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(3);
        form->setContentsMargins(0,0,0,0);

        struct SecParam { const char *lbl; QDoubleSpinBox *&out;
                         double lo, hi; int dec; double step; const char *sfx; };
        SecParam sec[] = {
            {"Qms:", m_dpQms, 0.01, 500.0,  3, 0.1,  ""},
            {"Rₑ:",  m_dpRe,  0.01, 100.0,  2, 0.1,  " Ω"},
            {"mms:", m_dpMms, 0.01, 999.0,  2, 0.1,  " g"},
            {"BL:",  m_dpBL,  0.01, 200.0,  2, 0.1,  " Tm"},
            {"Sᵈ:",  m_dpSd,  0.1,  9999.0, 1, 1.0,  " cm²"},
        };
        for (auto &p : sec) {
            p.out = mkParamSpin(p.lo, p.hi, p.dec, p.step, p.sfx);
            auto *l = mkFormLabel(p.lbl);  // FormCaption objectName carries QSS
            p.out->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            form->addRow(l, p.out);
            if (auto *le = p.out->findChild<QLineEdit*>())
                le->installEventFilter(this);
            connect(p.out, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &EnclosureWidget::onParamChanged);
        }
        paramMain->addLayout(form);

        // All driver param spinboxes are now constructed — lock them
        lockTsFields();
    }

    paramMain->addStretch();

    // ── CHAMBERS TAB ──────────────────────────────────────────────────
    auto *chambersPanel = new QWidget;
    chambersPanel->setObjectName("encChambersPanel");
    auto *chambersMain = new QHBoxLayout(chambersPanel);
    chambersMain->setContentsMargins(8, 6, 8, 6);
    chambersMain->setSpacing(12);

    // Type + volumes column
    {
        auto *form = new QFormLayout;
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(3);
        form->setContentsMargins(0,0,0,0);

        // Enclosure type selector
        m_encType = new QComboBox;
        m_encType->addItems({"Sealed", "Vented (Ported)",
                             "Bandpass 4th-order", "Bandpass 6th-order",
                             "Infinite Baffle"});
        form->addRow(mkFormLabel("Type:"), m_encType);

        // Create all chamber widgets first, then add rows in desired order.

        // Rear / sealed volume
        m_volLabel = new QLabel("BOX VOL");
        m_volLabel->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:%accent%; letter-spacing:1.5px;"));
        m_volume = mkWheelSpin(1.0, 10000.0, 2, 1.0, " L");
        m_volume->setValue(40.0);

        // Rear port tuning (BP6 only — rear chamber is ported in 6th-order)
        m_fbRearBpLabel = new QLabel("REAR fb");
        m_fbRearBpLabel->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:%accent%; letter-spacing:1.5px;"));
        m_inFbRearBp = mkWheelSpin(1.0, 1000.0, 1, 0.5, " Hz");
        m_inFbRearBp->setValue(35.0);

        m_qlRearBpLabel = new QLabel("REAR QL");
        m_qlRearBpLabel->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:%accent%; letter-spacing:1.5px;"));
        m_inQLRearBp = mkWheelSpin(1.0, 30.0, 2, 0.5, "");
        m_inQLRearBp->setValue(7.0);

        // Bandpass front-chamber rows
        m_volFrontLabel = new QLabel("FRONT VOL");
        m_volFrontLabel->setStyleSheet(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:#7DD3FC; letter-spacing:1.5px;");
        m_volumeFront = mkWheelSpin(1.0, 10000.0, 2, 1.0, " L");
        m_volumeFront->setValue(40.0);

        m_fbFrontLabel = new QLabel("FRONT fb");
        m_fbFrontLabel->setStyleSheet(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:#7DD3FC; letter-spacing:1.5px;");
        m_inFbFront = mkWheelSpin(1.0, 1000.0, 1, 0.5, " Hz");
        m_inFbFront->setValue(60.0);

        m_qlFrontLabel = new QLabel("FRONT QL");
        m_qlFrontLabel->setStyleSheet(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:#7DD3FC; letter-spacing:1.5px;");
        m_inQLFront = mkWheelSpin(1.0, 30.0, 2, 0.5, "");
        m_inQLFront->setValue(7.0);

        // Vented fb/QL (mirrors Port tab, visible only for vented)
        m_chambersFbLabel = new QLabel("fb");
        m_chambersFbLabel->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:%accent%; letter-spacing:1.5px;"));
        m_chambersFb = mkWheelSpin(1.0, 500.0, 1, 1.0, " Hz");
        m_chambersFb->setValue(35.0);

        m_chambersQLLabel = new QLabel("QL");
        m_chambersQLLabel->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:600;"
            "font-size:8.5pt; color:%accent%; letter-spacing:1.5px;"));
        m_chambersQL = mkWheelSpin(0.5, 50.0, 1, 0.1, "");
        m_chambersQL->setValue(7.0);

        // Layout order: rear vol → vented fb/QL → rear fb/QL (BP6) → front vol → front fb/QL
        form->addRow(m_volLabel,         m_volume);
        form->addRow(m_chambersFbLabel,  m_chambersFb);
        form->addRow(m_chambersQLLabel,  m_chambersQL);
        form->addRow(m_fbRearBpLabel, m_inFbRearBp);
        form->addRow(m_qlRearBpLabel, m_inQLRearBp);
        form->addRow(m_volFrontLabel, m_volumeFront);
        form->addRow(m_fbFrontLabel,  m_inFbFront);
        form->addRow(m_qlFrontLabel,  m_inQLFront);

        for (QWidget *w : {static_cast<QWidget*>(m_volFrontLabel),
                           static_cast<QWidget*>(m_volumeFront),
                           static_cast<QWidget*>(m_fbFrontLabel),
                           static_cast<QWidget*>(m_inFbFront),
                           static_cast<QWidget*>(m_qlFrontLabel),
                           static_cast<QWidget*>(m_inQLFront),
                           static_cast<QWidget*>(m_fbRearBpLabel),
                           static_cast<QWidget*>(m_inFbRearBp),
                           static_cast<QWidget*>(m_qlRearBpLabel),
                           static_cast<QWidget*>(m_inQLRearBp)})
            w->setVisible(false);

        // IB ∞ glyph
        m_ibInfinityLbl = new QLabel(QString::fromUtf8("∞"));
        m_ibInfinityLbl->setAlignment(Qt::AlignCenter);
        m_ibInfinityLbl->setStyleSheet(themed(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:300;"
            "font-size:64pt; color:%accent%; padding:4px 0 4px 0;"));
        m_ibInfinityLbl->setToolTip("");
        m_ibInfinityLbl->setVisible(false);
        form->addRow(m_ibInfinityLbl);

        chambersMain->addLayout(form);

        for (auto *spin : {m_volume, m_volumeFront, m_inFbFront, m_inQLFront,
                           m_inFbRearBp, m_inQLRearBp})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &EnclosureWidget::onParamChanged);

        // Vented fb/QL in Chambers tab: sync to Port tab m_inFb/m_inQL (authoritative)
        connect(m_chambersFb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_inFb) { m_inFb->blockSignals(true); m_inFb->setValue(v); m_inFb->blockSignals(false); }
            onParamChanged();
        });
        connect(m_chambersQL, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_inQL) { m_inQL->blockSignals(true); m_inQL->setValue(v); m_inQL->blockSignals(false); }
            onParamChanged();
        });

        // m_inFbFront / m_inFbRearBp → keep Port tab m_inFb in sync for BP modes
        connect(m_inFbFront, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_activeIdx >= 0 && m_activeIdx < m_models.size() && isBP4(m_models[m_activeIdx]))
                if (m_inFb) { m_inFb->blockSignals(true); m_inFb->setValue(v); m_inFb->blockSignals(false); }
            if (m_inFbFrontPort) { m_inFbFrontPort->blockSignals(true); m_inFbFrontPort->setValue(v); m_inFbFrontPort->blockSignals(false); }
        });
        connect(m_inQLFront, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_activeIdx >= 0 && m_activeIdx < m_models.size() && isBP4(m_models[m_activeIdx]))
                if (m_inQL) { m_inQL->blockSignals(true); m_inQL->setValue(v); m_inQL->blockSignals(false); }
            if (m_inQLFrontPort) { m_inQLFrontPort->blockSignals(true); m_inQLFrontPort->setValue(v); m_inQLFrontPort->blockSignals(false); }
        });
        connect(m_inFbRearBp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_inFb) { m_inFb->blockSignals(true); m_inFb->setValue(v); m_inFb->blockSignals(false); }
        });
        connect(m_inQLRearBp, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_inQL) { m_inQL->blockSignals(true); m_inQL->setValue(v); m_inQL->blockSignals(false); }
        });

        // Hide new Chambers vented controls initially (revealed by onEncTypeChanged for vented)
        for (QWidget *w : {static_cast<QWidget*>(m_chambersFbLabel),
                           static_cast<QWidget*>(m_chambersFb),
                           static_cast<QWidget*>(m_chambersQLLabel),
                           static_cast<QWidget*>(m_chambersQL)})
            w->setVisible(false);

        connect(m_encType, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EnclosureWidget::onEncTypeChanged);
    }

    // Results column (moved here from Driver tab)
    {
        auto *grid = new QGridLayout;
        grid->setHorizontalSpacing(10);
        grid->setVerticalSpacing(4);
        grid->setContentsMargins(2,4,2,4);

        auto mkResultLbl = [&](const QString &lbl) {
            auto *l = new QLabel(lbl);
            l->setObjectName("ResultLabel");
            return l;
        };
        auto addResRow = [&](int r, const QString &lbl, QLabel *&out,
                             const QString &unit, QLabel *&lblOut) {
            lblOut = mkResultLbl(lbl);
            out = mkBigValue();
            auto *u = new QLabel(unit);
            u->setObjectName("ResultUnit");
            grid->addWidget(lblOut, r, 0);
            grid->addWidget(out,    r, 1);
            grid->addWidget(u,      r, 2);
        };
        QLabel *dummy = nullptr;
        addResRow(0, "α:",    m_resAlpha, "",    dummy);
        addResRow(1, "Fc:",   m_resFc,    "Hz",  m_resLblFc);
        addResRow(2, "Qtc:",  m_resQtc,   "",    m_resLblQtc);
        addResRow(3, "f₋₃:", m_resF3,    "Hz",  dummy);
        addResRow(4, "η₀:",  m_resEta,   "%",   dummy);
        addResRow(5, "SPL:",  m_resSpl,   "dB",  dummy);
        chambersMain->addLayout(grid);
    }
    chambersMain->addStretch();

    // ── PORT TAB ──────────────────────────────────────────────────
    m_portTabContent = new QWidget;
    {
        auto *vb = new QVBoxLayout(m_portTabContent);
        vb->setContentsMargins(6, 4, 6, 4);
        vb->setSpacing(4);

        // Context header: "REAR PORT" for BP6/vented, "FRONT PORT" for BP4 (hidden for others)
        m_portSectionHdr = new QLabel;
        m_portSectionHdr->setStyleSheet(
            "font-family:'IBM Plex Sans',sans-serif; font-weight:700;"
            "font-size:8pt; color:#6B7280; letter-spacing:2px; padding:0 0 2px 0;");
        m_portSectionHdr->setVisible(false);
        vb->addWidget(m_portSectionHdr);

        // Empty — port-tab combos inherit the global dark theme.
        const QString kComboSS = QString();

        // Helper: combo in HBox with stretch so it doesn't expand to fill width
        auto mkComboRow = [](QComboBox *cb) -> QWidget * {
            auto *w = new QWidget;
            auto *h = new QHBoxLayout(w);
            h->setContentsMargins(0,0,0,0);
            h->setSpacing(0);
            h->addWidget(cb);
            h->addStretch();
            return w;
        };

        // 3-column horizontal layout: [Tuning+Shape] | [Dimensions] | [Results]
        auto *cols = new QHBoxLayout;
        cols->setSpacing(0);
        cols->setContentsMargins(0,0,0,0);

        // ── Column 1: tuning, count, shape, flare ─────────────
        {
            auto *f = new QFormLayout;
            f->setLabelAlignment(Qt::AlignRight);
            f->setSpacing(2);
            f->setContentsMargins(0,0,0,0);

            m_inFb = mkWheelSpin(1.0, 500.0, 1, 1.0, " Hz");
            m_inFb->setValue(35.0);
            m_inQL = mkWheelSpin(0.5, 50.0, 1, 0.1, "");
            m_inQL->setValue(7.0);
            m_numPorts = new WheelWhenFocusedIntSpinBox;
            m_numPorts->setRange(1, 8);
            m_numPorts->setValue(1);
            m_numPorts->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            m_numPorts->setFixedWidth(55);

            m_portShape = new QComboBox;
            m_portShape->addItems({"Round", "Rectangular"});
            m_portShape->setStyleSheet(kComboSS);
            m_portShape->setSizeAdjustPolicy(QComboBox::AdjustToContents);

            m_portFlare = new QComboBox;
            m_portFlare->addItems({"Straight", "One end flared", "Both ends flared"});
            m_portFlare->setStyleSheet(kComboSS);
            m_portFlare->setSizeAdjustPolicy(QComboBox::AdjustToContents);
            m_portFlare->setToolTip(
                "Flared ends reduce turbulence. Length is measured from the\n"
                "midpoint of the flare radius on each flared end.");

            m_portFbLabel = mkFormLabel("fb:");
            f->addRow(m_portFbLabel, m_inFb);
            m_portQLLabel = mkFormLabel("QL:");
            f->addRow(m_portQLLabel, m_inQL);
            f->addRow(mkFormLabel("# Ports:"), m_numPorts);
            f->addRow(mkFormLabel("Shape:"),   mkComboRow(m_portShape));
            f->addRow(mkFormLabel("Flare:"),   mkComboRow(m_portFlare));

            auto *col1 = new QWidget;
            col1->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            col1->setLayout(f);
            cols->addWidget(col1);
        }

        // Vertical divider
        auto mkVDiv = [] {
            auto *d = new QFrame;
            d->setFrameShape(QFrame::VLine);
            d->setStyleSheet(themed("color:%border%; background:%border%; max-width:1px;"));
            return d;
        };
        cols->addWidget(mkVDiv());
        cols->addSpacing(6);

        // ── Column 2: shape-specific dimensions ────────────────
        {
            auto *col2 = new QWidget;
            col2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            auto *cv = new QVBoxLayout(col2);
            cv->setContentsMargins(0,0,0,0);
            cv->setSpacing(0);

            // Round section
            m_portDiamRow = new QWidget;
            {
                auto *f = new QFormLayout(m_portDiamRow);
                f->setLabelAlignment(Qt::AlignRight);
                f->setSpacing(2);
                f->setContentsMargins(0,0,0,0);

                m_inPortDiam = mkWheelSpin(10.0, 400.0, 0, 5.0, " mm");
                m_inPortDiam->setValue(75.0);
                m_inPortDiam->setToolTip("Inner (bore) diameter of the port tube");

                m_inPortWallThick = mkWheelSpin(0.5, 50.0, 1, 1.0, " mm");
                m_inPortWallThick->setValue(3.0);
                m_inPortWallThick->setToolTip("Wall thickness of the port tube");

                m_inPortInsert = mkParamSpin(0.0, 1000.0, 0, 5.0, " mm");
                m_inPortInsert->setValue(0.0);
                m_inPortInsert->setToolTip(
                    "How far the port tube protrudes into the box interior.\n"
                    "Used to calculate volume displaced inside the enclosure.");

                auto *insertSliderRow = new QWidget;
                {
                    auto *sh = new QHBoxLayout(insertSliderRow);
                    sh->setContentsMargins(0, 0, 0, 0);
                    sh->setSpacing(4);
                    m_portInsertSlider = new QSlider(Qt::Horizontal);
                    m_portInsertSlider->setRange(0, 100);
                    m_portInsertSlider->setValue(0);
                    m_portInsertSlider->setToolTip("Insertion depth as % of computed port length");
                    m_portInsertPctLbl = new QLabel("0%");
                    m_portInsertPctLbl->setFixedWidth(32);
                    m_portInsertPctLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    sh->addWidget(m_portInsertSlider, 1);
                    sh->addWidget(m_portInsertPctLbl);
                }

                f->addRow(mkFormLabel("Inner Ø:"),         m_inPortDiam);
                f->addRow(mkFormLabel("Wall thickness:"),   m_inPortWallThick);
                f->addRow(mkFormLabel("Extends into box:"), m_inPortInsert);
                f->addRow(mkFormLabel(""),                  insertSliderRow);
            }
            cv->addWidget(m_portDiamRow);

            // Rect section (hidden by default)
            m_portRectRows = new QWidget;
            m_portRectRows->setVisible(false);
            {
                auto *f = new QFormLayout(m_portRectRows);
                f->setLabelAlignment(Qt::AlignRight);
                f->setSpacing(2);
                f->setContentsMargins(0,0,0,0);

                auto *whWidget = new QWidget;
                {
                    auto *h = new QHBoxLayout(whWidget);
                    h->setContentsMargins(0,0,0,0);
                    h->setSpacing(4);
                    m_inPortW = mkWheelSpin(10.0, 1000.0, 0, 1.0, " mm");
                    m_inPortW->setValue(100.0);
                    m_inPortH = mkWheelSpin(10.0, 1000.0, 0, 1.0, " mm");
                    m_inPortH->setValue(50.0);
                    h->addWidget(new QLabel("W:"));
                    h->addWidget(m_inPortW);
                    h->addWidget(new QLabel("H:"));
                    h->addWidget(m_inPortH);
                    h->addStretch();
                }
                f->addRow(mkFormLabel("W × H:"), whWidget);

                m_portWalls = new QComboBox;
                m_portWalls->addItems({
                    "0 – free-standing",
                    "1 – 1 wall",
                    "2 – 2 walls (edge)",
                    "3 – 3 walls (corner)"
                });
                m_portWalls->setStyleSheet(kComboSS);
                m_portWalls->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                m_portWalls->setToolTip(
                    "Number of enclosure walls the inner end of the port shares.\n"
                    "End corrections: 0→0.732·De, 1→0.8216·De, 2→0.8730·De, 3→0.8860·De");
                f->addRow(mkFormLabel("Port walls:"), mkComboRow(m_portWalls));
            }
            cv->addWidget(m_portRectRows);

            // Flare diagram note (hidden when straight)
            m_portFlareNote = new QLabel;
            m_portFlareNote->setFont(QFont("Monospace", 8));
            m_portFlareNote->setStyleSheet(themed(
                "color:%text2%; background:%input%;"
                "font-family:'IBM Plex Mono',monospace; font-size:8pt;"
                "padding:6px 8px; border-left:2px solid %accent%;"));
            m_portFlareNote->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            m_portFlareNote->setVisible(false);
            cv->addWidget(m_portFlareNote);

            // Extra surface area (optional, e.g. bracing) — always visible
            {
                auto *extraRow = new QWidget;
                auto *f = new QFormLayout(extraRow);
                f->setLabelAlignment(Qt::AlignRight);
                f->setSpacing(2);
                f->setContentsMargins(0, 4, 0, 0);
                m_inPortBraceSurf = mkParamSpin(0.0, 10000.0, 1, 10.0, " cm²");
                m_inPortBraceSurf->setValue(0.0);
                m_inPortBraceSurf->setSpecialValueText("–");
                m_inPortBraceSurf->setToolTip(
                    "Optional extra surface area (e.g. bracing obstructing the port).\n"
                    "Added to the computed inner surface area.");
                f->addRow(mkFormLabel("Bracing surf. area:"), m_inPortBraceSurf);
                cv->addWidget(extraRow);
            }

            cv->addStretch();
            cols->addWidget(col2);
        }

        cols->addSpacing(6);
        cols->addWidget(mkVDiv());
        cols->addSpacing(6);

        // ── Column 3: computed results ─────────────────────────
        {
            auto *g = new QGridLayout;
            g->setSpacing(2);
            g->setHorizontalSpacing(6);
            g->setContentsMargins(0,0,0,0);
            int row = 0;
            auto addR = [&](const QString &lbl, QLabel *&out) {
                out = mkValue("–");
                g->addWidget(mkFormLabel(lbl), row,   0, Qt::AlignRight);
                g->addWidget(out,              row++, 1, Qt::AlignLeft);
            };
            addR("Port area (each):",   m_portAreaLbl);
            addR("Inner surface area:", m_portSurfAreaLbl);
            addR("Port vol. (air):",    m_portVolInnerLbl);
            addR("Vol. in box:",        m_portVolDisplLbl);
            addR("Port length:",        m_portLenLbl);
            m_portLenLbl->setStyleSheet(themed(
                "color:%accentLt%; font-family:'IBM Plex Mono',monospace;"
                "font-size:11pt; font-weight:500;"));
            addR("(per port):",         m_portLenEachLbl);
            m_portLenEachLbl->setStyleSheet(themed(
                "color:%muted%; font-family:'IBM Plex Mono',monospace; font-size:8.5pt;"));
            addR("2nd harmonic:",       m_portF2HLbl);
            m_portF2HLbl->setToolTip(
                "2nd pipe harmonic of the port tube (open–open resonance):\n"
                "f₂H = c / Lp_eff  (= 2 × first pipe resonance)\n"
                "Above this frequency the port may produce pipe coloration.");

            auto *col3 = new QWidget;
            auto *cv3 = new QVBoxLayout(col3);
            cv3->setContentsMargins(0,0,0,0);
            cv3->addLayout(g);
            cv3->addStretch();
            col3->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            cols->addWidget(col3);
        }

        cols->addStretch();
        vb->addLayout(cols);

        // ── FRONT PORT SECTION (BP6 only) ─────────────────────────────
        m_frontPortSection = new QWidget;
        m_frontPortSection->setVisible(false);
        {
            auto *fpvb = new QVBoxLayout(m_frontPortSection);
            fpvb->setContentsMargins(0, 6, 0, 0);
            fpvb->setSpacing(4);

            auto *divLbl = new QLabel("FRONT PORT");
            divLbl->setStyleSheet(themed(
                "font-family:'IBM Plex Sans',sans-serif; font-weight:700;"
                "font-size:8pt; color:#7DD3FC; letter-spacing:2px;"
                "border-top:1px solid %border%; padding:4px 0 2px 0;"));
            fpvb->addWidget(divLbl);

            auto *fpcols = new QHBoxLayout;
            fpcols->setSpacing(0);
            fpcols->setContentsMargins(0,0,0,0);

            // Col 1: shape + # ports
            {
                auto *f = new QFormLayout;
                f->setLabelAlignment(Qt::AlignRight);
                f->setSpacing(2);
                f->setContentsMargins(0,0,0,0);

                m_portFrontShape = new QComboBox;
                m_portFrontShape->addItems({"Round", "Rectangular"});
                m_portFrontShape->setStyleSheet(kComboSS);
                m_portFrontShape->setSizeAdjustPolicy(QComboBox::AdjustToContents);

                m_numPortsFront = new WheelWhenFocusedIntSpinBox;
                m_numPortsFront->setRange(1, 8);
                m_numPortsFront->setValue(1);
                m_numPortsFront->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                m_numPortsFront->setFixedWidth(55);

                m_inFbFrontPort = mkWheelSpin(1.0, 500.0, 1, 1.0, " Hz");
                m_inFbFrontPort->setValue(60.0);
                m_inQLFrontPort = mkWheelSpin(0.5, 50.0, 1, 0.1, "");
                m_inQLFrontPort->setValue(7.0);
                f->addRow(mkFormLabel("Front fb:"), m_inFbFrontPort);
                f->addRow(mkFormLabel("Front QL:"), m_inQLFrontPort);
                f->addRow(mkFormLabel("Shape:"),   mkComboRow(m_portFrontShape));
                f->addRow(mkFormLabel("# Ports:"), m_numPortsFront);

                auto *col1fp = new QWidget;
                col1fp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
                col1fp->setLayout(f);
                fpcols->addWidget(col1fp);
            }

            fpcols->addWidget(mkVDiv());
            fpcols->addSpacing(6);

            // Col 2: round / rect dimensions
            {
                auto *col2fp = new QWidget;
                col2fp->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                auto *cv2fp = new QVBoxLayout(col2fp);
                cv2fp->setContentsMargins(0,0,0,0);
                cv2fp->setSpacing(0);

                m_portFrontDiamRow = new QWidget;
                {
                    auto *f = new QFormLayout(m_portFrontDiamRow);
                    f->setLabelAlignment(Qt::AlignRight);
                    f->setSpacing(2);
                    f->setContentsMargins(0,0,0,0);
                    m_inPortFrontDiam = mkWheelSpin(10.0, 400.0, 0, 5.0, " mm");
                    m_inPortFrontDiam->setValue(75.0);

                    m_inPortFrontInsert = mkParamSpin(0.0, 1000.0, 0, 5.0, " mm");
                    m_inPortFrontInsert->setValue(0.0);
                    m_inPortFrontInsert->setToolTip(
                        "How far the port tube protrudes into the box interior.\n"
                        "Used to calculate volume displaced inside the enclosure.");

                    auto *frontInsertSliderRow = new QWidget;
                    {
                        auto *sh = new QHBoxLayout(frontInsertSliderRow);
                        sh->setContentsMargins(0, 0, 0, 0);
                        sh->setSpacing(4);
                        m_portFrontInsertSlider = new QSlider(Qt::Horizontal);
                        m_portFrontInsertSlider->setRange(0, 100);
                        m_portFrontInsertSlider->setValue(0);
                        m_portFrontInsertSlider->setToolTip("Insertion depth as % of computed port length");
                        m_portFrontInsertPctLbl = new QLabel("0%");
                        m_portFrontInsertPctLbl->setFixedWidth(32);
                        m_portFrontInsertPctLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                        sh->addWidget(m_portFrontInsertSlider, 1);
                        sh->addWidget(m_portFrontInsertPctLbl);
                    }

                    f->addRow(mkFormLabel("Inner Ø:"),          m_inPortFrontDiam);
                    f->addRow(mkFormLabel("Extends into box:"),  m_inPortFrontInsert);
                    f->addRow(mkFormLabel(""),                   frontInsertSliderRow);
                }
                cv2fp->addWidget(m_portFrontDiamRow);

                m_portFrontRectRows = new QWidget;
                m_portFrontRectRows->setVisible(false);
                {
                    auto *f = new QFormLayout(m_portFrontRectRows);
                    f->setLabelAlignment(Qt::AlignRight);
                    f->setSpacing(2);
                    f->setContentsMargins(0,0,0,0);

                    auto *whw = new QWidget;
                    auto *whh = new QHBoxLayout(whw);
                    whh->setContentsMargins(0,0,0,0); whh->setSpacing(4);
                    m_inPortFrontW = mkWheelSpin(10.0, 1000.0, 0, 1.0, " mm");
                    m_inPortFrontW->setValue(100.0);
                    m_inPortFrontH = mkWheelSpin(10.0, 1000.0, 0, 1.0, " mm");
                    m_inPortFrontH->setValue(50.0);
                    whh->addWidget(new QLabel("W:")); whh->addWidget(m_inPortFrontW);
                    whh->addWidget(new QLabel("H:")); whh->addWidget(m_inPortFrontH);
                    whh->addStretch();
                    f->addRow(mkFormLabel("W × H:"), whw);

                    m_portFrontWalls = new QComboBox;
                    m_portFrontWalls->addItems({"0 – free-standing","1 – 1 wall",
                                                "2 – 2 walls (edge)","3 – 3 walls (corner)"});
                    m_portFrontWalls->setStyleSheet(kComboSS);
                    m_portFrontWalls->setSizeAdjustPolicy(QComboBox::AdjustToContents);
                    f->addRow(mkFormLabel("Port walls:"), mkComboRow(m_portFrontWalls));
                }
                cv2fp->addWidget(m_portFrontRectRows);

                // Bracing surface area (always visible, like rear port)
                {
                    auto *extraRow = new QWidget;
                    auto *f = new QFormLayout(extraRow);
                    f->setLabelAlignment(Qt::AlignRight);
                    f->setSpacing(2);
                    f->setContentsMargins(0, 4, 0, 0);
                    m_inPortFrontBraceSurf = mkParamSpin(0.0, 10000.0, 1, 10.0, " cm²");
                    m_inPortFrontBraceSurf->setValue(0.0);
                    m_inPortFrontBraceSurf->setSpecialValueText("–");
                    m_inPortFrontBraceSurf->setToolTip(
                        "Optional extra surface area (e.g. bracing obstructing the port).\n"
                        "Added to the computed inner surface area.");
                    f->addRow(mkFormLabel("Bracing surf. area:"), m_inPortFrontBraceSurf);
                    cv2fp->addWidget(extraRow);
                }

                cv2fp->addStretch();
                fpcols->addWidget(col2fp);
            }

            fpcols->addSpacing(6);
            fpcols->addWidget(mkVDiv());
            fpcols->addSpacing(6);

            // Col 3: computed results
            {
                auto *g = new QGridLayout;
                g->setSpacing(2); g->setHorizontalSpacing(6); g->setContentsMargins(0,0,0,0);
                int r = 0;
                auto addR2 = [&](const QString &lbl, QLabel *&out) {
                    out = mkValue("–");
                    g->addWidget(mkFormLabel(lbl), r,   0, Qt::AlignRight);
                    g->addWidget(out,              r++, 1, Qt::AlignLeft);
                };
                addR2("Port area (each):", m_portFrontAreaLbl);
                addR2("Inner surface area:", m_portFrontSurfAreaLbl);
                addR2("Vol. in box:",        m_portFrontVolDisplLbl);
                addR2("Port length:",        m_portFrontLenLbl);
                if (m_portFrontLenLbl)
                    m_portFrontLenLbl->setStyleSheet(themed(
                        "color:%accentLt%; font-family:'IBM Plex Mono',monospace;"
                        "font-size:11pt; font-weight:500;"));
                auto *col3fp = new QWidget;
                col3fp->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
                auto *cv3fp = new QVBoxLayout(col3fp);
                cv3fp->setContentsMargins(0,0,0,0);
                cv3fp->addLayout(g);
                cv3fp->addStretch();
                fpcols->addWidget(col3fp);
            }
            fpcols->addStretch();
            fpvb->addLayout(fpcols);
        }
        vb->addWidget(m_frontPortSection);

        vb->addStretch();

        // Wire up port tab signals
        for (auto *s : {m_inFb, m_inQL, m_inPortDiam, m_inPortWallThick,
                        m_inPortInsert, m_inPortW, m_inPortH, m_inPortBraceSurf})
            connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &EnclosureWidget::onParamChanged);

        // Port tab m_inFb/m_inQL → sync Chambers tab counterpart for BP modes
        connect(m_inFb, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
            const auto &m = m_models[m_activeIdx];
            if (isBP4(m) && m_inFbFront)
                { m_inFbFront->blockSignals(true); m_inFbFront->setValue(v); m_inFbFront->blockSignals(false); }
            else if (isBP6(m) && m_inFbRearBp)
                { m_inFbRearBp->blockSignals(true); m_inFbRearBp->setValue(v); m_inFbRearBp->blockSignals(false); }
            else if (isVented(m) && m_chambersFb)
                { m_chambersFb->blockSignals(true); m_chambersFb->setValue(v); m_chambersFb->blockSignals(false); }
        });
        connect(m_inQL, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
            const auto &m = m_models[m_activeIdx];
            if (isBP4(m) && m_inQLFront)
                { m_inQLFront->blockSignals(true); m_inQLFront->setValue(v); m_inQLFront->blockSignals(false); }
            else if (isBP6(m) && m_inQLRearBp)
                { m_inQLRearBp->blockSignals(true); m_inQLRearBp->setValue(v); m_inQLRearBp->blockSignals(false); }
            else if (isVented(m) && m_chambersQL)
                { m_chambersQL->blockSignals(true); m_chambersQL->setValue(v); m_chambersQL->blockSignals(false); }
        });
        // Slider ↔ spinbox sync for insert depth
        connect(m_portInsertSlider, &QSlider::valueChanged, this, [this](int pct) {
            if (!m_inPortInsert) return;
            const double max = m_inPortInsert->maximum();
            m_inPortInsert->blockSignals(true);
            m_inPortInsert->setValue(pct / 100.0 * max);
            m_inPortInsert->blockSignals(false);
            if (m_portInsertPctLbl) m_portInsertPctLbl->setText(QString("%1%").arg(pct));
            onParamChanged();
        });
        connect(m_inPortInsert, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, [this](double v) {
            if (!m_portInsertSlider) return;
            const double max = m_inPortInsert->maximum();
            const int pct = max > 0 ? qRound(v / max * 100.0) : 0;
            m_portInsertSlider->blockSignals(true);
            m_portInsertSlider->setValue(pct);
            m_portInsertSlider->blockSignals(false);
            if (m_portInsertPctLbl) m_portInsertPctLbl->setText(QString("%1%").arg(pct));
        });

        connect(m_portShape, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EnclosureWidget::onPortShapeChanged);
        connect(m_portWalls, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &EnclosureWidget::onParamChanged);
        connect(m_portFlare, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int){ onParamChanged(); });
        connect(m_numPorts, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int){ onParamChanged(); });

        // Front port section signals
        // fb/QL in front port section → sync Chambers tab m_inFbFront/m_inQLFront (authoritative)
        if (m_inFbFrontPort)
            connect(m_inFbFrontPort, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double v) {
                if (m_inFbFront) { m_inFbFront->blockSignals(true); m_inFbFront->setValue(v); m_inFbFront->blockSignals(false); }
                onParamChanged();
            });
        if (m_inQLFrontPort)
            connect(m_inQLFrontPort, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double v) {
                if (m_inQLFront) { m_inQLFront->blockSignals(true); m_inQLFront->setValue(v); m_inQLFront->blockSignals(false); }
                onParamChanged();
            });
        for (auto *s : {m_inPortFrontDiam, m_inPortFrontW, m_inPortFrontH, m_inPortFrontBraceSurf})
            if (s) connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                           this, &EnclosureWidget::onParamChanged);
        if (m_inPortFrontInsert)
            connect(m_inPortFrontInsert, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &EnclosureWidget::onParamChanged);
        // Front insert slider ↔ spinbox sync
        if (m_portFrontInsertSlider)
            connect(m_portFrontInsertSlider, &QSlider::valueChanged, this, [this](int pct) {
                if (!m_inPortFrontInsert) return;
                const double max = m_inPortFrontInsert->maximum();
                m_inPortFrontInsert->blockSignals(true);
                m_inPortFrontInsert->setValue(pct / 100.0 * max);
                m_inPortFrontInsert->blockSignals(false);
                if (m_portFrontInsertPctLbl) m_portFrontInsertPctLbl->setText(QString("%1%").arg(pct));
                onParamChanged();
            });
        if (m_inPortFrontInsert)
            connect(m_inPortFrontInsert, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double v) {
                if (!m_portFrontInsertSlider) return;
                const double max = m_inPortFrontInsert->maximum();
                const int pct = max > 0 ? qRound(v / max * 100.0) : 0;
                m_portFrontInsertSlider->blockSignals(true);
                m_portFrontInsertSlider->setValue(pct);
                m_portFrontInsertSlider->blockSignals(false);
                if (m_portFrontInsertPctLbl) m_portFrontInsertPctLbl->setText(QString("%1%").arg(pct));
            });
        if (m_portFrontShape)
            connect(m_portFrontShape, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int idx) {
                        if (m_portFrontDiamRow)  m_portFrontDiamRow ->setVisible(idx == 0);
                        if (m_portFrontRectRows) m_portFrontRectRows->setVisible(idx == 1);
                        if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
                            m_models[m_activeIdx].portFrontShape = idx;
                        onParamChanged();
                    });
        if (m_portFrontWalls)
            connect(m_portFrontWalls, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, &EnclosureWidget::onParamChanged);
        if (m_numPortsFront)
            connect(m_numPortsFront, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [this](int){ onParamChanged(); });
    }

    // ── PARAM TABS ────────────────────────────────────────────────
    m_paramTabs = new QTabWidget;
    m_paramTabs->setObjectName("encParamTabs");
    m_paramTabs->addTab(paramPanel,      "DRIVER");
    m_paramTabs->addTab(chambersPanel,   "CHAMBERS");
    m_paramTabs->addTab(m_portTabContent, "PORT");
    m_paramTabs->setMinimumHeight(160);
    m_paramTabs->setMaximumHeight(400);

    // Right side: vertical splitter with plot tabs + param tabs
    auto *rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->addWidget(m_plotTabs);
    rightSplit->addWidget(m_paramTabs);
    rightSplit->setStretchFactor(0, 4);
    rightSplit->setStretchFactor(1, 0);
    rightSplit->setCollapsible(0, false);
    rightSplit->setCollapsible(1, false);
    rightSplit->setHandleWidth(1);
    rightSplit->setStyleSheet(themed("QSplitter::handle{background:%border%;}"
                              "QSplitter::handle:hover{background:%accent%;}"));

    // Main horizontal splitter: left panel + right area
    auto *mainSplit = new QSplitter(Qt::Horizontal, this);
    mainSplit->addWidget(leftPanel);
    mainSplit->addWidget(rightSplit);
    mainSplit->setStretchFactor(0, 0);
    mainSplit->setStretchFactor(1, 1);
    mainSplit->setCollapsible(0, false);
    mainSplit->setCollapsible(1, false);
    mainSplit->setHandleWidth(1);
    mainSplit->setStyleSheet(themed("QSplitter::handle{background:%border%;}"
                             "QSplitter::handle:hover{background:%accent%;}"));
    mainSplit->setSizes({200, 900});

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->addWidget(mainSplit);

    // Connect model list selection
    connect(m_modelList, &QListWidget::currentRowChanged,
            this, &EnclosureWidget::onModelSelected);

    // Clicks on the colour swatch toggle visibility (handled in
    // eventFilter); clicks on the rest of the row pass through to the
    // default selection machinery (currentRowChanged → onModelSelected).
    m_modelList->viewport()->installEventFilter(this);

    // Right-click on model list → change plot colour
    m_modelList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_modelList, &QListWidget::customContextMenuRequested,
            this, [this](const QPoint &pos) {
        const int row = m_modelList->row(m_modelList->itemAt(pos));
        if (row < 0 || row >= m_models.size()) return;
        const QColor chosen = QColorDialog::getColor(
            m_models[row].color, this, "Choose plot colour");
        if (!chosen.isValid()) return;
        m_models[row].color = chosen;
        updateModelList();
        updatePlot();
    });
}

// ── Model list management ────────────────────────────────────────

void EnclosureWidget::updateModelList()
{
    m_modelList->blockSignals(true);
    m_modelList->clear();
    for (int i = 0; i < m_models.size(); ++i) {
        const auto &m = m_models[i];
        QString tag;
        if (isBandpass(m)) {
            const char *typeTag = isBP6(m) ? "BP6" : "BP4";
            tag = QString("[%1 %2–%3 Hz]")
                .arg(typeTag)
                .arg(m.f3Low  > 0 ? QString::number(m.f3Low,  'f', 0) : "–")
                .arg(m.f3High > 0 ? QString::number(m.f3High, 'f', 0) : "–");
        } else if (isVented(m)) {
            tag = QString("[fb=%1, F3=%2]")
                .arg(m.fb > 0 ? QString::number(m.fb, 'f', 1) : "–")
                .arg(m.f3 > 0 ? QString::number(m.f3, 'f', 1) : "–");
        } else {
            tag = QString("[Fc=%1, Qtc=%2]")
                .arg(m.Fc  > 0 ? QString::number(m.Fc,  'f', 1) : "–")
                .arg(m.Qtc > 0 ? QString::number(m.Qtc, 'f', 3) : "–");
        }

        // Dim the swatch and grey the text when the model is hidden.
        QColor swatchColor = m.color;
        if (!m.visible) {
            swatchColor.setHsl(swatchColor.hslHue(),
                               swatchColor.hslSaturation() / 4,
                               swatchColor.lightness() / 2 + 30);
        }
        auto *item = new QListWidgetItem(
            QIcon(colorSwatch(swatchColor, 14)),
            QString("%1  %2").arg(m.name, tag));
        item->setData(Qt::UserRole, i);
        if (!m.visible) {
            item->setForeground(Theme::instance().textDisabled());
            QFont f = item->font(); f.setItalic(true); item->setFont(f);
            item->setToolTip("Hidden — click the swatch to show on plots");
        } else {
            item->setForeground(Theme::instance().textPrimary());
            item->setToolTip("Visible — click the swatch to hide from plots");
        }
        m_modelList->addItem(item);
    }
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        m_modelList->setCurrentRow(m_activeIdx);
    m_modelList->blockSignals(false);
}

void EnclosureWidget::onModelSelected(int row)
{
    if (row < 0 || row >= m_models.size()) {
        m_activeIdx = -1;
        clearFields();
        updatePlot();
        return;
    }
    m_activeIdx = row;
    loadModelIntoFields(row);
    updatePlot();
}

void EnclosureWidget::onAddModel()
{
    BoxModel m;
    m.autoName = true;
    m.color    = nextColor();
    m.volumeL  = 40.0;
    m_models.append(m);
    m_activeIdx = m_models.size() - 1;
    refreshAutoName(m_activeIdx);
    updateModelList();
    loadModelIntoFields(m_activeIdx);
    updatePlot();
}

void EnclosureWidget::onDuplicateModel()
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    BoxModel copy = m_models[m_activeIdx];
    copy.color = nextColor();
    if (!copy.autoName) copy.name += " (copy)";
    const int insertAt = m_activeIdx + 1;
    m_models.insert(insertAt, copy);
    m_activeIdx = insertAt;
    refreshAutoName(m_activeIdx);
    recalculate(m_activeIdx);
    updateModelList();
    loadModelIntoFields(m_activeIdx);
    updatePlot();
}

void EnclosureWidget::addDriverModel(int driverId)
{
    if (!m_db || !m_db->isOpen()) return;
    const auto r = m_db->loadDriver(driverId);
    if (!r.isValid() || !r.hasResults()) return;

    BoxModel m;
    m.autoName  = true;
    m.color     = nextColor();
    m.volumeL   = 40.0;
    m.driverId  = driverId;
    m.fs        = r.fs;
    m.Vas_L     = r.Vas * 1000.0;
    m.Qts       = r.Qts;
    m.Qes       = r.Qes;
    m.Qms       = r.Qms;
    m.Re        = r.Re;
    m.mms_g     = r.mms * 1000.0;
    m.BL        = r.BL;
    m.Sd_cm2    = r.Sd * 10000.0;
    m.xmax_mm   = r.Xmax;
    m.xlim_mm   = r.Xlim;
    m.isDVC     = r.isDVC;
    m.dvcWiring = r.dvcWiring;
    m_models.append(m);
    m_activeIdx = m_models.size() - 1;
    refreshAutoName(m_activeIdx);
    recalculate(m_activeIdx);
    updateModelList();
    loadModelIntoFields(m_activeIdx);
    updatePlot();
}

void EnclosureWidget::setSpeedOfSound(double c)
{
    if (c <= 0) return;
    pp::setSpeedOfSound(c);
    recalculateAll();
    updatePortLength();
    updatePlot();
}

void EnclosureWidget::setPlotRanges(const PlotRangeSettings &r)
{
    m_plotRanges = r;
    auto applyRange = [](auto *plot, auto minOpt, auto maxOpt) {
        if (minOpt.has_value() && maxOpt.has_value())
            plot->setYRange(*minOpt, *maxOpt);
        else
            plot->resetYRange();
    };
    applyRange(m_splPlot, r.splMin,  r.splMax);
    applyRange(m_gdPlot,  r.gdMin,   r.gdMax);
    applyRange(m_vpPlot,  r.voltMin, r.voltMax);
    applyRange(m_excPlot, r.excMin,  r.excMax);
}

void EnclosureWidget::onRemoveModel()
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    m_models.removeAt(m_activeIdx);
    if (m_models.isEmpty()) {
        m_activeIdx = -1;
        clearFields();
    } else {
        m_activeIdx = std::min(m_activeIdx, int(m_models.size()) - 1);
        loadModelIntoFields(m_activeIdx);
    }
    updateModelList();
    updatePlot();
}

void EnclosureWidget::onRenameModel()
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, "Rename Model", "Name:",
        QLineEdit::Normal, m_models[m_activeIdx].name, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        m_models[m_activeIdx].name     = name.trimmed();
        m_models[m_activeIdx].autoName = false;
        updateModelList();
        updatePlot();
    }
}

// ── Driver selection ─────────────────────────────────────────────

void EnclosureWidget::refreshDriverList()
{
    m_driverCombo->blockSignals(true);
    const int prevDriverId = (m_activeIdx >= 0 && m_activeIdx < m_models.size())
                             ? m_models[m_activeIdx].driverId : -1;

    m_driverCombo->clear();
    m_driverIds.clear();
    m_driverCombo->addItem("(manual entry)");
    m_driverIds.append(-1);

    const auto records = m_db->isOpen() ? m_db->allDrivers() : QList<DriverRecord>{};
    for (const auto &r : records) {
        if (!r.hasResults()) continue;
        m_driverCombo->addItem(
            QString("%1 %2  (fₛ=%3, Qts=%4)")
                .arg(r.make, r.model)
                .arg(r.fs, 0, 'f', 1)
                .arg(r.Qts, 0, 'f', 3));
        m_driverIds.append(r.id);
    }

    // Restore selection
    if (prevDriverId >= 0) {
        int idx = m_driverIds.indexOf(prevDriverId);
        if (idx >= 0) m_driverCombo->setCurrentIndex(idx);
    }
    m_driverCombo->blockSignals(false);
}

void EnclosureWidget::onDriverChanged(int index)
{
    if (m_updating) return;
    if (index < 0 || index >= m_driverIds.size()) return;

    const int driverId = m_driverIds[index];

    // Auto-add a model when the list is empty and a real driver is chosen
    if (m_models.isEmpty() && driverId >= 0)
        onAddModel();

    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    auto &model = m_models[m_activeIdx];
    model.driverId = driverId;

    if (m_btnViewDriver) m_btnViewDriver->setEnabled(driverId >= 0);

    if (driverId < 0) return;   // manual — don't touch fields

    const auto r = m_db->loadDriver(driverId);
    if (!r.isValid() || !r.hasResults()) return;

    m_updating = true;

    model.fs    = r.fs;
    model.Vas_L = r.Vas * 1000.0;
    model.Qts   = r.Qts;
    model.Qes   = r.Qes;
    model.Qms   = r.Qms;
    model.Re     = r.Re;
    model.mms_g  = r.mms * 1000.0;
    model.BL     = r.BL;
    model.Sd_cm2 = r.Sd * 10000.0;
    model.xmax_mm= r.Xmax;
    model.xlim_mm= r.Xlim;
    model.isDVC     = r.isDVC;
    model.dvcWiring = r.dvcWiring;

    refreshAutoName(m_activeIdx);
    loadModelIntoFields(m_activeIdx);
    recalculate(m_activeIdx);
    updateModelList();
    updatePlot();

    m_updating = false;
}

void EnclosureWidget::onResetToDriver()
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    const int driverId = m_models[m_activeIdx].driverId;
    if (driverId < 0) return;

    // Re-trigger driver load
    const int comboIdx = m_driverIds.indexOf(driverId);
    if (comboIdx >= 0) {
        m_driverCombo->blockSignals(true);
        m_driverCombo->setCurrentIndex(comboIdx);
        m_driverCombo->blockSignals(false);
        onDriverChanged(comboIdx);
    }
}

void EnclosureWidget::onViewDriverParams()
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    const int driverId = m_models[m_activeIdx].driverId;
    if (driverId < 0 || !m_db) return;

    const auto r = m_db->loadDriver(driverId);
    if (!r.isValid()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(r.make + " " + r.model + " — Driver Parameters");
    dlg.setMinimumWidth(520);

    auto *detail = new DriverDetailWidget(&dlg);
    detail->loadDriver(r);

    connect(detail, &DriverDetailWidget::editRequested, this, [&dlg, this](int id){
        dlg.accept();
        emit editDriverRequested(id);
    });

    auto *btnClose = new QPushButton("Close");
    btnClose->setStyleSheet(themed(
        "QPushButton{background:%accent%;color:%page%;"
        "border:1px solid %accent%;border-radius:0px;"
        "padding:5px 18px;font-weight:600;}"
        "QPushButton:hover{background:%accentHov%;border-color:%accentHov%;}"));
    connect(btnClose, &QPushButton::clicked, &dlg, &QDialog::accept);

    auto *vb = new QVBoxLayout(&dlg);
    vb->setContentsMargins(0, 0, 0, 8);
    vb->setSpacing(0);
    vb->addWidget(detail, 1);
    auto *bb = new QHBoxLayout;
    bb->addStretch();
    bb->addWidget(btnClose);
    bb->setContentsMargins(0, 0, 8, 0);
    vb->addLayout(bb);

    dlg.exec();
}

// ── Parameter editing ────────────────────────────────────────────

void EnclosureWidget::onEncTypeChanged(int index)
{
    if (m_updating) return;
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    using ET = BoxModel::EncType;
    // Combo order: Sealed(0), Vented(1), BP4(2), BP6(3), IB(4)
    static const ET kComboToType[] = { ET::Sealed, ET::Vented, ET::Bandpass4, ET::Bandpass6, ET::IB };
    m_models[m_activeIdx].encType = kComboToType[std::clamp(index, 0, 4)];
    const BoxModel &cur = m_models[m_activeIdx];
    const bool sealed = isSealed(cur);
    const bool vented = isVented(cur);
    const bool ib     = isIB(cur);
    const bool ported = isPorted(cur);  // vented or bandpass
    const bool bandpass = isBandpass(cur);
    const bool bp4 = isBP4(cur);
    const bool bp6 = isBP6(cur);

    if (m_volLabel) {
        m_volLabel->setVisible(!ib);
        if (bp4)      m_volLabel->setText("REAR (sealed)");
        else if (bp6) m_volLabel->setText("REAR");
        else          m_volLabel->setText("BOX VOL");
    }
    if (m_volume)   m_volume  ->setVisible(!ib);
    if (m_volFrontLabel) {
        m_volFrontLabel->setVisible(bandpass);
        m_volFrontLabel->setText(bp6 ? "FRONT VOL" : "FRONT VOL (ported)");
    }
    if (m_volumeFront)   m_volumeFront  ->setVisible(bandpass);
    if (m_fbFrontLabel)  { m_fbFrontLabel ->setVisible(bandpass); m_fbFrontLabel->setText("FRONT fb"); }
    if (m_inFbFront)     m_inFbFront    ->setVisible(bandpass);
    if (m_qlFrontLabel)  { m_qlFrontLabel ->setVisible(bandpass); m_qlFrontLabel->setText("FRONT QL"); }
    if (m_inQLFront)     m_inQLFront    ->setVisible(bandpass);
    if (m_ibInfinityLbl) m_ibInfinityLbl->setVisible(ib);

    // Rear port tuning in Chambers tab (BP6 only)
    if (m_fbRearBpLabel) m_fbRearBpLabel->setVisible(bp6);
    if (m_inFbRearBp)    m_inFbRearBp   ->setVisible(bp6);
    if (m_qlRearBpLabel) m_qlRearBpLabel->setVisible(bp6);
    if (m_inQLRearBp)    m_inQLRearBp   ->setVisible(bp6);

    // Port tab: show fb/QL for all ported modes; label reflects which port
    if (m_portFbLabel) { m_portFbLabel->setVisible(ported);
        m_portFbLabel->setText(bp4 ? "Front fb:" : bp6 ? "Rear fb:" : "fb:"); }
    if (m_inFb)        m_inFb->setVisible(ported);
    if (m_portQLLabel) { m_portQLLabel->setVisible(ported);
        m_portQLLabel->setText(bp4 ? "Front QL:" : bp6 ? "Rear QL:" : "QL:"); }
    if (m_inQL)        m_inQL->setVisible(ported);
    // Chambers tab: vented fb/QL only for vented mode
    if (m_chambersFbLabel) m_chambersFbLabel->setVisible(vented);
    if (m_chambersFb)      m_chambersFb     ->setVisible(vented);
    if (m_chambersQLLabel) m_chambersQLLabel->setVisible(vented);
    if (m_chambersQL)      m_chambersQL     ->setVisible(vented);

    // Port tab section header and front port section
    if (m_portSectionHdr) {
        m_portSectionHdr->setVisible(bandpass);
        if (bp4)      m_portSectionHdr->setText("FRONT PORT");
        else if (bp6) m_portSectionHdr->setText("REAR PORT");
    }
    if (m_frontPortSection) m_frontPortSection->setVisible(bp6);
    if (m_btnBessel) m_btnBessel->setVisible(sealed);
    if (m_btnB2)     m_btnB2    ->setVisible(sealed);
    if (m_btnB4)     m_btnB4    ->setVisible(vented);
    if (m_paramTabs) {
        m_paramTabs->setTabEnabled(2, ported);
        if (!ported && m_paramTabs->currentIndex() == 2)
            m_paramTabs->setCurrentIndex(0);
    }
    if (m_plotTabs) {
        m_plotTabs->setTabEnabled(4, ported);
        if (!ported && m_plotTabs->currentIndex() == 4)
            m_plotTabs->setCurrentIndex(0);
    }
    refreshAutoName(m_activeIdx);
    updatePortLength();
    recalculate(m_activeIdx);
    updateModelList();
    updatePlot();
    if (m_resLblFc)  m_resLblFc ->setText(isBandpass(cur) ? "fb_f:" : (ported ? "fb:"  : "Fc:"));
    if (m_resLblQtc) m_resLblQtc->setText(isBandpass(cur) ? "Ripple:" : (ported ? "–:" : "Qtc:"));
    if (m_resLblQtc) m_resLblQtc->setVisible(sealed || isBandpass(cur));
    if (m_resQtc)    m_resQtc   ->setVisible(sealed || isBandpass(cur));
    updateOptSealedHint();
}

void EnclosureWidget::updateOptSealedHint()
{
    if (!m_lblOptSealed) return;
    const bool isSealedSel = m_activeIdx >= 0 && m_activeIdx < m_models.size()
                          && isSealed(m_models[m_activeIdx]);
    const double Qts = m_inQts ? m_inQts->value() : 0.0;
    const double Vas = m_inVas ? m_inVas->value() : 0.0;
    constexpr double TARGET_QTC = 0.707;
    if (isSealedSel && Qts > 0.0 && Qts < TARGET_QTC && Vas > 0.0) {
        const double alpha = (TARGET_QTC / Qts) * (TARGET_QTC / Qts) - 1.0;
        const double Vb = Vas / alpha;
        m_lblOptSealed->setText(QString("Closed Qtc 0.707: %1 L").arg(Vb, 0, 'f', 1));
        m_lblOptSealed->setVisible(true);
    } else {
        m_lblOptSealed->setVisible(false);
    }
}

// ── Alignment suggestions ─────────────────────────────────────────

static void applyVolumeToModel(EnclosureWidget *w, BoxModel &m,
                                QDoubleSpinBox *volSpin, double Vb_L)
{
    m.volumeL = Vb_L;
    volSpin->blockSignals(true);
    volSpin->setValue(Vb_L);
    volSpin->blockSignals(false);
    (void)w;
}

void EnclosureWidget::onAlignBessel()   // Qtc = 1/√3 ≈ 0.577
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    auto &m = m_models[m_activeIdx];
    constexpr double Qtc = 0.5774;
    if (m.Qts <= 0 || m.Qts >= Qtc) {
        QMessageBox::information(this, "Bessel alignment",
            QString("Requires Qts < %1 (current Qts = %2).")
            .arg(Qtc).arg(m.Qts, 0, 'f', 3));
        return;
    }
    const double alpha = (Qtc / m.Qts) * (Qtc / m.Qts) - 1.0;
    const double Vb    = m.Vas_L / alpha;
    applyVolumeToModel(this, m, m_volume, Vb);
    recalculate(m_activeIdx); updateModelList(); updatePlot();
}

void EnclosureWidget::onAlignB2()       // Butterworth: Qtc = 1/√2 ≈ 0.707
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    auto &m = m_models[m_activeIdx];
    constexpr double Qtc = 0.7071;
    if (m.Qts <= 0 || m.Qts >= Qtc) {
        QMessageBox::information(this, "Butterworth alignment",
            QString("Requires Qts < %1 (current Qts = %2).")
            .arg(Qtc).arg(m.Qts, 0, 'f', 3));
        return;
    }
    const double alpha = (Qtc / m.Qts) * (Qtc / m.Qts) - 1.0;
    const double Vb    = m.Vas_L / alpha;
    applyVolumeToModel(this, m, m_volume, Vb);
    recalculate(m_activeIdx); updateModelList(); updatePlot();
}

void EnclosureWidget::onAlignB4()
{
    // 4th-order Butterworth vented (B4) starting point.
    // Uses α ≈ (0.4/Qts)^2.87, h ≈ (Qts/0.4)^0.32 (fits to Small 1973 tables, QL=7).
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;
    auto &m = m_models[m_activeIdx];
    if (m.Qts <= 0 || m.Qts > 0.6 || m.Vas_L <= 0 || m.fs <= 0) {
        QMessageBox::information(this, "B4 vented alignment",
            "B4 alignment requires Qts in approximately 0.2–0.5 range.");
        return;
    }
    const double ratio = 0.4 / m.Qts;
    const double alpha = std::pow(ratio, 2.87);
    const double h     = std::pow(1.0 / ratio, 0.32);   // h = (Qts/0.4)^0.32
    const double Vb    = m.Vas_L / alpha;
    const double fb    = m.fs * h;

    applyVolumeToModel(this, m, m_volume, Vb);
    m.fb = fb;
    if (m_inFb) { m_inFb->blockSignals(true); m_inFb->setValue(fb); m_inFb->blockSignals(false); }
    recalculate(m_activeIdx); updateModelList(); updatePlot();
}

void EnclosureWidget::onPortShapeChanged(int index)
{
    if (m_portDiamRow)  m_portDiamRow ->setVisible(index == 0);
    if (m_portRectRows) m_portRectRows->setVisible(index == 1);
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
        m_models[m_activeIdx].portShape = index;
    updatePortLength();
    // Vol-in-box display only makes sense for round (we have insertion depth there)
    if (m_portVolDisplLbl) m_portVolDisplLbl->setVisible(index == 0);
}

void EnclosureWidget::updatePortGroup()
{
    const bool ported = (m_activeIdx >= 0 && m_activeIdx < m_models.size())
                        && isPorted(m_models[m_activeIdx]);
    if (m_paramTabs) {
        m_paramTabs->setTabEnabled(2, ported);
        if (!ported && m_paramTabs->currentIndex() == 2)
            m_paramTabs->setCurrentIndex(0);
    }
    if (m_plotTabs) {
        m_plotTabs->setTabEnabled(4, ported);
        if (!ported && m_plotTabs->currentIndex() == 4)
            m_plotTabs->setCurrentIndex(0);
    }
    updatePortLength();
}

void EnclosureWidget::updatePortLength()
{
    auto clearOut = [this]{
        if (m_portLenLbl)      m_portLenLbl     ->setText("–");
        if (m_portLenEachLbl)  m_portLenEachLbl ->setText("–");
        if (m_portAreaLbl)     m_portAreaLbl    ->setText("–");
        if (m_portSurfAreaLbl) m_portSurfAreaLbl->setText("–");
        if (m_portVolInnerLbl) m_portVolInnerLbl->setText("–");
        if (m_portVolDisplLbl) m_portVolDisplLbl->setText("–");
    };

    if (!m_portLenLbl) return;
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) { clearOut(); return; }
    const auto &m = m_models[m_activeIdx];

    // ── Flare note / diagram ─────────────────────────────────────
    if (m_portFlareNote) {
        if (m.portFlare == 0) {
            m_portFlareNote->setVisible(false);
        } else {
            // ASCII cross-section diagram: the flared end is to the left (outer/baffle side)
            const bool bothEnds = (m.portFlare == 2);
            QString diagram;
            if (!bothEnds) {
                diagram =
                    "  \u2190\u2500\u2500\u2500\u2500\u2500 L \u2500\u2500\u2500\u2500\u2500\u2192\n"
                    " \u256f\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2510\n"
                    "\u2502                   \u2502\n"
                    " \u2572\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2518\n"
                    " \u2191 from \u00BD flare";
            } else {
                diagram =
                    "  \u2190\u2500\u2500\u2500\u2500\u2500 L \u2500\u2500\u2500\u2500\u2500\u2192\n"
                    " \u256f\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u256e\n"
                    "\u2502                   \u2502\n"
                    " \u2572\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2571\n"
                    " \u2191 from \u00BD flares \u2191";
            }
            m_portFlareNote->setText(diagram);
            m_portFlareNote->setVisible(true);
        }
    }

    // For BP4, use front-chamber tuning with existing rear-port geometry controls
    if (isBP4(m)) {
        if (m.volumeFront_L <= 0 || m.fbFront <= 0) { clearOut(); return; }
    } else if (!isVented(m) && !isBP6(m)) {
        clearOut(); return;
    } else if (m.volumeL <= 0 || m.fb <= 0) {
        clearOut(); return;
    }

    // Effective tuning & volume: for BP4 use front chamber, others use rear/main
    const double calcVb = isBP4(m) ? m.volumeFront_L : m.volumeL;
    const double calcFb = isBP4(m) ? m.fbFront        : m.fb;

    const int N = std::max(1, m.numPorts);

    // Per-port cross-sectional area and equivalent diameter
    double Ap, De;
    if (m.portShape == 0) {
        // Round port: use inner diameter
        const double Dp = m_inPortDiam ? m_inPortDiam->value() / 1000.0 : 0.075;
        Ap = PI * (Dp / 2.0) * (Dp / 2.0);
        De = Dp;
    } else {
        const double W = m_inPortW ? m_inPortW->value() / 1000.0 : 0.1;
        const double H = m_inPortH ? m_inPortH->value() / 1000.0 : 0.05;
        Ap = W * H;
        De = 2.0 * std::sqrt(Ap / PI);
    }

    if (m_portAreaLbl)
        m_portAreaLbl->setText(QString("%1 cm²").arg(Ap * 1e4, 0, 'f', 1));

    // Acoustic mass and required physical port length
    const double Vb      = calcVb * 1e-3;
    const double Cab     = Vb / (RHO * g_C * g_C);
    const double omega_b = 2.0 * PI * calcFb;
    const double Map_total = 1.0 / (omega_b * omega_b * Cab);

    const double Lp_eff = N * Map_total * Ap / RHO;  // acoustic effective length = Lp + endCorr

    // End correction (total, both ends)
    // Round port: interpolate between outer-flanged/inner-unflanged (0% insert, 0.732·De)
    // and both-flanged/shared-wall (100% insert, 0.8216·De).
    // Two-pass: use the 0%-insertion Lp estimate to resolve the insertion ratio without
    // a circular dependency.
    // Rect port: depends on shared-wall count (portWalls).
    double endCorr;
    if (m.portShape == 0) {
        const double endCorr0   = 0.732  * De;   // outer flanged, inner unflanged
        const double endCorr100 = 0.8216 * De;   // both flanged (shared wall)
        const double Lp_base    = Lp_eff - endCorr0;
        const double insertM    = m.portInsertDepth_mm / 1000.0;
        const double r          = (Lp_base > 0) ? std::clamp(insertM / Lp_base, 0.0, 1.0) : 0.0;
        endCorr = endCorr0 + (endCorr100 - endCorr0) * r;
    } else {
        static constexpr double kEndCorr[] = { 0.732, 0.8216, 0.8730, 0.8860 };
        endCorr = kEndCorr[std::clamp(m.portWalls, 0, 3)] * De;
    }

    const double Lp = Lp_eff - endCorr;

    // Clamp "extends into box" spinbox maximum to the computed port length
    if (m_inPortInsert) {
        const double Lp_mm_max = Lp > 0 ? Lp * 1000.0 : 0.0;
        m_inPortInsert->blockSignals(true);
        m_inPortInsert->setMaximum(Lp_mm_max);
        if (m_inPortInsert->value() > Lp_mm_max)
            m_inPortInsert->setValue(Lp_mm_max);
        m_inPortInsert->blockSignals(false);
        // Sync slider to new value/range
        if (m_portInsertSlider) {
            const double v = m_inPortInsert->value();
            const int pct = Lp_mm_max > 0 ? qRound(v / Lp_mm_max * 100.0) : 0;
            m_portInsertSlider->blockSignals(true);
            m_portInsertSlider->setValue(pct);
            m_portInsertSlider->blockSignals(false);
            if (m_portInsertPctLbl) m_portInsertPctLbl->setText(QString("%1%").arg(pct));
        }
        // Keep model in sync if value was clamped
        if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
            m_models[m_activeIdx].portInsertDepth_mm = m_inPortInsert->value();
    }

    // ── Inner lateral surface area (per port + optional extra) ──────
    if (m_portSurfAreaLbl && Lp > 0) {
        double surfArea;
        if (m.portShape == 0) {
            const double Dp = m_inPortDiam ? m_inPortDiam->value() / 1000.0 : 0.075;
            surfArea = PI * Dp * Lp;  // π·D·L per port
        } else {
            const double W = m_inPortW ? m_inPortW->value() / 1000.0 : 0.1;
            const double H = m_inPortH ? m_inPortH->value() / 1000.0 : 0.05;
            surfArea = 2.0 * (W + H) * Lp;  // perimeter·L per port
        }
        double total_cm2 = surfArea * 1e4 + m.portExtraSurfArea_cm2;
        QString txt = QString("%1 cm²").arg(total_cm2, 0, 'f', 1);
        if (m.portExtraSurfArea_cm2 > 0)
            txt += QString(" (+%1)").arg(m.portExtraSurfArea_cm2, 0, 'f', 1);
        m_portSurfAreaLbl->setText(txt);
    } else if (m_portSurfAreaLbl) {
        m_portSurfAreaLbl->setText("–");
    }

    // ── Port air volume (bore × length, all ports combined) ──────
    if (m_portVolInnerLbl && Lp > 0) {
        const double V_air_L = N * Ap * Lp * 1000.0;  // m³ → litres
        m_portVolInnerLbl->setText(QString("%1 L").arg(V_air_L, 0, 'f', 3));
    } else if (m_portVolInnerLbl) {
        m_portVolInnerLbl->setText("–");
    }

    // ── Volume displaced inside box (round port only) ────────────
    if (m_portVolDisplLbl) {
        if (m.portShape == 0) {
            const double r_inner = (m_inPortDiam ? m_inPortDiam->value() : 75.0) / 2000.0; // m
            const double t       = m.portWallThick_mm / 1000.0;
            const double r_outer = r_inner + t;
            const double insertM = m.portInsertDepth_mm / 1000.0;
            const double V_L = N * PI * r_outer * r_outer * insertM * 1000.0;  // m³ → litres
            m_portVolDisplLbl->setText(insertM > 0.0
                ? QString("%1 L").arg(V_L, 0, 'f', 3)
                : "0 L");
            m_portVolDisplLbl->setVisible(true);
        } else {
            m_portVolDisplLbl->setText("–");
            m_portVolDisplLbl->setVisible(false);
        }
    }

    // ── Port length display ──────────────────────────────────────
    if (Lp <= 0) {
        m_portLenLbl->setText("<span style='color:#c0392b;'>Dimensions too large</span>");
        if (m_portLenEachLbl) m_portLenEachLbl->setText("–");
    } else {
        const double Lp_mm = Lp * 1000.0;
        m_portLenLbl->setText(QString("<b>%1 mm</b>").arg(Lp_mm, 0, 'f', 0));
        if (m_portLenEachLbl) {
            if (N > 1)
                m_portLenEachLbl->setText(QString("%1 mm each").arg(Lp_mm, 0, 'f', 0));
            else
                m_portLenEachLbl->setText("–");
        }
    }

    // ── 2nd pipe harmonic ────────────────────────────────────────
    // f₂H = c / Lp_eff  (open–open tube, 2nd resonance mode)
    if (m_portF2HLbl) {
        if (Lp_eff > 0) {
            const double f2h = g_C / Lp_eff;
            m_portF2HLbl->setText(QString("%1 Hz").arg(f2h, 0, 'f', 1));
            // Store on the model so the SPL plot can mark it
            if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
                m_models[m_activeIdx].portF2H = f2h;
        } else {
            m_portF2HLbl->setText("–");
            if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
                m_models[m_activeIdx].portF2H = 0.0;
        }
    }

    // ── BP6 front port length ────────────────────────────────────
    if (isBP6(m) && m_portFrontLenLbl) {
        auto clearFront = [this] {
            if (m_portFrontLenLbl)     m_portFrontLenLbl    ->setText("–");
            if (m_portFrontAreaLbl)    m_portFrontAreaLbl   ->setText("–");
            if (m_portFrontSurfAreaLbl) m_portFrontSurfAreaLbl->setText("–");
            if (m_portFrontVolDisplLbl) m_portFrontVolDisplLbl->setText("–");
        };
        if (m.volumeFront_L <= 0 || m.fbFront <= 0) { clearFront(); return; }

        const int Nf = std::max(1, m.numPortsFront);
        double Apf, Def;
        if (m.portFrontShape == 0) {
            const double Dp = m_inPortFrontDiam ? m_inPortFrontDiam->value() / 1000.0 : 0.075;
            Apf = PI * (Dp / 2.0) * (Dp / 2.0);
            Def = Dp;
        } else {
            const double Wf = m_inPortFrontW ? m_inPortFrontW->value() / 1000.0 : 0.1;
            const double Hf = m_inPortFrontH ? m_inPortFrontH->value() / 1000.0 : 0.05;
            Apf = Wf * Hf;
            Def = 2.0 * std::sqrt(Apf / PI);
        }
        if (m_portFrontAreaLbl)
            m_portFrontAreaLbl->setText(QString("%1 cm²").arg(Apf * 1e4, 0, 'f', 1));

        const double VbF      = m.volumeFront_L * 1e-3;
        const double CabF     = VbF / (RHO * g_C * g_C);
        const double omegaF   = 2.0 * PI * m.fbFront;
        const double Map_totF = 1.0 / (omegaF * omegaF * CabF);
        const double Lp_effF  = Nf * Map_totF * Apf / RHO;

        // End correction — same interpolation logic as rear port
        double endCorrF;
        if (m.portFrontShape == 0) {
            const double ec0    = 0.732  * Def;
            const double ec100  = 0.8216 * Def;
            const double Lp_base = Lp_effF - ec0;
            const double insertM = m.portFrontInsertDepth_mm / 1000.0;
            const double r = (Lp_base > 0) ? std::clamp(insertM / Lp_base, 0.0, 1.0) : 0.0;
            endCorrF = ec0 + (ec100 - ec0) * r;
        } else {
            static constexpr double kEC[] = { 0.732, 0.8216, 0.8730, 0.8860 };
            endCorrF = kEC[std::clamp(m.portFrontWalls, 0, 3)] * Def;
        }
        const double LpF = Lp_effF - endCorrF;

        // Clamp insert spinbox max to computed length
        if (m_inPortFrontInsert) {
            const double maxMm = LpF > 0 ? LpF * 1000.0 : 0.0;
            m_inPortFrontInsert->blockSignals(true);
            m_inPortFrontInsert->setMaximum(maxMm);
            if (m_inPortFrontInsert->value() > maxMm)
                m_inPortFrontInsert->setValue(maxMm);
            m_inPortFrontInsert->blockSignals(false);
            if (m_portFrontInsertSlider) {
                const double v = m_inPortFrontInsert->value();
                const int pct = maxMm > 0 ? qRound(v / maxMm * 100.0) : 0;
                m_portFrontInsertSlider->blockSignals(true);
                m_portFrontInsertSlider->setValue(pct);
                m_portFrontInsertSlider->blockSignals(false);
                if (m_portFrontInsertPctLbl) m_portFrontInsertPctLbl->setText(QString("%1%").arg(pct));
            }
            if (m_activeIdx >= 0 && m_activeIdx < m_models.size())
                m_models[m_activeIdx].portFrontInsertDepth_mm = m_inPortFrontInsert->value();
        }

        // Inner surface area + bracing
        if (m_portFrontSurfAreaLbl && LpF > 0) {
            double surfArea;
            if (m.portFrontShape == 0) {
                const double Dp = m_inPortFrontDiam ? m_inPortFrontDiam->value() / 1000.0 : 0.075;
                surfArea = PI * Dp * LpF;
            } else {
                const double Wf = m_inPortFrontW ? m_inPortFrontW->value() / 1000.0 : 0.1;
                const double Hf = m_inPortFrontH ? m_inPortFrontH->value() / 1000.0 : 0.05;
                surfArea = 2.0 * (Wf + Hf) * LpF;
            }
            const double total_cm2 = surfArea * 1e4 + m.portFrontExtraSurfArea_cm2;
            QString txt = QString("%1 cm²").arg(total_cm2, 0, 'f', 1);
            if (m.portFrontExtraSurfArea_cm2 > 0)
                txt += QString(" (+%1)").arg(m.portFrontExtraSurfArea_cm2, 0, 'f', 1);
            m_portFrontSurfAreaLbl->setText(txt);
        } else if (m_portFrontSurfAreaLbl) {
            m_portFrontSurfAreaLbl->setText("–");
        }

        // Vol displaced in box (round only, insert depth)
        if (m_portFrontVolDisplLbl) {
            if (m.portFrontShape == 0) {
                const double r_inner = (m_inPortFrontDiam ? m_inPortFrontDiam->value() : 75.0) / 2000.0;
                const double insertM = m.portFrontInsertDepth_mm / 1000.0;
                const double V_L = Nf * PI * r_inner * r_inner * insertM * 1000.0;
                m_portFrontVolDisplLbl->setText(insertM > 0.0
                    ? QString("%1 L").arg(V_L, 0, 'f', 3) : "0 L");
                m_portFrontVolDisplLbl->setVisible(true);
            } else {
                m_portFrontVolDisplLbl->setText("–");
                m_portFrontVolDisplLbl->setVisible(false);
            }
        }

        if (LpF <= 0)
            m_portFrontLenLbl->setText("<span style='color:#c0392b;'>Too large</span>");
        else
            m_portFrontLenLbl->setText(QString("<b>%1 mm</b>").arg(LpF * 1000.0, 0, 'f', 0));
    } else if (m_portFrontLenLbl) {
        m_portFrontLenLbl->setText("–");
        if (m_portFrontAreaLbl)    m_portFrontAreaLbl   ->setText("–");
        if (m_portFrontSurfAreaLbl) m_portFrontSurfAreaLbl->setText("–");
        if (m_portFrontVolDisplLbl) m_portFrontVolDisplLbl->setText("–");
    }
}

void EnclosureWidget::onParamChanged()
{
    if (m_updating) return;
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) return;

    auto &model = m_models[m_activeIdx];
    model.fs      = m_inFs->value();
    model.Vas_L   = m_inVas->value();
    model.Qts     = m_inQts->value();
    model.Qes     = m_inQes->value();
    model.volumeL = m_volume->value();
    model.Qms     = m_dpQms->value();
    model.Re      = m_dpRe->value();
    model.mms_g   = m_dpMms->value();
    model.BL      = m_dpBL->value();
    model.Sd_cm2  = m_dpSd->value();
    // For BP modes fb/QL come from Chambers tab; Port tab fb/QL only used for vented
    if (!isBandpass(model)) {
        if (m_inFb) model.fb = m_inFb->value();
        if (m_inQL) model.QL = m_inQL->value();
    }
    // BP6: rear port tuning from dedicated Chambers tab spinboxes
    if (isBP6(model)) {
        if (m_inFbRearBp) model.fb = m_inFbRearBp->value();
        if (m_inQLRearBp) model.QL = m_inQLRearBp->value();
    }
    if (m_volumeFront) model.volumeFront_L = m_volumeFront->value();
    if (m_inFbFront)   model.fbFront       = m_inFbFront->value();
    if (m_inQLFront)   model.QLFront       = m_inQLFront->value();
    if (m_portShape)  model.portShape  = m_portShape->currentIndex();
    if (m_portFlare)  model.portFlare  = m_portFlare->currentIndex();
    if (m_portWalls)  model.portWalls  = m_portWalls->currentIndex();
    if (m_numPorts)   model.numPorts   = m_numPorts->value();
    if (m_numDrivers) model.numDrivers = m_numDrivers->value();
    if (m_wiringGroup) model.wiringMode = static_cast<BoxModel::WiringMode>(m_wiringGroup->checkedId());
    // portWidth_mm stores round diameter (shape=0) OR rectangular width (shape=1)
    if (model.portShape == 0 && m_inPortDiam)
        model.portWidth_mm  = m_inPortDiam->value();
    else if (model.portShape == 1 && m_inPortW)
        model.portWidth_mm  = m_inPortW->value();
    if (m_inPortH)         model.portHeight_mm      = m_inPortH->value();
    if (m_inPortWallThick)  model.portWallThick_mm        = m_inPortWallThick->value();
    if (m_inPortInsert)     model.portInsertDepth_mm      = m_inPortInsert->value();
    if (m_inPortBraceSurf)  model.portExtraSurfArea_cm2   = m_inPortBraceSurf->value();
    if (m_inAddedMass)      model.addedMass_g             = m_inAddedMass->value();
    // BP6 front port geometry
    if (isBP6(model)) {
        if (m_portFrontShape) model.portFrontShape = m_portFrontShape->currentIndex();
        if (m_portFrontWalls) model.portFrontWalls = m_portFrontWalls->currentIndex();
        if (m_numPortsFront)  model.numPortsFront  = m_numPortsFront->value();
        if (model.portFrontShape == 0 && m_inPortFrontDiam)
            model.portFrontWidth_mm  = m_inPortFrontDiam->value();
        else if (model.portFrontShape == 1 && m_inPortFrontW)
            model.portFrontWidth_mm  = m_inPortFrontW->value();
        if (m_inPortFrontH)         model.portFrontHeight_mm        = m_inPortFrontH->value();
        if (m_inPortFrontInsert)    model.portFrontInsertDepth_mm   = m_inPortFrontInsert->value();
        if (m_inPortFrontBraceSurf) model.portFrontExtraSurfArea_cm2 = m_inPortFrontBraceSurf->value();
    }

    // Update the "→ fs ≈ X Hz" hint next to the added-mass spinbox
    if (m_addedMassEffLbl) {
        if (model.addedMass_g > 0.0 && model.mms_g > 0.0 && model.fs > 0.0) {
            const double r  = (model.mms_g + model.addedMass_g) / model.mms_g;
            const double fe = model.fs / std::sqrt(r);
            m_addedMassEffLbl->setText(
                QString("→ fₛ ≈ %1 Hz, mₘₛ ≈ %2 g")
                    .arg(fe, 0, 'f', 2)
                    .arg(model.mms_g + model.addedMass_g, 0, 'f', 2));
            m_addedMassEffLbl->setVisible(true);
        } else {
            m_addedMassEffLbl->setVisible(false);
        }
    }

    refreshAutoName(m_activeIdx);
    updateOptSealedHint();
    updatePortLength();
    recalculate(m_activeIdx);
    updateModelList();
    updatePlot();
}

void EnclosureWidget::loadModelIntoFields(int index)
{
    if (index < 0 || index >= m_models.size()) { clearFields(); return; }

    lockTsFields();
    m_updating = true;
    const auto &model = m_models[index];

    m_inFs ->setValue(model.fs);
    m_inVas->setValue(model.Vas_L);
    m_inQts->setValue(model.Qts);
    m_inQes->setValue(model.Qes);
    m_volume->setValue(model.volumeL);

    m_dpQms->setValue(model.Qms);
    m_dpRe ->setValue(model.Re);
    m_dpMms->setValue(model.mms_g);
    m_dpBL ->setValue(model.BL);
    m_dpSd ->setValue(model.Sd_cm2);

    if (m_inAddedMass) {
        m_inAddedMass->blockSignals(true);
        m_inAddedMass->setValue(model.addedMass_g);
        m_inAddedMass->blockSignals(false);
    }
    if (m_addedMassEffLbl) {
        if (model.addedMass_g > 0.0 && model.mms_g > 0.0 && model.fs > 0.0) {
            const double r  = (model.mms_g + model.addedMass_g) / model.mms_g;
            const double fe = model.fs / std::sqrt(r);
            m_addedMassEffLbl->setText(
                QString("→ fₛ ≈ %1 Hz, mₘₛ ≈ %2 g")
                    .arg(fe, 0, 'f', 2)
                    .arg(model.mms_g + model.addedMass_g, 0, 'f', 2));
            m_addedMassEffLbl->setVisible(true);
        } else {
            m_addedMassEffLbl->setVisible(false);
        }
    }

    // Enclosure type + ported inputs
    if (m_encType) {
        m_encType->blockSignals(true);
        // Combo order: Sealed(0), Vented(1), BP4(2), BP6(3), IB(4)
        // EncType enum: Sealed=0, Vented=1, IB=2, Bandpass4=3, Bandpass6=4
        static const int kTypeToCombo[] = { 0, 1, 4, 2, 3 };
        m_encType->setCurrentIndex(kTypeToCombo[std::clamp(static_cast<int>(model.encType), 0, 4)]);
        m_encType->blockSignals(false);
    }
    {
        const bool ib     = isIB(model);
        const bool bp     = isBandpass(model);
        const bool bp4    = isBP4(model);
        const bool bp6    = isBP6(model);
        const bool vented = isVented(model);
        const bool ported = isPorted(model);
        if (m_volLabel) {
            m_volLabel->setVisible(!ib);
            if (bp4)      m_volLabel->setText("REAR (sealed)");
            else if (bp6) m_volLabel->setText("REAR");
            else          m_volLabel->setText("BOX VOL");
        }
        if (m_volume)   m_volume  ->setVisible(!ib);
        if (m_volFrontLabel) {
            m_volFrontLabel->setVisible(bp);
            m_volFrontLabel->setText(bp6 ? "FRONT VOL" : "FRONT VOL (ported)");
        }
        if (m_volumeFront)   m_volumeFront  ->setVisible(bp);
        if (m_fbFrontLabel)  m_fbFrontLabel ->setVisible(bp);
        if (m_inFbFront)     m_inFbFront    ->setVisible(bp);
        if (m_qlFrontLabel)  m_qlFrontLabel ->setVisible(bp);
        if (m_inQLFront)     m_inQLFront    ->setVisible(bp);
        if (m_ibInfinityLbl) m_ibInfinityLbl->setVisible(ib);
        // Rear port tuning in Chambers tab (BP6 only)
        if (m_fbRearBpLabel) m_fbRearBpLabel->setVisible(bp6);
        if (m_inFbRearBp)    m_inFbRearBp   ->setVisible(bp6);
        if (m_qlRearBpLabel) m_qlRearBpLabel->setVisible(bp6);
        if (m_inQLRearBp)    m_inQLRearBp   ->setVisible(bp6);
        // Port tab: show fb/QL for all ported modes; label reflects which port
        if (m_portFbLabel) { m_portFbLabel->setVisible(ported);
            m_portFbLabel->setText(bp4 ? "Front fb:" : bp6 ? "Rear fb:" : "fb:"); }
        if (m_inFb)        m_inFb->setVisible(ported);
        if (m_portQLLabel) { m_portQLLabel->setVisible(ported);
            m_portQLLabel->setText(bp4 ? "Front QL:" : bp6 ? "Rear QL:" : "QL:"); }
        if (m_inQL)        m_inQL->setVisible(ported);
        // Chambers tab: vented fb/QL only for vented mode
        if (m_chambersFbLabel) m_chambersFbLabel->setVisible(vented);
        if (m_chambersFb)      m_chambersFb     ->setVisible(vented);
        if (m_chambersQLLabel) m_chambersQLLabel->setVisible(vented);
        if (m_chambersQL)      m_chambersQL     ->setVisible(vented);
        // Port tab section header and front port section
        if (m_portSectionHdr) {
            m_portSectionHdr->setVisible(bp);
            if (bp4)      m_portSectionHdr->setText("FRONT PORT");
            else if (bp6) m_portSectionHdr->setText("REAR PORT");
        }
        if (m_frontPortSection) m_frontPortSection->setVisible(bp6);
    }
    if (m_inFb) { m_inFb->blockSignals(true); m_inFb->setValue(isBP4(model) ? model.fbFront : model.fb); m_inFb->blockSignals(false); }
    if (m_inQL) { m_inQL->blockSignals(true); m_inQL->setValue(isBP4(model) ? model.QLFront : model.QL); m_inQL->blockSignals(false); }
    if (m_chambersFb) { m_chambersFb->blockSignals(true); m_chambersFb->setValue(model.fb);      m_chambersFb->blockSignals(false); }
    if (m_chambersQL) { m_chambersQL->blockSignals(true); m_chambersQL->setValue(model.QL);      m_chambersQL->blockSignals(false); }
    if (m_inFbFrontPort) { m_inFbFrontPort->blockSignals(true); m_inFbFrontPort->setValue(model.fbFront);  m_inFbFrontPort->blockSignals(false); }
    if (m_inQLFrontPort) { m_inQLFrontPort->blockSignals(true); m_inQLFrontPort->setValue(model.QLFront);  m_inQLFrontPort->blockSignals(false); }
    if (m_volumeFront) { m_volumeFront->blockSignals(true); m_volumeFront->setValue(model.volumeFront_L); m_volumeFront->blockSignals(false); }
    if (m_inFbFront)   { m_inFbFront  ->blockSignals(true); m_inFbFront  ->setValue(model.fbFront);       m_inFbFront  ->blockSignals(false); }
    if (m_inQLFront)   { m_inQLFront  ->blockSignals(true); m_inQLFront  ->setValue(model.QLFront);       m_inQLFront  ->blockSignals(false); }
    // Rear port tuning for BP6
    if (m_inFbRearBp) { m_inFbRearBp->blockSignals(true); m_inFbRearBp->setValue(model.fb);    m_inFbRearBp->blockSignals(false); }
    if (m_inQLRearBp) { m_inQLRearBp->blockSignals(true); m_inQLRearBp->setValue(model.QL);    m_inQLRearBp->blockSignals(false); }
    // Front port geometry for BP6
    if (m_portFrontShape) {
        m_portFrontShape->blockSignals(true);
        m_portFrontShape->setCurrentIndex(model.portFrontShape);
        m_portFrontShape->blockSignals(false);
        if (m_portFrontDiamRow)  m_portFrontDiamRow ->setVisible(model.portFrontShape == 0);
        if (m_portFrontRectRows) m_portFrontRectRows->setVisible(model.portFrontShape == 1);
    }
    if (m_inPortFrontDiam) { m_inPortFrontDiam->blockSignals(true); m_inPortFrontDiam->setValue(model.portFrontWidth_mm); m_inPortFrontDiam->blockSignals(false); }
    if (m_inPortFrontW)    { m_inPortFrontW   ->blockSignals(true); m_inPortFrontW   ->setValue(model.portFrontShape == 1 ? model.portFrontWidth_mm : m_inPortFrontW->value()); m_inPortFrontW->blockSignals(false); }
    if (m_inPortFrontH)    { m_inPortFrontH   ->blockSignals(true); m_inPortFrontH   ->setValue(model.portFrontHeight_mm); m_inPortFrontH->blockSignals(false); }
    if (m_portFrontWalls)  { m_portFrontWalls ->blockSignals(true); m_portFrontWalls ->setCurrentIndex(std::clamp(model.portFrontWalls, 0, 3)); m_portFrontWalls->blockSignals(false); }
    if (m_numPortsFront)   { m_numPortsFront  ->blockSignals(true); m_numPortsFront  ->setValue(std::max(1, model.numPortsFront)); m_numPortsFront->blockSignals(false); }
    if (m_inPortFrontInsert) {
        m_inPortFrontInsert->blockSignals(true);
        m_inPortFrontInsert->setValue(model.portFrontInsertDepth_mm);
        m_inPortFrontInsert->blockSignals(false);
    }
    if (m_inPortFrontBraceSurf) {
        m_inPortFrontBraceSurf->blockSignals(true);
        m_inPortFrontBraceSurf->setValue(model.portFrontExtraSurfArea_cm2);
        m_inPortFrontBraceSurf->blockSignals(false);
    }

    // Port tab fields
    if (m_portShape) {
        m_portShape->blockSignals(true);
        m_portShape->setCurrentIndex(model.portShape);
        m_portShape->blockSignals(false);
        if (m_portDiamRow)  m_portDiamRow ->setVisible(model.portShape == 0);
        if (m_portRectRows) m_portRectRows->setVisible(model.portShape == 1);
    }
    if (m_inPortDiam && model.portShape == 0) {
        m_inPortDiam->blockSignals(true);
        m_inPortDiam->setValue(model.portWidth_mm);
        m_inPortDiam->blockSignals(false);
    }
    if (m_inPortW) {
        m_inPortW->blockSignals(true);
        m_inPortW->setValue(model.portShape == 1 ? model.portWidth_mm : m_inPortW->value());
        m_inPortW->blockSignals(false);
    }
    if (m_inPortH) {
        m_inPortH->blockSignals(true);
        m_inPortH->setValue(model.portHeight_mm);
        m_inPortH->blockSignals(false);
    }
    if (m_portWalls) {
        m_portWalls->blockSignals(true);
        m_portWalls->setCurrentIndex(std::clamp(model.portWalls, 0, 3));
        m_portWalls->blockSignals(false);
    }
    if (m_numPorts) {
        m_numPorts->blockSignals(true);
        m_numPorts->setValue(std::max(1, model.numPorts));
        m_numPorts->blockSignals(false);
    }
    if (m_numDrivers) {
        m_numDrivers->blockSignals(true);
        m_numDrivers->setValue(std::max(1, model.numDrivers));
        m_numDrivers->blockSignals(false);
    }
    if (m_wiringBtnSeries && m_wiringBtnParallel && m_wiringBtnSeparate) {
        m_wiringBtnSeries  ->blockSignals(true);
        m_wiringBtnParallel->blockSignals(true);
        m_wiringBtnSeparate->blockSignals(true);
        switch (model.wiringMode) {
            case BoxModel::WiringMode::Parallel:  m_wiringBtnParallel->setChecked(true); break;
            case BoxModel::WiringMode::Separate:  m_wiringBtnSeparate->setChecked(true); break;
            default:                              m_wiringBtnSeries  ->setChecked(true); break;
        }
        m_wiringBtnSeries  ->blockSignals(false);
        m_wiringBtnParallel->blockSignals(false);
        m_wiringBtnSeparate->blockSignals(false);
    }
    if (m_portFlare) {
        m_portFlare->blockSignals(true);
        m_portFlare->setCurrentIndex(std::clamp(model.portFlare, 0, 2));
        m_portFlare->blockSignals(false);
    }
    if (m_inPortWallThick) {
        m_inPortWallThick->blockSignals(true);
        m_inPortWallThick->setValue(model.portWallThick_mm);
        m_inPortWallThick->blockSignals(false);
    }
    if (m_inPortInsert) {
        m_inPortInsert->blockSignals(true);
        m_inPortInsert->setValue(model.portInsertDepth_mm);
        m_inPortInsert->blockSignals(false);
    }
    if (m_inPortBraceSurf) {
        m_inPortBraceSurf->blockSignals(true);
        m_inPortBraceSurf->setValue(model.portExtraSurfArea_cm2);
        m_inPortBraceSurf->blockSignals(false);
    }
    if (m_portVolDisplLbl) m_portVolDisplLbl->setVisible(model.portShape == 0);
    if (m_paramTabs) {
        m_paramTabs->setTabEnabled(2, isPorted(model));
        if (!isPorted(model) && m_paramTabs->currentIndex() == 2)
            m_paramTabs->setCurrentIndex(0);
    }
    if (m_plotTabs) {
        m_plotTabs->setTabEnabled(4, isPorted(model));
        if (!isPorted(model) && m_plotTabs->currentIndex() == 4)
            m_plotTabs->setCurrentIndex(0);
    }
    if (m_resLblFc)  m_resLblFc ->setText(isBandpass(model) ? "fb_f:" : (isPorted(model) ? "fb:" : "Fc:"));
    if (m_resLblQtc) { m_resLblQtc->setText(isBandpass(model) ? "Ripple:" : (isPorted(model) ? "–:" : "Qtc:"));
                       m_resLblQtc->setVisible(isSealed(model) || isBandpass(model)); }
    if (m_resQtc)    m_resQtc->setVisible(isSealed(model) || isBandpass(model));
    updatePortLength();

    auto fmt = [](double v, int d){ return QString::number(v,'f',d); };

    // Driver combo
    m_driverCombo->blockSignals(true);
    int comboIdx = (model.driverId >= 0) ? m_driverIds.indexOf(model.driverId) : 0;
    m_driverCombo->setCurrentIndex(std::max(0, comboIdx));
    m_driverCombo->blockSignals(false);

    if (m_btnViewDriver) m_btnViewDriver->setEnabled(model.driverId >= 0);

    if (m_vcTypeLbl) {
        if (!model.isDVC)
            m_vcTypeLbl->setText("Voice coil: SVC");
        else
            m_vcTypeLbl->setText(model.dvcWiring == 0
                ? "Voice coil: DVC – Series"
                : "Voice coil: DVC – Parallel");
    }

    // Show results
    if (isBandpass(model)) {
        m_resAlpha->setText("–");
        m_resFc   ->setText(model.Fc > 0 ? fmt(model.Fc, 2) : "–");      // front fb
        m_resQtc  ->setText(model.passbandRippleDb > 0
                            ? QString("%1 dB").arg(model.passbandRippleDb, 0, 'f', 2) : "–");
        m_resF3   ->setText(model.f3Low > 0 && model.f3High > 0
                            ? QString("%1 – %2").arg(model.f3Low,  0, 'f', 1)
                                                 .arg(model.f3High, 0, 'f', 1)
                            : "–");
        m_resEta  ->setText("–");
        m_resSpl  ->setText(model.peakSpl > 0 ? fmt(model.peakSpl, 2) : "–");
    } else {
        m_resAlpha->setText(isIB(model) ? "–" : (model.alpha > 0 ? fmt(model.alpha, 3) : "–"));
        m_resFc   ->setText(model.Fc > 0 ? fmt(model.Fc, 2) : "–");
        m_resQtc  ->setText(isVented(model) ? "–" : (model.Qtc > 0 ? fmt(model.Qtc, 3) : "–"));
        m_resF3   ->setText(model.f3 > 0 ? fmt(model.f3, 2) : "–");
        m_resEta  ->setText(model.eta > 0 ? fmt(model.eta * 100.0, 3) : "–");
        m_resSpl  ->setText(model.spl > 0 ? fmt(model.spl, 2) : "–");
    }

    m_updating = false;
    updateOptSealedHint();
}

void EnclosureWidget::clearFields()
{
    lockTsFields();
    m_updating = true;
    for (auto *s : {m_inFs, m_inVas, m_inQts, m_inQes})
        s->setValue(0.0);
    m_volume->setValue(40.0);
    for (auto *s : {m_dpQms, m_dpRe, m_dpMms, m_dpBL, m_dpSd})
        s->setValue(0.0);
    if (m_inAddedMass) {
        m_inAddedMass->blockSignals(true);
        m_inAddedMass->setValue(0.0);
        m_inAddedMass->blockSignals(false);
    }
    if (m_addedMassEffLbl) m_addedMassEffLbl->setVisible(false);
    if (m_encType)  { m_encType->blockSignals(true); m_encType->setCurrentIndex(0); m_encType->blockSignals(false); }
    if (m_volLabel) m_volLabel->setVisible(true);
    if (m_volume)   m_volume  ->setVisible(true);
    if (m_inFb)          { m_inFb->blockSignals(true);          m_inFb->setValue(35.0);          m_inFb->blockSignals(false); }
    if (m_inQL)          { m_inQL->blockSignals(true);          m_inQL->setValue(7.0);           m_inQL->blockSignals(false); }
    if (m_chambersFb)    { m_chambersFb->blockSignals(true);    m_chambersFb->setValue(35.0);    m_chambersFb->blockSignals(false); }
    if (m_chambersQL)    { m_chambersQL->blockSignals(true);    m_chambersQL->setValue(7.0);     m_chambersQL->blockSignals(false); }
    if (m_inFbFrontPort) { m_inFbFrontPort->blockSignals(true); m_inFbFrontPort->setValue(60.0); m_inFbFrontPort->blockSignals(false); }
    if (m_inQLFrontPort) { m_inQLFrontPort->blockSignals(true); m_inQLFrontPort->setValue(7.0);  m_inQLFrontPort->blockSignals(false); }
    if (m_paramTabs) {
        m_paramTabs->setTabEnabled(2, false);
        if (m_paramTabs->currentIndex() == 2)
            m_paramTabs->setCurrentIndex(0);
    }
    if (m_plotTabs) {
        m_plotTabs->setTabEnabled(4, false);
        if (m_plotTabs->currentIndex() == 4)
            m_plotTabs->setCurrentIndex(0);
    }
    if (m_resLblFc)   m_resLblFc ->setText("Fc:");
    if (m_resLblQtc) { m_resLblQtc->setText("Qtc:"); m_resLblQtc->setVisible(true); }
    if (m_resQtc)     m_resQtc->setVisible(true);
    if (m_vcTypeLbl)  m_vcTypeLbl->setText("Voice coil: –");
    if (m_portLenLbl)      m_portLenLbl->setText("–");
    if (m_portLenEachLbl)  m_portLenEachLbl->setText("");
    if (m_portF2HLbl)      m_portF2HLbl->setText("–");
    if (m_portAreaLbl)     m_portAreaLbl->setText("–");
    if (m_portSurfAreaLbl) m_portSurfAreaLbl->setText("–");
    if (m_portVolInnerLbl) m_portVolInnerLbl->setText("–");
    if (m_portVolDisplLbl) m_portVolDisplLbl->setText("–");
    if (m_portWalls)   { m_portWalls->blockSignals(true);   m_portWalls->setCurrentIndex(0);  m_portWalls->blockSignals(false); }
    if (m_numPorts)    { m_numPorts->blockSignals(true);    m_numPorts->setValue(1);           m_numPorts->blockSignals(false); }
    if (m_numDrivers)  { m_numDrivers->blockSignals(true);  m_numDrivers->setValue(1);         m_numDrivers->blockSignals(false); }
    if (m_wiringBtnSeries) { m_wiringBtnSeries->blockSignals(true); m_wiringBtnSeries->setChecked(true); m_wiringBtnSeries->blockSignals(false); }
    if (m_portFlare)   { m_portFlare->blockSignals(true);   m_portFlare->setCurrentIndex(0);   m_portFlare->blockSignals(false); }
    if (m_inPortWallThick){ m_inPortWallThick->blockSignals(true); m_inPortWallThick->setValue(3.0); m_inPortWallThick->blockSignals(false); }
    if (m_inPortInsert)   { m_inPortInsert->blockSignals(true);    m_inPortInsert->setValue(0.0);    m_inPortInsert->blockSignals(false); }
    if (m_portInsertSlider) { m_portInsertSlider->blockSignals(true); m_portInsertSlider->setValue(0); m_portInsertSlider->blockSignals(false); }
    if (m_portInsertPctLbl) m_portInsertPctLbl->setText("0%");
    if (m_inPortBraceSurf) { m_inPortBraceSurf->blockSignals(true); m_inPortBraceSurf->setValue(0.0); m_inPortBraceSurf->blockSignals(false); }
    if (m_portFlareNote)   m_portFlareNote->setVisible(false);
    if (m_portFrontLenLbl)  m_portFrontLenLbl ->setText("–");
    if (m_portFrontAreaLbl) m_portFrontAreaLbl->setText("–");
    if (m_volLabel) m_volLabel->setText("BOX VOL");
    if (m_portSectionHdr)   m_portSectionHdr->setVisible(false);
    if (m_frontPortSection) m_frontPortSection->setVisible(false);
    if (m_portFbLabel) { m_portFbLabel->setVisible(false); m_portFbLabel->setText("fb:"); }
    if (m_inFb)          m_inFb       ->setVisible(false);
    if (m_portQLLabel) { m_portQLLabel->setVisible(false); m_portQLLabel->setText("QL:"); }
    if (m_inQL)          m_inQL       ->setVisible(false);
    if (m_chambersFbLabel) m_chambersFbLabel->setVisible(false);
    if (m_chambersFb)      m_chambersFb     ->setVisible(false);
    if (m_chambersQLLabel) m_chambersQLLabel->setVisible(false);
    if (m_chambersQL)      m_chambersQL     ->setVisible(false);
    if (m_fbRearBpLabel) m_fbRearBpLabel->setVisible(false);
    if (m_inFbRearBp)    m_inFbRearBp   ->setVisible(false);
    if (m_qlRearBpLabel) m_qlRearBpLabel->setVisible(false);
    if (m_inQLRearBp)    m_inQLRearBp   ->setVisible(false);
    for (QLabel *l : {m_resAlpha, m_resFc, m_resQtc, m_resF3, m_resEta, m_resSpl})
        l->setText("–");
    m_driverCombo->blockSignals(true);
    m_driverCombo->setCurrentIndex(0);
    m_driverCombo->blockSignals(false);
    m_updating = false;
}

// ── Auto-naming ──────────────────────────────────────────────────

void EnclosureWidget::refreshAutoName(int index)
{
    if (index < 0 || index >= m_models.size()) return;
    auto &m = m_models[index];
    if (!m.autoName) return;

    // Driver label: combo text up to the first '(' or the full text
    QString driverPart;
    if (m.driverId >= 0) {
        const int ci = m_driverIds.indexOf(m.driverId);
        if (ci >= 0) {
            QString txt = m_driverCombo->itemText(ci);
            const int paren = txt.indexOf('(');
            driverPart = (paren > 0 ? txt.left(paren) : txt).trimmed();
        }
    }
    if (driverPart.isEmpty())
        driverPart = "Driver";

    // Volume
    QString volPart;
    if (isIB(m))             volPart = "IB";
    else if (isBandpass(m))  volPart = QString("%1+%2 L")
                                          .arg(m.volumeL,       0, 'f', 0)
                                          .arg(m.volumeFront_L, 0, 'f', 0);
    else                     volPart = QString("%1 L").arg(m.volumeL, 0, 'f', 0);

    // Type
    QString typePart;
    if      (isIB(m))     typePart = "IB";
    else if (isBP4(m))    typePart = "BP4";
    else if (isBP6(m))    typePart = "BP6";
    else if (isVented(m)) typePart = "vented";
    else                  typePart = "sealed";

    m.name = QString("%1 / %2 / %3").arg(driverPart, volPart, typePart);
}

// ── Calculation ──────────────────────────────────────────────────

void EnclosureWidget::recalculate(int index)
{
    if (index < 0 || index >= m_models.size()) return;
    auto &m = m_models[index];

    // Effective driver after baking in any added cone mass.  All scalar
    // results (Fc, Qtc, η, SPL, f3) are computed against this so the
    // displayed numbers reflect the loaded driver.
    const BoxModel me = withEffectiveMass(m);

    const double N       = static_cast<double>(me.numDrivers);
    const double Vas_raw = me.Vas_L * 1e-3;
    const double Vas_eff = Vas_raw * N;          // N series drivers share one box
    const double Vb_m3   = me.volumeL * 1e-3;

    if (isIB(me)) {
        // ── Infinite baffle ── Vb → ∞, α → 0; driver sees no box restoring force
        if (me.fs <= 0 || me.Qts <= 0 || me.Qes <= 0 || Vas_raw <= 0) {
            m.alpha = m.Fc = m.Qtc = m.f3 = m.eta = m.spl = 0;
            return;
        }
        m.alpha = 0.0;
        m.eta   = (4.0*PI*PI) * (me.fs*me.fs*me.fs) * Vas_eff / (g_C*g_C*g_C * me.Qes);
        m.spl   = SPL_REF_DB + 10.0 * std::log10(std::max(m.eta, 1e-30));
        m.Fc    = me.fs;
        m.Qtc   = me.Qts;
        const double a   = 1.0/(m.Qtc*m.Qtc) - 2.0;
        const double xsq = (a + std::sqrt(a*a + 4.0)) / 2.0;
        m.f3    = (xsq > 0) ? (m.Fc * std::sqrt(xsq)) : 0.0;
    } else {
        if (me.fs <= 0 || me.Qts <= 0 || me.Qes <= 0 || Vas_raw <= 0 || Vb_m3 <= 0) {
            m.alpha = m.Fc = m.Qtc = m.f3 = m.eta = m.spl = 0;
            return;
        }
        m.alpha = Vas_eff / Vb_m3;
        // Series wiring: Qes unchanged, η scales as N (not N²)
        m.eta   = (4.0*PI*PI) * (me.fs*me.fs*me.fs) * Vas_eff / (g_C*g_C*g_C * me.Qes);
        m.spl   = SPL_REF_DB + 10.0 * std::log10(std::max(m.eta, 1e-30));

        if (isBandpass(me)) {
            // ── Bandpass (BP4 / BP6) ──────────────────────────────────
            m.Fc  = me.fbFront;   // show front-port tuning in Fc slot
            m.Qtc = 0.0;
            const auto bm = bandpassMetrics(me);
            m.f3Low            = bm.f3Low;
            m.f3High           = bm.f3High;
            m.peakSpl          = bm.peakDb;
            m.passbandRippleDb = bm.ripple;
            m.f3               = bm.f3Low;  // legacy field
            m.alpha = 0.0;                  // not meaningful for BP
        } else if (isSealed(me)) {
            // ── Sealed box ───────────────────────────────────────────
            const double k = std::sqrt(m.alpha + 1.0);
            m.Fc  = me.fs  * k;
            m.Qtc = me.Qts * k;
            const double a   = 1.0/(m.Qtc*m.Qtc) - 2.0;
            const double xsq = (a + std::sqrt(a*a + 4.0)) / 2.0;
            m.f3  = (xsq > 0) ? (m.Fc * std::sqrt(xsq)) : 0.0;
        } else {
            // ── Vented (ported) box ───────────────────────────────────
            m.Fc  = me.fb;  // store port tuning in Fc slot for display
            m.Qtc = 0.0;    // no single Qtc for a vented system
            m.f3  = portedF3(me, 0.0 /* power: small-signal f3 */);
        }
    }

    // Update result labels if this is the active model
    if (index == m_activeIdx) {
        auto fmt = [](double v, int d){ return QString::number(v,'f',d); };
        m_resAlpha->setText(isIB(m) ? "–" : fmt(m.alpha, 3));
        m_resFc   ->setText(m.Fc > 0 ? fmt(m.Fc, 2) : "–");
        m_resQtc  ->setText(isVented(m) ? "–" : (m.Qtc > 0 ? fmt(m.Qtc, 3) : "–"));
        m_resF3   ->setText(m.f3 > 0 ? fmt(m.f3, 2) : "–");
        m_resEta  ->setText(fmt(m.eta * 100.0, 3));
        m_resSpl  ->setText(fmt(m.spl, 2));
    }
}

void EnclosureWidget::recalculateAll()
{
    for (int i = 0; i < m_models.size(); ++i)
        recalculate(i);
}

void EnclosureWidget::updatePlot()
{
    // Bake any added cone mass into a per-model effective copy before
    // handing to the plots — they read fs/Qms/mms/etc. directly when
    // computing curves and have no notion of addedMass_g.
    QList<BoxModel> eff;
    eff.reserve(m_models.size());
    for (const auto &m : m_models) {
        BoxModel mm = withEffectiveMass(m);
        // Preserve precomputed result fields populated by recalculate()
        mm.alpha = m.alpha; mm.Fc = m.Fc; mm.Qtc = m.Qtc;
        mm.f3 = m.f3; mm.eta = m.eta; mm.spl = m.spl;
        mm.portF2H = m.portF2H;
        eff.append(mm);
    }
    // Filter out models the user has hidden via the model list.
    // Visibility is per-EnclosureWidget state — the plots never see
    // the flag, so their existing index logic (cursor, legend, active
    // highlight) keeps working unchanged.
    QList<BoxModel> visible;
    int visActive = -1;
    visible.reserve(eff.size());
    for (int i = 0; i < eff.size(); ++i) {
        if (!eff[i].visible) continue;
        if (i == m_activeIdx) visActive = visible.size();
        visible.append(eff[i]);
    }
    m_splPlot ->setModels(visible, visActive);
    m_gdPlot  ->setModels(visible, visActive);
    m_vpPlot  ->setModels(visible, visActive);
    m_excPlot ->setModels(visible, visActive);
    m_pvPlot  ->setModels(visible, visActive);
}

// ── T/S field locking ────────────────────────────────────────────

void EnclosureWidget::lockTsFields()
{
    m_tsLocked = true;
    const QString s = themed(
        "QDoubleSpinBox{"
        "  background:%sunken%; color:%disabled%;"
        "  border:1px solid %border%; border-radius:0px;"
        "  font-family:'IBM Plex Mono',monospace; padding:4px 8px;"
        "}");
    for (auto *sp : {m_inFs, m_inVas, m_inQts, m_inQes,
                     m_dpQms, m_dpRe, m_dpMms, m_dpBL, m_dpSd}) {
        sp->setReadOnly(true);
        sp->setStyleSheet(s);
    }
}

void EnclosureWidget::unlockTsFields()
{
    m_tsLocked = false;
    const QString s = themed(
        "QDoubleSpinBox{"
        "  background:%inputFocus%; color:%accentLt%;"
        "  border:1px solid %border%; border-bottom:2px solid %accent%;"
        "  border-radius:0px;"
        "  font-family:'IBM Plex Mono',monospace; padding:4px 8px;"
        "}");
    for (auto *sp : {m_inFs, m_inVas, m_inQts, m_inQes,
                     m_dpQms, m_dpRe, m_dpMms, m_dpBL, m_dpSd}) {
        sp->setReadOnly(false);
        sp->setStyleSheet(s);
    }
}

bool EnclosureWidget::eventFilter(QObject *obj, QEvent *event)
{
    // After a drag-drop reorder completes, sync m_models to the new visual order.
    // We defer with a queued call so Qt's drop handling finishes first.
    if (m_modelList && obj == m_modelList->viewport()
        && event->type() == QEvent::Drop) {
        QMetaObject::invokeMethod(this, [this]() {
            if (m_modelList->count() != m_models.size()) return;
            QList<BoxModel> reordered;
            reordered.reserve(m_models.size());
            int newActive = -1;
            for (int i = 0; i < m_modelList->count(); ++i) {
                const int orig = m_modelList->item(i)->data(Qt::UserRole).toInt();
                if (orig < 0 || orig >= m_models.size()) return;
                if (orig == m_activeIdx) newActive = i;
                reordered.append(m_models[orig]);
            }
            m_models = reordered;
            m_activeIdx = newActive;
            updateModelList();
            updatePlot();
        }, Qt::QueuedConnection);
        return false;  // let Qt finish the drop
    }
    if (m_modelList && obj == m_modelList->viewport()
        && event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton) {
            QListWidgetItem *item = m_modelList->itemAt(me->pos());
            if (item) {
                const int row = m_modelList->row(item);
                const QRect r = m_modelList->visualItemRect(item);
                // Swatch icon occupies the leftmost ~22 px of the row
                // (14 px pixmap + small left/right padding).
                const QRect swatchHit(r.left(), r.top(), 22, r.height());
                if (swatchHit.contains(me->pos())
                    && row >= 0 && row < m_models.size()) {
                    m_models[row].visible = !m_models[row].visible;
                    updateModelList();
                    updatePlot();
                    return true;
                }
            }
        }
    }
    if (event->type() == QEvent::MouseButtonDblClick && m_tsLocked) {
        for (auto *s : {m_inFs, m_inVas, m_inQts, m_inQes,
                        m_dpQms, m_dpRe, m_dpMms, m_dpBL, m_dpSd}) {
            if (obj == s->findChild<QLineEdit*>()) {
                const auto ans = QMessageBox::warning(
                    this, "Edit T/S Parameters",
                    "You are about to manually override the Thiele/Small parameters.\n"
                    "This will not update the driver database record.\n\n"
                    "Allow editing?",
                    QMessageBox::Ok | QMessageBox::Cancel,
                    QMessageBox::Cancel);
                if (ans == QMessageBox::Ok)
                    unlockTsFields();
                return true;  // consume the event either way
            }
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Serialization helpers ─────────────────────────────────────────

static QJsonObject modelToJson(const BoxModel &m)
{
    QJsonObject obj;
    obj["name"]      = m.name;
    obj["driverId"]  = m.driverId;
    obj["fs"]        = m.fs;
    obj["Vas_L"]     = m.Vas_L;
    obj["Qts"]       = m.Qts;
    obj["Qes"]       = m.Qes;
    obj["Qms"]       = m.Qms;
    obj["Re"]        = m.Re;
    obj["mms_g"]     = m.mms_g;
    obj["BL"]        = m.BL;
    obj["Sd_cm2"]    = m.Sd_cm2;
    obj["volumeL"]   = m.volumeL;
    {
        const char *s = "sealed";
        switch (m.encType) {
        case BoxModel::EncType::Sealed:    s = "sealed";    break;
        case BoxModel::EncType::Vented:    s = "vented";    break;
        case BoxModel::EncType::IB:        s = "ib";        break;
        case BoxModel::EncType::Bandpass4: s = "bandpass4"; break;
        case BoxModel::EncType::Bandpass6: s = "bandpass6"; break;
        }
        obj["encType"] = s;
    }
    // Legacy bools for forward-compat reads by older builds
    obj["isVented"]          = isVented(m) || isBP4(m) || isBP6(m);
    obj["isInfiniteBaffle"]  = isIB(m);
    obj["fb"]             = m.fb;
    obj["QL"]             = m.QL;
    // Bandpass front-chamber persistence
    obj["volumeFront_L"]    = m.volumeFront_L;
    obj["fbFront"]          = m.fbFront;
    obj["QLFront"]          = m.QLFront;
    obj["portFrontShape"]      = m.portFrontShape;
    obj["portFrontWidth_mm"]   = m.portFrontWidth_mm;
    obj["portFrontHeight_mm"]  = m.portFrontHeight_mm;
    obj["portFrontWalls"]      = m.portFrontWalls;
    obj["numPortsFront"]       = m.numPortsFront;
    obj["portFrontWallThick_mm"]      = m.portFrontWallThick_mm;
    obj["portFrontInsertDepth_mm"]    = m.portFrontInsertDepth_mm;
    obj["portFrontExtraSurfArea_cm2"] = m.portFrontExtraSurfArea_cm2;
    obj["portFrontFlare"]             = m.portFrontFlare;
    obj["portShape"]           = m.portShape;
    obj["portWidth_mm"]        = m.portWidth_mm;
    obj["portHeight_mm"]       = m.portHeight_mm;
    obj["portWalls"]           = m.portWalls;
    obj["numPorts"]            = m.numPorts;
    obj["numDrivers"]          = m.numDrivers;
    obj["wiringMode"]          = static_cast<int>(m.wiringMode);
    obj["portWallThick_mm"]       = m.portWallThick_mm;
    obj["portInsertDepth_mm"]     = m.portInsertDepth_mm;
    obj["portExtraSurfArea_cm2"]  = m.portExtraSurfArea_cm2;
    obj["portFlare"]              = m.portFlare;
    obj["autoName"]            = m.autoName;
    obj["color"]               = m.color.name();
    obj["isDVC"]               = m.isDVC;
    obj["dvcWiring"]           = m.dvcWiring;
    obj["addedMass_g"]         = m.addedMass_g;
    obj["visible"]             = m.visible;
    return obj;
}

static BoxModel jsonToModel(const QJsonObject &obj, int fallbackColorIdx)
{
    BoxModel m;
    m.name      = obj["name"].toString();
    m.driverId  = obj["driverId"].toInt(-1);
    m.fs        = obj["fs"].toDouble();
    m.Vas_L     = obj["Vas_L"].toDouble();
    m.Qts       = obj["Qts"].toDouble();
    m.Qes       = obj["Qes"].toDouble();
    m.Qms       = obj["Qms"].toDouble();
    m.Re        = obj["Re"].toDouble();
    m.mms_g     = obj["mms_g"].toDouble();
    m.BL        = obj["BL"].toDouble();
    m.Sd_cm2    = obj["Sd_cm2"].toDouble();
    m.volumeL   = obj["volumeL"].toDouble(40.0);
    if (obj.contains("encType")) {
        const QString s = obj["encType"].toString("sealed");
        if      (s == "vented")    m.encType = BoxModel::EncType::Vented;
        else if (s == "ib")        m.encType = BoxModel::EncType::IB;
        else if (s == "bandpass4") m.encType = BoxModel::EncType::Bandpass4;
        else if (s == "bandpass6") m.encType = BoxModel::EncType::Bandpass6;
        else                       m.encType = BoxModel::EncType::Sealed;
    } else if (obj["isInfiniteBaffle"].toBool(false)) {
        m.encType = BoxModel::EncType::IB;
    } else if (obj["isVented"].toBool(false)) {
        m.encType = BoxModel::EncType::Vented;
    } else {
        m.encType = BoxModel::EncType::Sealed;
    }
    m.fb             = obj["fb"].toDouble(35.0);
    m.QL             = obj["QL"].toDouble(7.0);
    m.volumeFront_L      = obj["volumeFront_L"].toDouble(40.0);
    m.fbFront            = obj["fbFront"].toDouble(60.0);
    m.QLFront            = obj["QLFront"].toDouble(7.0);
    m.portFrontShape     = std::clamp(obj["portFrontShape"].toInt(0), 0, 1);
    m.portFrontWidth_mm  = obj["portFrontWidth_mm"].toDouble(75.0);
    m.portFrontHeight_mm = obj["portFrontHeight_mm"].toDouble(50.0);
    m.portFrontWalls     = std::clamp(obj["portFrontWalls"].toInt(0), 0, 3);
    m.numPortsFront      = std::max(1, obj["numPortsFront"].toInt(1));
    m.portFrontWallThick_mm   = obj["portFrontWallThick_mm"].toDouble(3.0);
    m.portFrontInsertDepth_mm = obj["portFrontInsertDepth_mm"].toDouble(0.0);
    m.portFrontExtraSurfArea_cm2 = obj["portFrontExtraSurfArea_cm2"].toDouble(0.0);
    m.portFrontFlare     = std::clamp(obj["portFrontFlare"].toInt(0), 0, 2);
    m.portShape     = obj["portShape"].toInt(0);
    m.portWidth_mm  = obj["portWidth_mm"].toDouble(75.0);
    m.portHeight_mm = obj["portHeight_mm"].toDouble(50.0);
    // portWalls: fall back to old portSharedWall bool for backward compat
    if (obj.contains("portWalls"))
        m.portWalls = std::clamp(obj["portWalls"].toInt(0), 0, 3);
    else
        m.portWalls = obj["portSharedWall"].toBool(false) ? 1 : 0;
    m.numPorts            = std::max(1, obj["numPorts"].toInt(1));
    m.numDrivers  = std::max(1, obj["numDrivers"].toInt(1));
    m.wiringMode  = static_cast<BoxModel::WiringMode>(std::clamp(obj["wiringMode"].toInt(0), 0, 2));
    m.portWallThick_mm        = obj["portWallThick_mm"].toDouble(3.0);
    m.portInsertDepth_mm      = obj["portInsertDepth_mm"].toDouble(0.0);
    m.portExtraSurfArea_cm2   = obj["portExtraSurfArea_cm2"].toDouble(0.0);
    m.portFlare               = std::clamp(obj["portFlare"].toInt(0), 0, 2);
    // autoName: old files lack the key → treat as user-named to preserve saved names
    m.autoName  = obj["autoName"].toBool(false);
    m.isDVC     = obj["isDVC"].toBool(false);
    m.dvcWiring = obj["dvcWiring"].toInt(0);
    m.addedMass_g = obj["addedMass_g"].toDouble(0.0);
    m.visible     = obj["visible"].toBool(true);
    const QString cstr = obj["color"].toString();
    m.color = cstr.isEmpty() ? paletteColor(fallbackColorIdx) : QColor(cstr);
    return m;
}

QString EnclosureWidget::serializeModels() const
{
    QJsonArray arr;
    for (const auto &m : m_models)
        arr.append(modelToJson(m));
    return QJsonDocument(arr).toJson(QJsonDocument::Indented);
}

void EnclosureWidget::deserializeModels(const QString &json)
{
    m_models.clear();
    const auto arr = QJsonDocument::fromJson(json.toUtf8()).array();
    for (int i = 0; i < arr.size(); ++i)
        m_models.append(jsonToModel(arr[i].toObject(), i));
    recalculateAll();
    m_activeIdx = m_models.isEmpty() ? -1 : 0;
    updateModelList();
    if (m_activeIdx >= 0) loadModelIntoFields(m_activeIdx);
    else clearFields();
    updatePlot();
}

// ── File save / load ─────────────────────────────────────────────

static QString sanitizeFilename(const QString &s)
{
    QString out = s;
    out.replace(" / ", "_");
    out.replace(' ', '_');
    out.remove(QRegularExpression(R"([^A-Za-z0-9_\-+.])"));
    out.replace(QRegularExpression("_+"), "_");
    out.remove(QRegularExpression("^_+|_+$"));
    return out.isEmpty() ? "enclosure" : out;
}

void EnclosureWidget::onSaveModel()
{
    if (m_activeIdx < 0 || m_activeIdx >= m_models.size()) {
        QMessageBox::information(this, "No model selected",
            "Select a model from the list first.");
        return;
    }
    const auto &model = m_models[m_activeIdx];
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Model",
        QDir::homePath() + "/" + sanitizeFilename(model.name) + ".tsbox",
        "TSBoss Model (*.tsbox)");
    if (path.isEmpty()) return;

    QJsonObject root;
    root["type"]  = "tsbox";
    root["model"] = modelToJson(model);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save error", "Cannot open file for writing.");
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    m_statusLbl->setStyleSheet(
        "color:#A3E635; font-family:'IBM Plex Mono',monospace; font-size:8pt;");
    m_statusLbl->setText("Saved: " + QFileInfo(path).fileName());
}

void EnclosureWidget::onLoadModel()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Load Model", QDir::homePath(),
        "TSBoss Model (*.tsbox)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Load error", "Cannot open file.");
        return;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto root = doc.object();

    if (root["type"].toString() != "tsbox" || !root.contains("model")) {
        QMessageBox::critical(this, "Load error",
            "Not a valid .tsbox file.");
        return;
    }

    BoxModel m = jsonToModel(root["model"].toObject(), m_models.size());
    m.color = nextColor();
    m_models.append(m);
    m_activeIdx = m_models.size() - 1;
    recalculate(m_activeIdx);
    updateModelList();
    loadModelIntoFields(m_activeIdx);
    updatePlot();

    m_statusLbl->setStyleSheet(
        "color:#A3E635; font-family:'IBM Plex Mono',monospace; font-size:8pt;");
    m_statusLbl->setText("Loaded: " + m.name);
}

QString EnclosureWidget::deriveProjectFileBaseName(const QList<BoxModel> &models,
                                                   DriverDatabase *db)
{
    QStringList brands;
    QSet<int> seenIds;
    for (const auto &m : models) {
        if (m.driverId >= 0 && !seenIds.contains(m.driverId) &&
            db && db->isOpen()) {
            seenIds.insert(m.driverId);
            const auto r = db->loadDriver(m.driverId);
            if (!r.make.isEmpty()) {
                QString b = sanitizeFilename(r.make);
                b.replace('-', '_');
                if (!brands.contains(b, Qt::CaseInsensitive))
                    brands.append(b);
            }
        }
    }
    brands.sort(Qt::CaseInsensitive);
    return brands.isEmpty()
        ? QStringLiteral("enclosure")
        : QString("%1x_%2").arg(models.size()).arg(brands.join('-'));
}

void EnclosureWidget::onSaveProject()
{
    if (m_models.isEmpty()) {
        QMessageBox::information(this, "No models",
            "Add at least one model before saving a project.");
        return;
    }
    const QString stem = deriveProjectFileBaseName(m_models, m_db);

    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project", QDir::homePath() + "/" + stem + ".tsproj",
        "TSBoss Project (*.tsproj)");
    if (path.isEmpty()) return;

    QJsonObject root;
    root["type"]   = "tsproj";
    QJsonArray arr;
    for (const auto &m : m_models)
        arr.append(modelToJson(m));
    root["models"] = arr;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Save error", "Cannot open file for writing.");
        return;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    m_statusLbl->setStyleSheet(
        "color:#A3E635; font-family:'IBM Plex Mono',monospace; font-size:8pt;");
    m_statusLbl->setText("Project saved: " + QFileInfo(path).fileName());
}

void EnclosureWidget::onLoadProject()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Load Project", QDir::homePath(),
        "TSBoss Project (*.tsproj)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Load error", "Cannot open file.");
        return;
    }
    const auto doc = QJsonDocument::fromJson(f.readAll());
    const auto root = doc.object();

    if (root["type"].toString() != "tsproj" || !root.contains("models")) {
        QMessageBox::critical(this, "Load error",
            "Not a valid .tsproj file.");
        return;
    }

    m_models.clear();
    const auto arr = root["models"].toArray();
    for (int i = 0; i < arr.size(); ++i)
        m_models.append(jsonToModel(arr[i].toObject(), i));
    recalculateAll();
    m_activeIdx = m_models.isEmpty() ? -1 : 0;
    updateModelList();
    if (m_activeIdx >= 0) loadModelIntoFields(m_activeIdx);
    else clearFields();
    updatePlot();

    m_statusLbl->setStyleSheet(
        "color:#A3E635; font-family:'IBM Plex Mono',monospace; font-size:8pt;");
    m_statusLbl->setText("Project loaded: " + QFileInfo(path).fileName());
}

void EnclosureWidget::onExportPdf()
{
    if (m_models.isEmpty()) {
        QMessageBox::information(this, "No models",
            "Add at least one model before exporting a PDF report.");
        return;
    }

    PdfReportOptions opts;
    opts.projectName    = deriveProjectFileBaseName(m_models, m_db);
    opts.selectedIndices.reserve(m_models.size());
    for (int i = 0; i < m_models.size(); ++i)
        opts.selectedIndices.append(i);
    opts.appliedPower   = m_splPower ? m_splPower->value() : 1.0;
    opts.perDriverMode  = m_perDriverPower && m_perDriverPower->isChecked();

    PdfReport::exportToFile(this, m_models, opts, m_db);
}
