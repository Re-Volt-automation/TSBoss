#pragma once
#include "driverrecord.h"
#include <QWidget>

class QLabel;

// ─────────────────────────────────────────────────────────────────
//  DriverDetailWidget – read-only display of all parameters for
//  one driver.  Shown when the user selects a row in the list.
// ─────────────────────────────────────────────────────────────────
class DriverDetailWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DriverDetailWidget(QWidget *parent = nullptr);

    void loadDriver(const DriverRecord &r);
    void clear();

signals:
    void editRequested(int id);
    void deleteRequested(int id);
    void useRequested(int id);

private:
    void buildUi();

    // Identity
    QLabel *m_lblMake, *m_lblModel;
    QLabel *m_lblDate, *m_lblBy, *m_lblNotes;

    // Measurement setup
    QLabel *m_lblVmeas, *m_lblRs, *m_lblVmode;

    // Raw measurements
    QLabel *m_lblRe,   *m_lblFs,   *m_lblZmax;
    QLabel *m_lblF1,   *m_lblF2,   *m_lblDeltaM;
    QLabel *m_lblFo,   *m_lblDd,   *m_lblZmin;
    QLabel *m_lblF3;

    // T/S parameters
    QLabel *m_lblZ12,  *m_lblR0,   *m_lblFsVerify;
    QLabel *m_lblQms,  *m_lblQes,  *m_lblQts;
    QLabel *m_lblMms,  *m_lblCms,  *m_lblRms;
    QLabel *m_lblBL,   *m_lblSd,   *m_lblVas;
    QLabel *m_lblSpl;
    QLabel *m_lblLe;

    // Additional linear parameters
    QLabel *m_lblZnom, *m_lblFLe, *m_lblKLe;

    // Large signal parameters
    QLabel *m_lblXmax, *m_lblXlim, *m_lblPe, *m_lblHg, *m_lblVd;

    int m_currentId = -1;
};
