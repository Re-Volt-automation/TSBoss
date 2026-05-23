#pragma once
#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QPointF>
#include <QString>

// Blueprint-style drawing primitives shared by every schematic diagram.
// Owns the whole visual language — line weights, hatch, dimension arrows,
// labels, theme colours. Diagrams draw ONLY through this; change a convention
// here and all diagrams update. Construct one over a target QPainter + rect.
class DiagramPainter
{
public:
    enum class Align { Left, Center, Right };

    DiagramPainter(QPainter &p, const QRectF &area);

    const QRectF &area() const { return m_area; }
    QPainter     &p() const { return m_p; }   // escape hatch for cases DiagramPainter doesn't cover; prefer adding a method

    void setOutlinePen();   // sets the standard 1.25px theme outline pen AND clears the brush (NoBrush)
    void hatchedRect(const QRectF &r);          // solid material: hatch fill + outline
    void wall(const QPainterPath &path);        // outline-pen polyline (no fill)
    void airflow(QPointF a, QPointF b);         // dashed blue accent arrow a->b
    void dimension(QPointF a, QPointF b, const QString &label); // ext lines, ticks, arrows, label
    void label(QPointF at, const QString &text, Align align = Align::Center);

private:
    void arrowHead(QPointF tip, QPointF from, const QColor &c); // filled triangle at tip
    QPainter &m_p;
    QRectF    m_area;
};
