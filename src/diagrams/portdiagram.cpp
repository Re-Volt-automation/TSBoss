#include "diagrams/portdiagram.h"
#include <QPainterPath>

void drawPortSection(DiagramPainter &d, const BoxModel &m)
{
    const QRectF a = d.area();
    const double baffleW = 10.0;
    const double cx0 = a.left() + 6;
    const double tubeTopY = a.top() + a.height() * 0.30;
    const double tubeBotY = a.top() + a.height() * 0.62;
    const double midY     = (tubeTopY + tubeBotY) / 2.0;
    const double outerX   = cx0 + baffleW;   // baffle/outer end
    const double innerX   = a.right() - 14;  // in-box end
    const double flareDX  = 16.0;
    const double flareDY  = 10.0;

    // Baffle wall (hatched solid).
    d.hatchedRect(QRectF(cx0, a.top() + 4, baffleW, a.height() - 28));

    // One tube wall. flareSign = -1 for the top wall (lip curves up), +1 for the
    // bottom (lip curves down). Flares are added per portFlare; each end flares
    // away from the tube axis (mirror of the other wall, flare sign flipped).
    auto buildWall = [&](double wallY, double flareSign) {
        QPainterPath w;
        if (portFlareOuter(m.portFlare)) {
            w.moveTo(outerX - flareDX, wallY + flareSign * flareDY);
            w.cubicTo(outerX, wallY + flareSign * flareDY, outerX, wallY, outerX + 6, wallY);
        } else {
            w.moveTo(outerX, wallY);
        }
        const double endX = portFlareInner(m.portFlare) ? innerX - flareDX : innerX;
        w.lineTo(endX, wallY);
        if (portFlareInner(m.portFlare))
            w.cubicTo(innerX, wallY, innerX, wallY + flareSign * flareDY,
                      innerX + 6, wallY + flareSign * flareDY);
        return w;
    };
    d.wall(buildWall(tubeTopY, -1.0));
    d.wall(buildWall(tubeBotY, +1.0));

    // Airflow through the centre (out of the box, toward the baffle/left).
    d.airflow(QPointF(innerX - 20, midY), QPointF(outerX + 14, midY));

    // Length dimension under the tube (qualitative — exact value is in the results column).
    d.dimension(QPointF(outerX, tubeBotY + flareDY + 16),
                QPointF(innerX, tubeBotY + flareDY + 16), "L");

    // Face/shape caption above the tube (~cap-height clearance from the top edge).
    QString cap = portFaceLabel(m);
    if (m.numPorts > 1) cap += QString(" · ×%1").arg(m.numPorts);
    d.label(QPointF(a.center().x(), a.top() + 12), cap, DiagramPainter::Align::Center);
}
