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

void drawBandpassSection(DiagramPainter &d, const BoxModel &m)
{
    const QRectF a = d.area();
    // Landscape enclosure box: 18px each side for port stubs, 30px below for labels.
    const QRectF box(a.left() + 18, a.top() + 6, a.width() - 36, a.height() - 30);
    const double midX = box.center().x();
    const double cy   = box.center().y();
    const bool rearSealed = bandpassRearSealed(m);

    // Sealed rear (BP4): faint hatch wash over the rear (left) chamber.
    if (rearSealed)
        d.hatchedRect(QRectF(box.left(), box.top(), midX - box.left(), box.height()));

    // Enclosure box + central divider.
    QPainterPath shell;
    shell.addRect(box);
    shell.moveTo(midX, box.top());
    shell.lineTo(midX, box.bottom());
    d.wall(shell);

    // Driver on the divider (cone throat toward the front/right chamber).
    QPainterPath drv;
    drv.moveTo(midX - 12, cy - 18);
    drv.lineTo(midX,      cy - 11);
    drv.lineTo(midX,      cy + 11);
    drv.lineTo(midX - 12, cy + 18);
    d.wall(drv);

    // Rear port (left wall) — only when the rear is vented (BP6).
    if (!rearSealed) {
        QPainterPath rp;
        rp.addRect(QRectF(box.left() - 12, cy - 7, 12, 14));
        d.wall(rp);
        d.airflow(QPointF(box.left() + 28, cy), QPointF(box.left() - 14, cy));
    }

    // Front port (right wall) — present for BP4 and BP6.
    QPainterPath fp;
    fp.addRect(QRectF(box.right(), cy - 7, 12, 14));
    d.wall(fp);
    d.airflow(QPointF(box.right() - 28, cy), QPointF(box.right() + 14, cy));

    // Chamber labels.
    QString rearFace;
    if (rearSealed) {
        rearFace = QStringLiteral("SEALED");
    } else {
        rearFace = portFaceLabel(m);
        if (m.numPorts > 1) rearFace += QString(" ×%1").arg(m.numPorts);
    }
    QString frontFace = portFaceLabelFront(m);
    if (m.numPortsFront > 1) frontFace += QString(" ×%1").arg(m.numPortsFront);
    const QString rearTxt  = QString("REAR %1 L · %2").arg(int(m.volumeL + 0.5)).arg(rearFace);
    const QString frontTxt = QString("FRONT %1 L · %2").arg(int(m.volumeFront_L + 0.5)).arg(frontFace);
    d.label(QPointF((box.left() + midX) / 2.0,  a.bottom() - 2), rearTxt,  DiagramPainter::Align::Center);
    d.label(QPointF((midX + box.right()) / 2.0, a.bottom() - 2), frontTxt, DiagramPainter::Align::Center);
}
