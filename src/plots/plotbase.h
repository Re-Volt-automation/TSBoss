#pragma once
#include "boxmodel.h"
#include "plotsupport.h"
#include <QList>
#include <QWidget>
#include <optional>

class QMouseEvent;
class QPainter;

// ─────────────────────────────────────────────────────────────────
//  PlotBase – common state and behaviour of the log-frequency model
//  plots: model list, optional fixed Y range, applied power, cursor
//  tracking.  Subclasses implement paintEvent only.
// ─────────────────────────────────────────────────────────────────
class PlotBase : public QWidget
{
    Q_OBJECT
public:
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void clear();
    void setYRange(double min, double max);
    void resetYRange();
    void setPower(double watts);
    void setPerDriverMode(bool on);

protected:
    /// minHeight: minimum widget height; defaultPower: initial applied power [W].
    explicit PlotBase(int minHeight, double defaultPower, QWidget *parent = nullptr);

    double modelPower(const BoxModel &m) const;

    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

    QList<BoxModel>       m_models;
    int                   m_activeIdx     = -1;
    double                m_power         = 1.0;
    bool                  m_perDriverMode = false;
    double                m_cursorFreq    = -1.0;
    std::optional<double> m_yMin, m_yMax;
};

/// Track the cursor frequency from a mouse move inside the plot area.
void plotUpdateCursor(QWidget *w, QMouseEvent *e, double &cursorFreq);

/// Crosshair + curve dots + annotation box at the cursor frequency.
void drawCursorOverlay(QPainter &p, const QRectF &area,
                       double cursorPx, double freqHz,
                       const QVector<CursorEntry> &entries);
