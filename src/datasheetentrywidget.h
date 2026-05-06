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
class QPushButton;
class QRadioButton;
class QWidget;

// ─────────────────────────────────────────────────────────────────
//  DatasheetEntryWidget – enter T/S parameters directly from a
//  manufacturer datasheet (no measurement required).
//
//  As fields are filled the widget computes every derivable value
//  from the SB Acoustics T/S relationships.  For each field that
//  can be cross-computed, a button shows the calculated result
//  inline next to the field.  Clicking the button fills the field
//  with the computed value.  The button is only enabled when the
//  computed value differs from what is currently entered.
//
//  An "Integrity Check" button then compares all entered values
//  against the cross-computed values and reports discrepancies.
// ─────────────────────────────────────────────────────────────────

class DatasheetEntryWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DatasheetEntryWidget(DriverDatabase *db, QWidget *parent = nullptr);

    void clear();
    void loadRecord(const DriverRecord &r);

signals:
    void driverSaved(int id);

private slots:
    void onSave();
    void onSaveAsNew();
    void onIntegrityCheck();
    void updateHints();
    void updateOriginColors();

private:
    void buildUi();
    DriverRecord collectRecord() const;

    DriverDatabase *m_db;
    int             m_editId = -1;
    DriverRecord    m_loadedRaw;       // preserves raw measurement fields on edit

    // ── Identity ──────────────────────────────────────────────────
    QLineEdit      *m_make, *m_model, *m_source;
    QDateEdit      *m_date;

    // ── Electrical ────────────────────────────────────────────────
    QDoubleSpinBox *m_Re;
    QPushButton    *m_btnRe  = nullptr;
    QLabel         *m_fmtRe  = nullptr;
    QDoubleSpinBox *m_Le_mH;

    // ── Voice coil type ───────────────────────────────────────────
    QRadioButton   *m_rbSVC        = nullptr;
    QRadioButton   *m_rbDVC        = nullptr;
    QRadioButton   *m_rbSeries     = nullptr;
    QRadioButton   *m_rbParallel   = nullptr;
    QWidget        *m_dvcOptions   = nullptr;
    QLabel         *m_perCoilReLbl = nullptr;

    // ── Field origin tracking ─────────────────────────────────────
    quint64 m_fieldOrigins      = 0;
    quint64 m_userEnteredFields = 0;
    bool    m_applyingHint      = false;

    // ── Resonance & Q-factors ─────────────────────────────────────
    QDoubleSpinBox *m_fs;
    QDoubleSpinBox *m_Qms;
    QDoubleSpinBox *m_Qes;
    QDoubleSpinBox *m_Qts;

    // Inline computed-value buttons + formula labels
    QPushButton *m_btnQts, *m_btnQms, *m_btnQes;
    QLabel      *m_fmtQts, *m_fmtQms, *m_fmtQes;

    // ── Mechanical ────────────────────────────────────────────────
    QDoubleSpinBox *m_mms_g;
    QDoubleSpinBox *m_Cms_mmN;
    QDoubleSpinBox *m_Rms;

    QPushButton *m_btnMms, *m_btnCms, *m_btnRms;
    QLabel      *m_fmtMms, *m_fmtCms, *m_fmtRms;

    // ── Force factor ──────────────────────────────────────────────
    QDoubleSpinBox *m_BL;
    QPushButton    *m_btnBL;
    QLabel         *m_fmtBL;

    // ── Geometry & volume ─────────────────────────────────────────
    QDoubleSpinBox *m_Dd_mm;
    QDoubleSpinBox *m_Sd_cm2;
    QDoubleSpinBox *m_Vas_L;

    QPushButton *m_btnSd, *m_btnDd, *m_btnVas;
    QLabel      *m_fmtSd, *m_fmtDd, *m_fmtVas;

    // ── Sensitivity ──────────────────────────────────────────────
    QDoubleSpinBox *m_Spl    = nullptr;
    QPushButton    *m_btnSpl = nullptr;
    QLabel         *m_fmtSpl = nullptr;

    // Secondary SPL-derived suggest buttons (on Qes, Vas, BL rows)
    QPushButton    *m_btnSplQes = nullptr;
    QLabel         *m_fmtSplQes = nullptr;
    QPushButton    *m_btnSplVas = nullptr;
    QLabel         *m_fmtSplVas = nullptr;
    QPushButton    *m_btnSplBL  = nullptr;
    QLabel         *m_fmtSplBL  = nullptr;

    // ── Additional linear parameters ─────────────────────────────
    QDoubleSpinBox *m_Znom;
    QDoubleSpinBox *m_fLe;
    QDoubleSpinBox *m_KLe;

    // ── Large signal parameters ───────────────────────────────────
    QDoubleSpinBox *m_Xmax;
    QDoubleSpinBox *m_Xlim;
    QDoubleSpinBox *m_Pe;
    QDoubleSpinBox *m_Hg;
    QLabel         *m_hintVd;   ///< Vd auto-label (no user field to fill)

    // ── Notes ─────────────────────────────────────────────────────
    QTextEdit *m_notes;

    // ── Buttons ───────────────────────────────────────────────────
    QPushButton *m_btnSaveAsNew = nullptr;

    // ── Status / integrity ────────────────────────────────────────
    QLabel     *m_statusLbl;
    QGroupBox  *m_integrityBox;
    QLabel     *m_integrityResult;
};
