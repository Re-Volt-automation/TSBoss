#pragma once
#include "driverrecord.h"
#include "driverdb.h"
#include <QWizard>
#include <QWizardPage>
#include <QWidget>

class QDoubleSpinBox;
class QLineEdit;
class QDateEdit;
class QTextEdit;
class QLabel;
class QRadioButton;

// ─────────────────────────────────────────────────────────────────
//  ImpedanceDiagram – QPainter-drawn reference diagram
// ─────────────────────────────────────────────────────────────────
class ImpedanceDiagram : public QWidget
{
    Q_OBJECT
public:
    explicit ImpedanceDiagram(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {460, 210}; }
protected:
    void paintEvent(QPaintEvent *) override;
};

// ─────────────────────────────────────────────────────────────────
//  Wizard pages
// ─────────────────────────────────────────────────────────────────

class IntroPage : public QWizardPage {
    Q_OBJECT
public: explicit IntroPage(QWidget *p = nullptr);
};

class IdentityPage : public QWizardPage {
    Q_OBJECT
public: explicit IdentityPage(QWidget *p = nullptr);
private:
    QLineEdit  *m_make, *m_model, *m_measuredBy;
    QDateEdit  *m_date;
};

class DCResistancePage : public QWizardPage {
    Q_OBJECT
public: explicit DCResistancePage(QWidget *p = nullptr);
private:
    QDoubleSpinBox *m_Re;
};

// ── NEW: ask for measurement voltage and series resistor ──────────
class MeasurementSetupPage : public QWizardPage {
    Q_OBJECT
public: explicit MeasurementSetupPage(QWidget *p = nullptr);
private:
    QDoubleSpinBox *m_vmeas, *m_rs;
    QRadioButton   *m_rbRms, *m_rbPp;
};

class FreeAirPage : public QWizardPage {
    Q_OBJECT
public:
    explicit FreeAirPage(QWidget *p = nullptr);
    bool validatePage() override;
private slots:
    void updateLiveCalc();
private:
    QDoubleSpinBox *m_fs, *m_Vpeak, *m_f1, *m_f2;
    QLabel         *m_lblZ12, *m_lblVerify, *m_lblVtarget;
};

class AddedMassPage : public QWizardPage {
    Q_OBJECT
public: explicit AddedMassPage(QWidget *p = nullptr);
    bool validatePage() override;
private:
    QDoubleSpinBox *m_deltaM_g, *m_fo;
};

class PhysicalPage : public QWizardPage {
    Q_OBJECT
public:
    explicit PhysicalPage(QWidget *p = nullptr);
    void initializePage() override;
private slots:
    void updateVoltageHints();
private:
    QDoubleSpinBox *m_Dd_mm, *m_Zmin, *m_f3;
    QLabel         *m_lblVmin, *m_lblVf3;
};

class ResultsPage : public QWizardPage {
    Q_OBJECT
public:
    explicit ResultsPage(QWidget *p = nullptr);
    void initializePage() override;
    bool validatePage()   override;
    const DriverRecord &record() const { return m_record; }
private:
    QTextEdit *m_notes;
    QLabel    *m_lblQts, *m_lblQms, *m_lblQes;
    QLabel    *m_lblFs,  *m_lblMms, *m_lblBL;
    QLabel    *m_lblCms, *m_lblRms, *m_lblVas;
    QLabel    *m_lblSd,  *m_lblLe,  *m_lblRe;
    QLabel    *m_lblVerify;
    DriverRecord m_record;
    void populateLabels(const DriverRecord &r);
};

// ─────────────────────────────────────────────────────────────────
//  TSWizard – 8-page guided measurement wizard
// ─────────────────────────────────────────────────────────────────
class TSWizard : public QWizard
{
    Q_OBJECT
public:
    explicit TSWizard(QWidget *parent = nullptr);

signals:
    void recordReady(DriverRecord record);

private:
    ResultsPage *m_resultsPage = nullptr;
};
