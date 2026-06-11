#include "modelplots.h"
#include "modeleval.h"
#include "portphysics.h"
#include "bandpassphysics.h"
#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QVector>
#include <cmath>

using namespace pp;

// ════════════════════════════════════════════════════════════════════
//  ResponsePlot – multi-curve
// ════════════════════════════════════════════════════════════════════
ResponsePlot::ResponsePlot(QWidget *parent) : PlotBase(300, 1.0, parent) {}


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
                const double db  = m.spl + pwrOffset + hpfDb(m, f) + 20.0*std::log10(raw / ref);
                if (std::isfinite(db)) { yMax = std::max(yMax, db); yMin = std::min(yMin, db); }
            }
            anyValid = true;
        } else if (isBandpass(m)) {
            const bool bp6 = isBP6(m);
            if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) continue;
            double ref = 0.0;
            for (int i = 0; i <= SPL_SCAN; ++i) {
                const double f = std::pow(10.0, lfMin + (lfMax - lfMin)*i/double(SPL_SCAN));
                ref = std::max(ref, bandpassSplRaw(m, f, mp));
            }
            if (ref <= 0) continue;
            const double peakDb = (m.peakSpl > 0) ? m.peakSpl : 100.0;
            for (int i = 0; i <= SPL_SCAN; ++i) {
                const double f   = std::pow(10.0, lfMin + (lfMax - lfMin)*i/double(SPL_SCAN));
                const double raw = bandpassSplRaw(m, f, mp);
                if (raw <= 0) continue;
                const double db  = peakDb + pwrOffset + hpfDb(m, f) + 20.0*std::log10(raw / ref);
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
                const double db  = m.spl + pwrOffset + hpfDb(m, f) + 10.0*std::log10(xsq*xsq/den);
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
                    const double db  = m.spl + pwrOffset + hpfDb(m, f) + 20.0*std::log10(raw / ref);
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
            const double mp = modelPower(m);
            if (bp6 ? !hasBP6Data(m) : !hasBP4Data(m)) return;
            // Find raw peak; normalise so peak == m.peakSpl
            double ref = 0.0;
            for (int i = 0; i <= N; ++i) {
                const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/double(N));
                ref = std::max(ref, bandpassSplRaw(m, f, mp));
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
                    const double db  = peakDb + pwrOffset + hpfDb(m, f) + 20.0*std::log10(raw / ref);
                    const QPointF pt(xPx(f), yPx(db));
                    if (first) { path.moveTo(pt); first = false; }
                    else       { path.lineTo(pt); }
                }
                return path;
            };
            // Total
            {
                QPainterPath path = buildBPath([&](const BoxModel &mm, double ff){ return bandpassSplRaw(mm, ff, mp); });
                QColor c = m.color; if (!active) c.setAlpha(100);
                p.setPen(QPen(c, active ? 3.0 : 1.8, Qt::SolidLine));
                p.setBrush(Qt::NoBrush); p.drawPath(path);
            }
            // For BP6: show individual port contributions (total = front − rear,
            // so the decomposition is meaningful).  For BP4: total == front port,
            // no useful decomposition to draw.
            if (bp6) {
                // Front port — dotted
                QPainterPath fpPath = buildBPath([&](const BoxModel &mm, double ff){ return bandpassFrontPortSplRaw(mm, ff, mp); });
                QColor cf = m.color; cf.setAlpha(active ? 160 : 70);
                p.setPen(QPen(cf, active ? 1.8 : 1.2, Qt::DotLine));
                p.setBrush(Qt::NoBrush); p.drawPath(fpPath);
                // Rear port — dashed
                QPainterPath rpPath = buildBPath([&](const BoxModel &mm, double ff){ return bandpassRearPortSplRaw(mm, ff, mp); });
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
                const double db = m.spl + pwrOffset + hpfDb(m, f) + 10.0*std::log10(xsq*xsq/den);
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
                const double dbTotal = m.spl + pwrOffset + hpfDb(m, cf) + 20.0*std::log10(portedSplRaw(m, cf, mp) / ref);
                entries.append({m.color, active,
                                m.name + " (total)",
                                yPx(dbTotal), QString("%1 dB").arg(dbTotal, 0, 'f', 1)});
                // Cone
                QColor cc = m.color; cc.setAlpha(active ? 200 : 120);
                const double rawCone = portedConeSplRaw(m, cf, mp);
                if (rawCone > 0) {
                    const double dbCone = m.spl + pwrOffset + hpfDb(m, cf) + 20.0*std::log10(rawCone / ref);
                    entries.append({cc, false,
                                    "  cone",
                                    yPx(dbCone), QString("%1 dB").arg(dbCone, 0, 'f', 1)});
                }
                // Port
                const double rawPort = portedPortSplRaw(m, cf, mp);
                if (rawPort > 0) {
                    const double dbPort = m.spl + pwrOffset + hpfDb(m, cf) + 20.0*std::log10(rawPort / ref);
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
                const double db = m.spl + pwrOffset + hpfDb(m, cf) + 10.0*std::log10(xsq*xsq/den);
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
GroupDelayPlot::GroupDelayPlot(QWidget *parent) : PlotBase(280, 1.0, parent) {}


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
                    const double gd = portedGroupDelay(m, f, 0.0 /* linear limit */) + hpfGroupDelayMs(m, f);
                    if (std::isfinite(gd) && gd > 0) yMax = std::max(yMax, gd);
                }
                anyValid = true;
            } else if (isBandpass(m)) {
                if (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) continue;
                for (int i = 0; i <= GD_SCAN; ++i) {
                    const double f  = std::pow(10.0, lfLo + (lfHi-lfLo)*i/double(GD_SCAN));
                    const double gd = bandpassGroupDelay(m, f, 0.0 /* linear limit */) + hpfGroupDelayMs(m, f);
                    if (std::isfinite(gd) && gd > 0) yMax = std::max(yMax, gd);
                }
                anyValid = true;
            } else if (m.Fc > 0 && m.Qtc > 0) {
                const double peak_ms = 1000.0 / (2.0*PI*m.Fc*m.Qtc) + hpfGroupDelayMs(m, m.hpfFreq);
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
                const double gd = portedGroupDelay(m, f, 0.0 /* linear limit */) + hpfGroupDelayMs(m, f);
                if (gd < 0 || gd > yMax*2) continue;
                const QPointF pt(xPx(f), yPx(gd));
                if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
            }
        } else if (isBandpass(m)) {
            if (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) return;
            for (int i = 0; i <= 500; ++i) {
                const double lf = lfMin + (lfMax-lfMin)*i/500.0;
                const double f  = std::pow(10.0, lf);
                const double gd = bandpassGroupDelay(m, f, 0.0 /* linear limit */) + hpfGroupDelayMs(m, f);
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
                const double gd = 1000.0/wc * (1.0/m.Qtc) * (1.0+x2)/den + hpfGroupDelayMs(m, f);
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
                gd = portedGroupDelay(m, cf, 0.0 /* linear limit */) + hpfGroupDelayMs(m, cf);
            } else if (isBandpass(m)) {
                if (isBP6(m) ? !hasBP6Data(m) : !hasBP4Data(m)) continue;
                gd = bandpassGroupDelay(m, cf, 0.0 /* linear limit */) + hpfGroupDelayMs(m, cf);
            } else {
                if (m.Fc <= 0 || m.Qtc <= 0) continue;
                const double x  = cf/m.Fc;
                const double x2 = x*x;
                const double den = (x/m.Qtc)*(x/m.Qtc) + (1.0-x2)*(1.0-x2);
                if (den <= 0) continue;
                gd = 1000.0/(2.0*PI*m.Fc) * (1.0/m.Qtc) * (1.0+x2)/den + hpfGroupDelayMs(m, cf);
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
VoltagePlot::VoltagePlot(QWidget *parent) : PlotBase(280, 1.0, parent) {}



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
            const double mpf = mp * hpfPowerScale(m, f);
            const double Z  = systemImpedance(m, f, mpf);
            const double V  = std::sqrt(mpf * Z);
            const double I  = Z > 0 ? std::sqrt(mpf / Z) : 0.0;
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
            const double mpf = mp * hpfPowerScale(m, f);
            const double Z  = systemImpedance(m, f, mpf);
            if (Z <= 0) continue;
            const double V  = std::sqrt(mpf * Z);
            const double I  = std::sqrt(mpf / Z);
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
            const double mpf = mp * hpfPowerScale(m, m_cursorFreq);
            const double Z = systemImpedance(m, m_cursorFreq, mpf);
            if (Z <= 0) continue;
            const double V = std::sqrt(mpf * Z);
            const double I = std::sqrt(mpf / Z);
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


// ════════════════════════════════════════════════════════════════════
//  ExcursionPlot
// ════════════════════════════════════════════════════════════════════
ExcursionPlot::ExcursionPlot(QWidget *parent) : PlotBase(280, 1.0, parent) {}



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
            const double x  = coneDisplacement_mm(m, f, mp * hpfPowerScale(m, f));
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
            const double x = coneDisplacement_mm(m, f, mp * hpfPowerScale(m, f));
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
            const double x = coneDisplacement_mm(m, m_cursorFreq, modelPower(m) * hpfPowerScale(m, m_cursorFreq));
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


// ════════════════════════════════════════════════════════════════════
//  PortVelocityPlot
// ════════════════════════════════════════════════════════════════════


PortVelocityPlot::PortVelocityPlot(QWidget *parent) : PlotBase(280, 100.0, parent) {}


// One drawable velocity curve. A vented model yields one; a BP6 yields two
// (front + rear). Kept as a plain enum dispatch — no std::function in the draw loop.
enum class PVKind { Vented, BPFront, BPRear };

static bool pvHasData(const BoxModel &m, PVKind k)
{
    switch (k) {
        case PVKind::Vented:  return hasPortVelocityData(m);
        case PVKind::BPFront: return hasBandpassPortVelocityData(m, true);
        case PVKind::BPRear:  return hasBandpassPortVelocityData(m, false);
    }
    return false;
}

static double pvVelocity(const BoxModel &m, double f, double power, PVKind k)
{
    switch (k) {
        case PVKind::Vented:  return portAirVelocity(m, f, power);
        case PVKind::BPFront: return bandpassPortVelocity(m, f, power, true);
        case PVKind::BPRear:  return bandpassPortVelocity(m, f, power, false);
    }
    return 0.0;
}

static int pvFlare(const BoxModel &m, PVKind k)
{
    switch (k) {
        case PVKind::BPFront: return m.portFrontFlare;
        case PVKind::BPRear:  return m.portFlare;
        case PVKind::Vented:  return m.portFlare;
    }
    return m.portFlare;
}

struct PVCurve { int modelIdx; PVKind kind; QString suffix; Qt::PenStyle style; };

// Build the list of velocity curves a model contributes (0, 1, or 2).
static QVector<PVCurve> pvCurvesFor(const BoxModel &m, int idx)
{
    QVector<PVCurve> out;
    if (m.encType == BoxModel::EncType::Vented) {
        if (pvHasData(m, PVKind::Vented)) out.push_back({idx, PVKind::Vented, "", Qt::SolidLine});
    } else if (m.encType == BoxModel::EncType::Bandpass4
            || m.encType == BoxModel::EncType::Bandpass6) {
        // BP4 has only a front port (rear chamber sealed): hasBandpassPortVelocityData(m,false)
        // returns false for BP4, so BP4 naturally yields a single front curve; BP6 yields both.
        if (pvHasData(m, PVKind::BPFront)) out.push_back({idx, PVKind::BPFront, " (front)", Qt::SolidLine});
        if (pvHasData(m, PVKind::BPRear))  out.push_back({idx, PVKind::BPRear,  " (rear)",  Qt::DashLine});
    }
    return out;
}

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

    // Y range: scan all curves
    QVector<PVCurve> curves;
    for (int i = 0; i < m_models.size(); ++i)
        curves += pvCurvesFor(m_models[i], i);

    double yMax = 1.0;
    bool anyValid = !curves.isEmpty();
    for (const auto &cv : curves) {
        const BoxModel &m = m_models[cv.modelIdx];
        const double mp = modelPower(m);
        for (int i = 0; i <= 80; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/80.0);
            const double v = pvVelocity(m, f, mp * hpfPowerScale(m, f), cv.kind);
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

    // Per-port chuffing limit line(s) for the active model. A BP6 with different
    // front/rear flares shows two; equal flares (and vented/BP4) collapse to one.
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size()) {
        const BoxModel &am = m_models[m_activeIdx];
        QVector<double> limits;
        for (const auto &cv : curves) {
            if (cv.modelIdx != m_activeIdx) continue;
            const double lim = chuffLimit(pvFlare(am, cv.kind));
            if (!limits.contains(lim)) limits.push_back(lim);
        }
        for (double lim : limits) {
            if (lim < yMin || lim > yMax) continue;
            p.setPen(QPen(Theme::instance().statusError(), 1.0, Qt::DashLine));
            p.drawLine(QPointF(area.left(), yPx(lim)), QPointF(area.right(), yPx(lim)));
            QFont rf; rf.setPointSize(7); p.setFont(rf);
            p.setPen(Theme::instance().statusError());
            p.drawText(QRectF(area.left()+4, yPx(lim)-13, 90, 12),
                       Qt::AlignLeft|Qt::AlignVCenter,
                       QString("%1 m/s limit").arg(int(std::round(lim))));
        }
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
        p.drawText(area, Qt::AlignCenter, "Select a ported or bandpass enclosure to see port velocity.");
        return;
    }

    // Curves
    auto drawPV = [&](const PVCurve &cv, bool active) {
        const BoxModel &m = m_models[cv.modelIdx];
        const double mp = modelPower(m);
        QPainterPath curve; bool first = true;
        for (int i = 0; i <= 500; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/500.0);
            const double v = pvVelocity(m, f, mp * hpfPowerScale(m, f), cv.kind);
            if (!std::isfinite(v)) continue;
            const QPointF pt(xPx(f), yPx(v));
            if (first) { curve.moveTo(pt); first = false; } else curve.lineTo(pt);
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setPen(QPen(c, active ? 3.0 : 1.8, cv.style));
        p.setBrush(Qt::NoBrush);
        p.drawPath(curve);
    };

    p.setClipRect(area);
    for (const auto &cv : curves)
        if (cv.modelIdx != m_activeIdx) drawPV(cv, false);
    for (const auto &cv : curves)
        if (cv.modelIdx == m_activeIdx) drawPV(cv, true);
    p.setClipping(false);

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-180; int ly = int(area.top())+8;
        for (const auto &cv : curves) {
            const BoxModel &m = m_models[cv.modelIdx];
            bool active = (cv.modelIdx == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5, cv.style));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx+24, ly, 150, 14), Qt::AlignLeft|Qt::AlignVCenter, m.name + cv.suffix);
            ly += 16;
        }
    }

    // Cursor overlay
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        for (const auto &cv : curves) {
            const BoxModel &m = m_models[cv.modelIdx];
            const double v = pvVelocity(m, m_cursorFreq, modelPower(m) * hpfPowerScale(m, m_cursorFreq), cv.kind);
            if (!std::isfinite(v)) continue;
            entries.append({m.color, cv.modelIdx == m_activeIdx, m.name + cv.suffix,
                            yPx(v), QString("%1 m/s").arg(v, 0, 'f', 2)});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
}



