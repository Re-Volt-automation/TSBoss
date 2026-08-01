#pragma once
#include "driverrecord.h"
#include "driverdb.h"
#include <QWidget>

class QDoubleSpinBox;
class QLineEdit;
class QDateEdit;
class QTextEdit;
class QLabel;
class QGroupBox;
class QRadioButton;

class QuickEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QuickEntryWidget(DriverDatabase *db, QWidget *parent = nullptr);

    void clear();
    void loadRecord(const DriverRecord &r);

signals:
    void recordCalculated(DriverRecord record);

private slots:
    void onCalculate();
    void updateFreeAirHints();
    void updatePhysicalHints();

private:
    void buildUi();
    void buildResultsGroup();
    DriverRecord collectRecord() const;
    void showResults(const DriverRecord &r);
    QString voltageUnit() const;

    DriverDatabase *m_db;

    // Identity
    QLineEdit      *m_make, *m_model, *m_measuredBy;
    QDateEdit      *m_date;

    // Measurement setup
    QDoubleSpinBox *m_vmeas, *m_rs;
    QRadioButton   *m_rbRms, *m_rbPp;

    // Raw measurements
    QDoubleSpinBox *m_Re;
    QDoubleSpinBox *m_fs, *m_Vpeak, *m_f1, *m_f2;
    QDoubleSpinBox *m_deltaM_g, *m_fo;
    QDoubleSpinBox *m_Dd_mm, *m_Zmin, *m_f3;

    // Live voltage hints
    QLabel         *m_hintZ12;      ///< "Z₁=Z₂ = X.XXX Ω"
    QLabel         *m_hintVtarget;  ///< target voltage for f1/f2
    QLabel         *m_hintVmin;     ///< voltage at Zmin
    QLabel         *m_hintVf3;      ///< target voltage for f3

    // Voice coil type
    QRadioButton   *m_rbSVC      = nullptr;
    QRadioButton   *m_rbDVC      = nullptr;
    QRadioButton   *m_rbSeries   = nullptr;
    QRadioButton   *m_rbParallel = nullptr;
    QWidget        *m_dvcOptions = nullptr;

    // Notes
    QTextEdit      *m_notes;

    // Results display
    QGroupBox      *m_resultsBox;
    QLabel         *m_resRe, *m_resFs, *m_resQms, *m_resQes, *m_resQts;
    QLabel         *m_resMms, *m_resCms, *m_resRms, *m_resBL;
    QLabel         *m_resSd, *m_resVas, *m_resLe, *m_resSpl, *m_resSpl283, *m_resVerify;
    QLabel         *m_statusLbl;

    DriverRecord    m_lastRecord;
    int             m_editId = -1;
};
