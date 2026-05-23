#include "diagrams/diagrampainter.h"
#include "theme.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPolygonF>
#include <cmath>

namespace {
const QColor kAirflow(0x4a, 0x90, 0xd9);   // semantic "air" accent (not the UI amber)
constexpr double kOutlineW = 1.25;
constexpr double kDimW     = 0.8;
constexpr double kHatchW   = 0.6;
constexpr double kTickW    = 0.6;
}

DiagramPainter::DiagramPainter(QPainter &p, const QRectF &area) : m_p(p), m_area(area) {}

void DiagramPainter::setOutlinePen()
{
    m_p.setPen(QPen(Theme::instance().textPrimary(), kOutlineW));
    m_p.setBrush(Qt::NoBrush);
}

void DiagramPainter::hatchedRect(const QRectF &r)
{
    m_p.save();
    QColor hc = Theme::instance().textPrimary();
    hc.setAlphaF(0.5);
    m_p.setPen(QPen(hc, kHatchW));
    m_p.setBrush(QBrush(hc, Qt::BDiagPattern));
    m_p.drawRect(r);
    // second pass: crisp outline drawn over the hatch's lighter border
    setOutlinePen();
    m_p.drawRect(r);
    m_p.restore();
}

void DiagramPainter::wall(const QPainterPath &path)
{
    m_p.save();
    setOutlinePen();
    m_p.drawPath(path);
    m_p.restore();
}

void DiagramPainter::arrowHead(QPointF tip, QPointF from, const QColor &c)
{
    QPointF d = tip - from;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1e-6) return;
    d /= len;
    const QPointF n(-d.y(), d.x());
    const double arrowHalfW = 4.0, arrowLen = 7.0;
    const QPointF base = tip - d * arrowLen;
    QPolygonF tri; tri << tip << (base + n * arrowHalfW) << (base - n * arrowHalfW);
    m_p.setPen(Qt::NoPen);
    m_p.setBrush(c);
    m_p.drawPolygon(tri);
}

void DiagramPainter::airflow(QPointF a, QPointF b)
{
    m_p.save();
    QPen pen(kAirflow, kOutlineW, Qt::DashLine);
    pen.setDashPattern({5, 3});
    m_p.setPen(pen);
    m_p.setBrush(Qt::NoBrush);
    m_p.drawLine(a, b);
    arrowHead(b, a, kAirflow);
    m_p.restore();
}

void DiagramPainter::dimension(QPointF a, QPointF b, const QString &label)
{
    m_p.save();
    const QColor c = Theme::instance().textSecondary();
    QPointF d = b - a;
    const double len = std::hypot(d.x(), d.y());
    if (len < 1e-6) { m_p.restore(); return; }
    d /= len;
    const QPointF n(-d.y(), d.x());
    m_p.setPen(QPen(c, kTickW));
    m_p.drawLine(a - n * 4, a + n * 4);
    m_p.drawLine(b - n * 4, b + n * 4);
    m_p.setPen(QPen(c, kDimW));
    m_p.drawLine(a, b);
    arrowHead(a, a + d * 8, c);
    arrowHead(b, b - d * 8, c);
    // label centred below the dimension line
    QFont f = m_p.font();
    f.setPointSizeF(7.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    m_p.setFont(f);
    m_p.setPen(c);
    const QString up = label.toUpper();
    QFontMetricsF fm(f);
    // Label sits centred below the line (dimensions in these diagrams are horizontal).
    const double cxMid = (a.x() + b.x()) / 2.0;
    const double yBelow = std::max(a.y(), b.y()) + 14.0;
    m_p.drawText(QPointF(cxMid - fm.horizontalAdvance(up) / 2.0, yBelow), up);
    m_p.restore();
}

void DiagramPainter::label(QPointF at, const QString &text, Align align)
{
    m_p.save();
    QFont f = m_p.font();
    f.setPointSizeF(7.5);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.5);
    m_p.setFont(f);
    m_p.setPen(Theme::instance().textSecondary());
    const QString up = text.toUpper();
    QFontMetricsF fm(f);
    const double w = fm.horizontalAdvance(up);
    QPointF pos = at;
    if (align == Align::Center) pos.rx() -= w / 2.0;
    else if (align == Align::Right) pos.rx() -= w;
    m_p.drawText(pos, up);
    m_p.restore();
}