// ════════════════════════════════════════════════════════════════════
//  ImpedancePlot
// ════════════════════════════════════════════════════════════════════
ImpedancePlot::ImpedancePlot(QWidget *parent) : PlotBase(280, 1.0, parent) {}

void ImpedancePlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 16, mt = 16, mb = 42;
    const QRectF area(ml, mt, width()-ml-mr, height()-mt-mb);
    p.fillRect(area, CLR_PLOT_BG());

    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);

    // |Z| at the small-signal limit (power -> 0): matches what an
    // impedance sweep (DATS / REW) measures on the finished box.
    auto zOf = [](const BoxModel &m, double f) { return systemImpedance(m, f, 0.0); };

    // Y range: scan all models
    double yMax = 10.0;
    bool anyValid = false;
    for (const auto &m : m_models) {
        if (!hasSystemZData(m)) continue;
        anyValid = true;
        for (int i = 0; i <= 200; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/200.0);
            const double z = zOf(m, f);
            if (std::isfinite(z)) yMax = std::max(yMax, z);
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
        yMax *= 1.1;
        const double rawStep = yMax / 5.0;
        const double mag = std::pow(10.0, std::floor(std::log10(std::max(rawStep, 1e-9))));
        niceStep = std::ceil(rawStep / mag) * mag;
        yMax = std::ceil(yMax / niceStep) * niceStep;
    }

    auto xPx = [&](double f) { return area.left() + area.width()*(std::log10(f)-lfMin)/(lfMax-lfMin); };
    auto yPx = [&](double z) { return area.top()  + area.height()*(yMax-z)/(yMax-yMin); };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = {20,30,50,70,100,200,300,500,700,1000};
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    for (double z = yMin; z <= yMax+1e-9; z += niceStep)
        p.drawLine(QPointF(area.left(), yPx(z)), QPointF(area.right(), yPx(z)));

    // Axis labels
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = f>=1000 ? QString("%1k").arg(f/1000,0,'f',0) : QString::number(int(f));
        p.drawText(QRectF(xPx(f)-20, area.bottom()+2, 40, 14), Qt::AlignHCenter, lbl);
    }
    for (double z = yMin; z <= yMax+1e-9; z += niceStep)
        p.drawText(QRectF(0, yPx(z)-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, QString::number(int(std::round(z))));

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Impedance  (Ω)");
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
                   "Set Rₑ, BL, mms, and Qms to see the impedance curve.");
        return;
    }

    // Curves
    auto drawCurve = [&](const BoxModel &m, bool active) {
        if (!hasSystemZData(m)) return;
        QPainterPath curve; bool first = true;
        for (int i = 0; i <= 500; ++i) {
            const double f = std::pow(10.0, lfMin + (lfMax-lfMin)*i/500.0);
            const double z = zOf(m, f);
            if (!std::isfinite(z)) continue;
            const QPointF pt(xPx(f), yPx(z));
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

    // Markers for the active model: Re reference and fb / Fc
    if (m_activeIdx >= 0 && m_activeIdx < m_models.size()) {
        const auto &am = m_models[m_activeIdx];
        if (hasSystemZData(am)) {
            // Re — the DC floor the curve approaches
            if (am.Re > yMin && am.Re < yMax) {
                p.setPen(QPen(CLR_GREY_LT(), 1.0, Qt::DotLine));
                p.drawLine(QPointF(area.left(), yPx(am.Re)), QPointF(area.right(), yPx(am.Re)));
                p.setPen(CLR_GREY());
                QFont rf; rf.setPointSize(7); p.setFont(rf);
                p.drawText(QRectF(area.right()-42, yPx(am.Re)-13, 40, 12),
                           Qt::AlignRight|Qt::AlignVCenter, "Rₑ");
            }
            // Tuning marker: fb for ported (impedance minimum), Fc for sealed peak
            const bool ported = isPorted(am);
            const double markerFreq = ported ? (isBP4(am) ? am.fbFront : am.fb) : am.Fc;
            const QString markerLbl = ported
                ? QString("fb = %1 Hz").arg(markerFreq, 0, 'f', 1)
                : QString("Fc = %1 Hz").arg(markerFreq, 0, 'f', 1);
            if (markerFreq > F_MIN && markerFreq < F_MAX) {
                QColor mc = am.color; mc.setAlpha(180);
                p.setPen(QPen(mc, 1.2, Qt::DashLine));
                p.drawLine(QPointF(xPx(markerFreq), area.top()), QPointF(xPx(markerFreq), area.bottom()));
                p.setPen(mc);
                QFont bf; bf.setPointSize(8); bf.setBold(true); p.setFont(bf);
                p.drawText(QRectF(xPx(markerFreq) + 4, area.top() + 4, 100, 14),
                           Qt::AlignLeft, markerLbl);
            }
        }
    }

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-180; int ly = int(area.top())+8;
        for (int i = 0; i < m_models.size(); ++i) {
            const auto &m = m_models[i];
            if (!hasSystemZData(m)) continue;
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
            if (!hasSystemZData(m)) continue;
            const double z = zOf(m, m_cursorFreq);
            if (!std::isfinite(z)) continue;
            entries.append({m.color, i == m_activeIdx, m.name,
                            yPx(z), QString("%1 Ω").arg(z, 0, 'f', 1)});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
}
// ════════════════════════════════════════════════════════════════════
//  MaxSplPlot
// ════════════════════════════════════════════════════════════════════
MaxSplPlot::MaxSplPlot(QWidget *parent) : PlotBase(280, 1.0, parent) {}

void MaxSplPlot::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), CLR_PAGE_BG());

    const int ml = 58, mr = 16, mt = 16, mb = 42;
    const QRectF area(ml, mt, width()-ml-mr, height()-mt-mb);
    p.fillRect(area, CLR_PLOT_BG());

    const double lfMin = std::log10(F_MIN);
    const double lfMax = std::log10(F_MAX);

    // The max-power bisection is comparatively expensive, so each curve is
    // computed once into a point list and reused for range, draw and legend.
    constexpr int N = 200;
    struct Curve { int modelIdx; QVector<QPointF> pts; };   // x = f, y = dB
    QVector<Curve> curves;
    double yMax = -1e9, yMin = 1e9;
    for (int mi = 0; mi < m_models.size(); ++mi) {
        const BoxModel &m = m_models[mi];
        if (!hasMaxSplData(m)) continue;
        const SplContext ctx = makeSplContext(m);
        Curve cv; cv.modelIdx = mi;
        cv.pts.reserve(N + 1);
        for (int i = 0; i <= N; ++i) {
            const double f  = std::pow(10.0, lfMin + (lfMax-lfMin)*i/double(N));
            const double db = maxSplDb(m, ctx, f, maxPowerAt(m, f));
            if (!std::isfinite(db) || db < -20.0) continue;
            cv.pts.append(QPointF(f, db));
            yMax = std::max(yMax, db);
            yMin = std::min(yMin, db);
        }
        if (!cv.pts.isEmpty()) curves.append(cv);
    }
    const bool anyValid = !curves.isEmpty();
    if (!anyValid) { yMax = 120.0; yMin = 80.0; }

    if (m_yMin.has_value() && m_yMax.has_value()) {
        yMin = *m_yMin; yMax = *m_yMax;
    } else {
        yMax = std::ceil((yMax + 3.0) / 5.0) * 5.0;
        yMin = std::floor((yMin - 3.0) / 5.0) * 5.0;
        if (yMax - yMin < 10.0) yMax = yMin + 10.0;
    }

    auto xPx = [&](double f)  { return area.left() + area.width()*(std::log10(f)-lfMin)/(lfMax-lfMin); };
    auto yPx = [&](double db) { return area.top()  + area.height()*(yMax-db)/(yMax-yMin); };

    // Grid
    QFont small; small.setPointSize(8); p.setFont(small);
    p.setPen(QPen(CLR_GRID(), 1.0));
    const double gFreqs[] = {20,30,50,70,100,200,300,500,700,1000};
    for (double f : gFreqs)
        p.drawLine(QPointF(xPx(f), area.top()), QPointF(xPx(f), area.bottom()));
    for (double db = yMin; db <= yMax+0.01; db += 5.0)
        p.drawLine(QPointF(area.left(), yPx(db)), QPointF(area.right(), yPx(db)));

    // Axis labels
    p.setPen(CLR_GREY());
    for (double f : gFreqs) {
        QString lbl = f>=1000 ? QString("%1k").arg(f/1000,0,'f',0) : QString::number(int(f));
        p.drawText(QRectF(xPx(f)-20, area.bottom()+2, 40, 14), Qt::AlignHCenter, lbl);
    }
    for (double db = yMin; db <= yMax+0.01; db += 5.0)
        p.drawText(QRectF(0, yPx(db)-8, area.left()-4, 16),
                   Qt::AlignRight|Qt::AlignVCenter, QString::number(int(db)));

    // Axis titles
    p.setPen(CLR_GREY_DK());
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height()/2.0, -14.0, area.height(), 28.0),
               Qt::AlignCenter, "Max SPL  (dB / 1 m, Xmax & port limited)");
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
                   "Set the driver's Xmax (large-signal data) to see the SPL ceiling.");
        return;
    }

    // Curves
    auto drawCurve = [&](const Curve &cv, bool active) {
        const BoxModel &m = m_models[cv.modelIdx];
        QPainterPath path; bool first = true;
        for (const auto &pt : cv.pts) {
            const QPointF px(xPx(pt.x()), yPx(pt.y()));
            if (first) { path.moveTo(px); first = false; } else path.lineTo(px);
        }
        QColor c = m.color; if (!active) c.setAlpha(100);
        p.setPen(QPen(c, active ? 3.0 : 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    };

    p.setClipRect(area);
    for (const auto &cv : curves)
        if (cv.modelIdx != m_activeIdx) drawCurve(cv, false);
    for (const auto &cv : curves)
        if (cv.modelIdx == m_activeIdx) drawCurve(cv, true);
    p.setClipping(false);

    // Legend
    {
        QFont lf; lf.setPointSize(8); p.setFont(lf);
        const int lx = int(area.right())-180; int ly = int(area.top())+8;
        for (const auto &cv : curves) {
            const BoxModel &m = m_models[cv.modelIdx];
            bool active = (cv.modelIdx == m_activeIdx);
            QColor c = m.color; if (!active) c.setAlpha(140);
            p.setPen(QPen(c, active ? 2.5 : 1.5));
            p.drawLine(QPoint(lx, ly+6), QPoint(lx+20, ly+6));
            p.setPen(active ? CLR_GREY_DK() : CLR_GREY());
            QFont tf; tf.setPointSize(8); tf.setBold(active); p.setFont(tf);
            p.drawText(QRect(lx+24, ly, 150, 14), Qt::AlignLeft|Qt::AlignVCenter, m.name);
            ly += 16;
        }
    }

    // Cursor overlay — shows the ceiling, allowed power and the limiter
    if (m_cursorFreq > 0) {
        QVector<CursorEntry> entries;
        for (const auto &cv : curves) {
            const BoxModel &m = m_models[cv.modelIdx];
            const SplContext ctx = makeSplContext(m);
            const auto mp = maxPowerAt(m, m_cursorFreq);
            const double db = maxSplDb(m, ctx, m_cursorFreq, mp);
            if (!std::isfinite(db)) continue;
            const QString lim = mp.limiter == MaxSplPoint::Port ? "port" : "Xmax";
            const QString pw  = mp.pMax >= 100 ? QString::number(qRound(mp.pMax))
                                               : QString::number(mp.pMax, 'f', 1);
            entries.append({m.color, cv.modelIdx == m_activeIdx, m.name,
                            yPx(db), QString("%1 dB @ %2 W (%3)").arg(db, 0, 'f', 1)
                                     .arg(pw).arg(lim)});
        }
        drawCursorOverlay(p, area, xPx(m_cursorFreq), m_cursorFreq, entries);
    }
}