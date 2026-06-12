#include "plotbase.h"
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <cmath>

PlotBase::PlotBase(int minHeight, double defaultPower, QWidget *parent)
    : QWidget(parent), m_power(defaultPower)
{
    setMinimumSize(400, minHeight);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void PlotBase::setModels(const QList<BoxModel> &models, int activeIndex)
{
    m_models    = models;
    m_activeIdx = activeIndex;
    update();
}

void PlotBase::clear()
{
    m_models.clear();
    m_activeIdx = -1;
    update();
}

void PlotBase::setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
void PlotBase::resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }
void PlotBase::setPower(double watts)            { m_power = watts; update(); }
void PlotBase::setPerDriverMode(bool on)         { m_perDriverMode = on; update(); }

double PlotBase::modelPower(const BoxModel &m) const
{
    return m_power * (m_perDriverMode ? std::max(1, m.numDrivers) : 1);
}

void PlotBase::mouseMoveEvent(QMouseEvent *e) { plotUpdateCursor(this, e, m_cursorFreq); }
void PlotBase::leaveEvent(QEvent *)           { m_cursorFreq = -1.0; update(); }

// ─────────────────────────────────────────────────────────────────
//  Legend / marker helpers (shared by every plot)
// ─────────────────────────────────────────────────────────────────

void drawLegend(QPainter &p, const QRectF &area,
                const QVector<LegendEntry> &entries, bool anchorRight)
{
    if (entries.isEmpty()) return;

    QFont main;  main.setPointSize(8);
    QFont mainB = main; mainB.setBold(true);
    QFont subF;  subF.setPointSize(7);

    constexpr int sampleMain = 20, sampleSub = 16, gap = 4, subIndent = 8;
    qreal w = 0;
    for (const auto &e : entries) {
        const QFontMetricsF fm(e.sub ? subF : (e.active ? mainB : main));
        const int lead = e.sub ? subIndent + sampleSub : sampleMain;
        w = std::max(w, lead + gap + fm.horizontalAdvance(e.text));
    }
    w = std::min(w, area.width() * 0.45);
    const qreal lx = anchorRight ? area.right() - w - 6 : area.left() + 8;
    qreal ly = area.top() + 8;

    for (const auto &e : entries) {
        const int  rowH   = e.sub ? 13 : 16;
        const int  lead   = e.sub ? subIndent : 0;
        const int  sample = e.sub ? sampleSub : sampleMain;
        QColor c = e.color;
        if (e.sub)          c.setAlpha(e.active ? 160 : 90);
        else if (!e.active) c.setAlpha(140);
        p.setPen(QPen(c, e.sub ? (e.active ? 1.6 : 1.1)
                               : (e.active ? 2.5 : 1.5), e.style));
        p.drawLine(QPointF(lx + lead, ly + 6), QPointF(lx + lead + sample, ly + 6));

        const QFont &tf = e.sub ? subF : (e.active ? mainB : main);
        p.setFont(tf);
        p.setPen(e.sub ? (e.active ? CLR_GREY() : CLR_GREY_LT())
                       : (e.active ? CLR_GREY_DK() : CLR_GREY()));
        const QFontMetricsF fm(tf);
        const qreal tw = w - lead - sample - gap;
        p.drawText(QRectF(lx + lead + sample + gap, ly, tw, rowH),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   fm.elidedText(e.text, Qt::ElideMiddle, tw));
        ly += rowH;
    }
}

void drawMarkerLabel(QPainter &p, const QRectF &area, double x, double yTop,
                     const QColor &color, const QString &text)
{
    QFont bf; bf.setPointSize(8); bf.setBold(true);
    const QFontMetricsF fm(bf);
    QRectF r(0, 0, fm.horizontalAdvance(text) + 8, fm.height() + 2);
    r.moveTop(yTop);
    r.moveLeft(x + 4 + r.width() > area.right() ? x - 4 - r.width() : x + 4);

    QColor bg = Theme::instance().plotBg(); bg.setAlpha(215);
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, 2, 2);
    p.restore();
    p.setFont(bf);
    p.setPen(color);
    p.drawText(r, Qt::AlignCenter, text);
}

// ─────────────────────────────────────────────────────────────────
//  Cursor helpers (shared by every plot)
// ─────────────────────────────────────────────────────────────────

void plotUpdateCursor(QWidget *w, QMouseEvent *e, double &cursorFreq)
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

void drawCursorOverlay(QPainter &p, const QRectF &area,
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
