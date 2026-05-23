#include "diagrams/diagramview.h"
#include "theme.h"
#include <QPainter>

DiagramView::DiagramView(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(&Theme::instance(), &Theme::themeChanged, this, [this]{ update(); });
}

void DiagramView::setDraw(std::function<void(DiagramPainter &)> fn)
{
    m_draw = std::move(fn);
    update();
}

void DiagramView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Theme::instance().sunkenBg());
    if (!m_draw) return;
    const QRectF content = QRectF(rect()).adjusted(10, 10, -10, -10);
    DiagramPainter d(p, content);
    m_draw(d);
}
