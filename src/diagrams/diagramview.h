#pragma once
#include <QWidget>
#include <functional>
#include "diagrams/diagrampainter.h"

// Generic host for any schematic diagram. Holds a draw callback and renders it
// through a DiagramPainter on paint. Reused for every diagram type — no
// per-diagram QWidget subclass.
class DiagramView : public QWidget
{
    Q_OBJECT
public:
    explicit DiagramView(QWidget *parent = nullptr);

    // Set the drawing callback (pass nullptr to show an empty frame). Repaints.
    void setDraw(std::function<void(DiagramPainter &)> fn);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    std::function<void(DiagramPainter &)> m_draw;
};
