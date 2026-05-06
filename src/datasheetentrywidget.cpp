#include "datasheetentrywidget.h"
#include "tscalculator.h"
#include "noscrollspinbox.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QMessageBox>
#include <QFrame>
#include <cmath>

// ── Constants ─────────────────────────────────────────────────────
static constexpr double PI  = TSCalculator::PI;
static constexpr double RHO = TSCalculator::RHO;
static constexpr double C   = TSCalculator::C;

// ── Helpers ───────────────────────────────────────────────────────

static QDoubleSpinBox *mkSpin(double lo, double hi, int dec,
                               double step, const QString &sfx)
{
    auto *s = new NoScrollSpinBox;
    s->setRange(lo, hi);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setSuffix(sfx);
    s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return s;
}

// Style used by all "apply computed value" buttons
static const char *kSuggestBtnStyleSpl =
    "QPushButton{"
    "  padding:2px 8px; font-size:9pt; font-weight:bold;"
    "  background:#1a6080; color:white;"
    "  border:none; border-radius:3px;"
    "}"
    "QPushButton:hover  { background:#2a80a0; }"
    "QPushButton:pressed{ background:#0a4060; }"
    "QPushButton:disabled{"
    "  background:#e8e8e8; color:#aaa; font-weight:normal;"
    "}";

static const char *kSuggestBtnStyle =
    "QPushButton{"
    "  padding:2px 8px; font-size:9pt; font-weight:bold;"
    "  background:#6b2a40; color:white;"
    "  border:none; border-radius:3px;"
    "}"
    "QPushButton:hover  { background:#8b3a50; }"
    "QPushButton:pressed{ background:#4a1a2e; }"
    "QPushButton:disabled{"
    "  background:#e8e8e8; color:#aaa; font-weight:normal;"
    "}";

// Creates a row widget: [spinbox | suggest-button | formula-label]
// btn and fmtLbl are set to the newly created widgets.
// formula is the static formula text shown after the button.
static QWidget *mkFieldRow(QDoubleSpinBox *spin,
                            QPushButton *&btn, QLabel *&fmtLbl,
                            const QString &formula)
{
    auto *w  = new QWidget;
    auto *hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);

    btn = new QPushButton("—");
    btn->setEnabled(false);
    btn->setMinimumWidth(80);
    btn->setStyleSheet(kSuggestBtnStyle);

    fmtLbl = new QLabel(formula);
    fmtLbl->setStyleSheet("color:#777; font-size:8.5pt; font-style:italic;");

    hl->addWidget(spin);
    hl->addWidget(btn);
    hl->addWidget(fmtLbl);
    hl->addStretch();
    return w;
}

// Two-button variant: primary formula button + SPL-derived secondary button
static QWidget *mkFieldRow2(QDoubleSpinBox *spin,
                             QPushButton *&btn1, QLabel *&fmt1, const QString &f1,
                             QPushButton *&btn2, QLabel *&fmt2, const QString &f2)
{
    auto *w  = new QWidget;
    auto *hl = new QHBoxLayout(w);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(6);

    btn1 = new QPushButton("—");
    btn1->setEnabled(false);
    btn1->setMinimumWidth(80);
    btn1->setStyleSheet(kSuggestBtnStyle);
    fmt1 = new QLabel(f1);
    fmt1->setStyleSheet("color:#777; font-size:8.5pt; font-style:italic;");

    btn2 = new QPushButton("—");
    btn2->setEnabled(false);
    btn2->setMinimumWidth(80);
    btn2->setStyleSheet(kSuggestBtnStyleSpl);
    fmt2 = new QLabel(f2);
    fmt2->setStyleSheet("color:#1a6080; font-size:8.5pt; font-style:italic;");

    hl->addWidget(spin);
    hl->addWidget(btn1);
    hl->addWidget(fmt1);
    hl->addWidget(btn2);
    hl->addWidget(fmt2);
    hl->addStretch();
    return w;
}

static QLabel *mkFormLabel(const QString &text)
{
    auto *l = new QLabel(text);
    l->setStyleSheet("color:#3a3a3a; background:transparent;");
    return l;
}

static void addRow(QFormLayout *f, const QString &lbl, QWidget *w)
{
    f->addRow(mkFormLabel(lbl), w);
}

// Percentage deviation helper (for integrity check)
static double pctDev(double entered, double computed)
{
    const double denom = std::max(std::fabs(entered), std::fabs(computed));
    if (denom < 1e-12) return 0.0;
    return 100.0 * std::fabs(entered - computed) / denom;
}

// Update a suggest button: set computed value, enable if differs from current.
// eps = half a display-unit step (determines "different enough to show button").
static void updateSuggestBtn(QPushButton *btn, double computed,
                              double current, int decimals, const QString &text)
{
    btn->setProperty("cv", computed);
    btn->setText(text);
    const double eps = 0.5 * std::pow(10.0, -decimals);
    btn->setEnabled(std::fabs(computed - current) > eps);
}

// ─────────────────────────────────────────────────────────────────
DatasheetEntryWidget::DatasheetEntryWidget(DriverDatabase *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    buildUi();
}

void DatasheetEntryWidget::buildUi()
{
    auto *inner = new QWidget;
    auto *vbox  = new QVBoxLayout(inner);
    vbox->setSpacing(10);
    vbox->setContentsMargins(16, 16, 16, 16);

    // ── Header ────────────────────────────────────────────────────
    auto *title = new QLabel("New Entry – T/S Parameters from Datasheet");
    title->setStyleSheet("font-size:14pt; font-weight:bold; color:#3a3a3a;");
    vbox->addWidget(title);

    auto *sub = new QLabel(
        "Enter Thiele/Small parameters as listed in the manufacturer datasheet. "
        "Where a value can be cross-computed from other fields, a button appears "
        "inline — click it to apply the calculated value. "
        "Use <b>Integrity Check</b> to verify all values are self-consistent before saving.");
    sub->setWordWrap(true);
    sub->setStyleSheet("color:#555; margin-bottom:4px;");
    vbox->addWidget(sub);

    // ── Driver Identification ─────────────────────────────────────
    {
        auto *box  = new QGroupBox("Driver Identification");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_make   = new QLineEdit;
        m_model  = new QLineEdit;
        m_source = new QLineEdit;
        m_source->setPlaceholderText("e.g. manufacturer datasheet rev 1.2, or your name");
        m_date   = new QDateEdit(QDate::currentDate());
        m_date->setCalendarPopup(true);
        m_date->setDisplayFormat("yyyy-MM-dd");

        addRow(form, "Manufacturer / Make:", m_make);
        addRow(form, "Model:",               m_model);
        addRow(form, "Date entered:",        m_date);
        addRow(form, "Source:",              m_source);
        vbox->addWidget(box);
    }

    // ── Electrical ────────────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Electrical Parameters");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_Re    = mkSpin(0.10, 200.0, 3, 0.01, " Ω");
        m_Le_mH = mkSpin(0.00, 100.0, 3, 0.01, " mH");

        auto *reRow = mkFieldRow(m_Re, m_btnRe, m_fmtRe, "= BL²·Qes / (2π·fₛ·mms)");
        connect(m_btnRe, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Re->setValue(m_btnRe->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Re;
            m_userEnteredFields &= ~FieldOrigin::Re;
            m_applyingHint = false;
            updateOriginColors();
        });

        addRow(form, "Rₑ – DC resistance:",          reRow);
        addRow(form, "Lₑ – voice-coil inductance:",  m_Le_mH);

        connect(m_Re,    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &DatasheetEntryWidget::updateHints);
        connect(m_Le_mH, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &DatasheetEntryWidget::updateHints);
        connect(m_Re, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Re;
                if (m_Re->value() > 0.0) m_userEnteredFields |= FieldOrigin::Re;
                else                      m_userEnteredFields &= ~FieldOrigin::Re;
                updateOriginColors();
            }
        });
        vbox->addWidget(box);
    }

    // ── Voice Coil Configuration ──────────────────────────────────
    {
        auto *box = new QGroupBox("Voice Coil Configuration");
        auto *vl  = new QVBoxLayout(box);
        vl->setSpacing(6);

        m_rbSVC = new QRadioButton("SVC – Single Voice Coil");
        m_rbDVC = new QRadioButton("DVC – Dual Voice Coil");
        m_rbSVC->setChecked(true);

        const QString rbStyle =
            "QRadioButton { color:#3a3a3a; padding:3px 6px; border-radius:4px; }"
            "QRadioButton:checked { background:#6b2a40; color:white; font-weight:bold; }";
        m_rbSVC->setStyleSheet(rbStyle);
        m_rbDVC->setStyleSheet(rbStyle);

        auto *vcGroup = new QButtonGroup(this);
        vcGroup->addButton(m_rbSVC, 0);
        vcGroup->addButton(m_rbDVC, 1);

        m_dvcOptions = new QWidget;
        auto *dvcVl  = new QVBoxLayout(m_dvcOptions);
        dvcVl->setContentsMargins(18, 0, 0, 0);
        dvcVl->setSpacing(4);

        auto *dvcRow = new QHBoxLayout;
        dvcRow->setSpacing(8);
        m_rbSeries   = new QRadioButton("Series   (Reff = 2·Rvc)");
        m_rbParallel = new QRadioButton("Parallel  (Reff = Rvc/2)");
        m_rbSeries->setChecked(true);
        m_rbSeries->setStyleSheet(rbStyle);
        m_rbParallel->setStyleSheet(rbStyle);
        auto *wirGroup = new QButtonGroup(this);
        wirGroup->addButton(m_rbSeries, 0);
        wirGroup->addButton(m_rbParallel, 1);
        dvcRow->addWidget(m_rbSeries);
        dvcRow->addWidget(m_rbParallel);
        dvcRow->addStretch();

        m_perCoilReLbl = new QLabel("Per-coil Rvc: —");
        m_perCoilReLbl->setStyleSheet("color:#555; font-size:9pt; padding-left:4px;");

        dvcVl->addLayout(dvcRow);
        dvcVl->addWidget(m_perCoilReLbl);
        m_dvcOptions->setVisible(false);

        connect(m_rbDVC, &QRadioButton::toggled, this, [this](bool on){
            m_dvcOptions->setVisible(on);
            updateHints();
        });
        connect(m_rbParallel, &QRadioButton::toggled,
                this, &DatasheetEntryWidget::updateHints);

        vl->addWidget(m_rbSVC);
        vl->addWidget(m_rbDVC);
        vl->addWidget(m_dvcOptions);
        vbox->addWidget(box);
    }

    // ── Resonance & Q-factors ─────────────────────────────────────
    {
        auto *box  = new QGroupBox("Resonance & Q-factors");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_fs  = mkSpin(1.0,  2000.0, 2, 0.1, " Hz");
        m_Qms = mkSpin(0.00, 100.0,  3, 0.01, "");
        m_Qes = mkSpin(0.00, 100.0,  3, 0.01, "");
        m_Qts = mkSpin(0.00, 100.0,  3, 0.01, "");

        auto *qtsRow = mkFieldRow(m_Qts, m_btnQts, m_fmtQts, "= Qms·Qes / (Qms+Qes)");
        auto *qmsRow = mkFieldRow(m_Qms, m_btnQms, m_fmtQms, "= Qts·Qes / (Qes−Qts)");
        auto *qesRow = mkFieldRow2(m_Qes, m_btnQes, m_fmtQes, "= Qts·Qms / (Qms−Qts)",
                                   m_btnSplQes, m_fmtSplQes, "← from SPL+fs+Vas");

        connect(m_btnQts, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Qts->setValue(m_btnQts->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Qts;
            m_userEnteredFields &= ~FieldOrigin::Qts;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnQms, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Qms->setValue(m_btnQms->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Qms;
            m_userEnteredFields &= ~FieldOrigin::Qms;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnQes, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Qes->setValue(m_btnQes->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Qes;
            m_userEnteredFields &= ~FieldOrigin::Qes;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnSplQes, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Qes->setValue(m_btnSplQes->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Qes;
            m_userEnteredFields &= ~FieldOrigin::Qes;
            m_applyingHint = false;
            updateOriginColors();
        });

        addRow(form, "fₛ  – free-air resonance:", m_fs);
        addRow(form, "Qms – mechanical Q-factor:", qmsRow);
        addRow(form, "Qes – electrical Q-factor:", qesRow);
        addRow(form, "Qts – total Q-factor:",      qtsRow);

        for (auto *spin : {m_fs, m_Qms, m_Qes, m_Qts})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &DatasheetEntryWidget::updateHints);
        connect(m_Qts, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Qts;
                if (m_Qts->value() > 0.0) m_userEnteredFields |= FieldOrigin::Qts;
                else                       m_userEnteredFields &= ~FieldOrigin::Qts;
                updateOriginColors();
            }
        });
        connect(m_Qms, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Qms;
                if (m_Qms->value() > 0.0) m_userEnteredFields |= FieldOrigin::Qms;
                else                       m_userEnteredFields &= ~FieldOrigin::Qms;
                updateOriginColors();
            }
        });
        connect(m_Qes, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Qes;
                if (m_Qes->value() > 0.0) m_userEnteredFields |= FieldOrigin::Qes;
                else                       m_userEnteredFields &= ~FieldOrigin::Qes;
                updateOriginColors();
            }
        });
        vbox->addWidget(box);
    }

    // ── Mechanical ────────────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Mechanical Parameters");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_mms_g  = mkSpin(0.00, 2000.0, 3, 0.1,    " g");
        m_Cms_mmN= mkSpin(0.00, 10000.0,3, 0.1,    " mm/N");
        m_Rms    = mkSpin(0.00, 100.0,  4, 0.0001, " kg/s");

        auto *mmsRow = mkFieldRow(m_mms_g,   m_btnMms, m_fmtMms, "= 1 / (4π²·fs²·Cms)");
        auto *cmsRow = mkFieldRow(m_Cms_mmN, m_btnCms, m_fmtCms, "= 1 / (4π²·fs²·mms)");
        auto *rmsRow = mkFieldRow(m_Rms,     m_btnRms, m_fmtRms, "= 2π·fs·mms / Qms");

        connect(m_btnMms, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_mms_g->setValue(m_btnMms->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Mms;
            m_userEnteredFields &= ~FieldOrigin::Mms;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnCms, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Cms_mmN->setValue(m_btnCms->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Cms;
            m_userEnteredFields &= ~FieldOrigin::Cms;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnRms, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Rms->setValue(m_btnRms->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Rms;
            m_userEnteredFields &= ~FieldOrigin::Rms;
            m_applyingHint = false;
            updateOriginColors();
        });

        addRow(form, "mms – moving mass (incl. air):", mmsRow);
        addRow(form, "Cms – suspension compliance:",   cmsRow);
        addRow(form, "Rms – mechanical resistance:",   rmsRow);

        for (auto *spin : {m_mms_g, m_Cms_mmN, m_Rms})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &DatasheetEntryWidget::updateHints);
        connect(m_mms_g,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Mms;
                if (m_mms_g->value() > 0.0) m_userEnteredFields |= FieldOrigin::Mms;
                else                          m_userEnteredFields &= ~FieldOrigin::Mms;
                updateOriginColors();
            }
        });
        connect(m_Cms_mmN, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Cms;
                if (m_Cms_mmN->value() > 0.0) m_userEnteredFields |= FieldOrigin::Cms;
                else                            m_userEnteredFields &= ~FieldOrigin::Cms;
                updateOriginColors();
            }
        });
        connect(m_Rms, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Rms;
                if (m_Rms->value() > 0.0) m_userEnteredFields |= FieldOrigin::Rms;
                else                       m_userEnteredFields &= ~FieldOrigin::Rms;
                updateOriginColors();
            }
        });
        vbox->addWidget(box);
    }

    // ── Force factor ──────────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Force Factor");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_BL = mkSpin(0.00, 500.0, 3, 0.01, " Tm");

        auto *blRow = mkFieldRow2(m_BL, m_btnBL, m_fmtBL, "= √(2π·fs·mms·Re / Qes)",
                                  m_btnSplBL, m_fmtSplBL, "← from SPL+Re+mms+Sd");

        connect(m_btnBL, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_BL->setValue(m_btnBL->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::BL;
            m_userEnteredFields &= ~FieldOrigin::BL;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnSplBL, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_BL->setValue(m_btnSplBL->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::BL;
            m_userEnteredFields &= ~FieldOrigin::BL;
            m_applyingHint = false;
            updateOriginColors();
        });

        addRow(form, "BL – force factor:", blRow);

        connect(m_BL, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &DatasheetEntryWidget::updateHints);
        connect(m_BL, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::BL;
                if (m_BL->value() > 0.0) m_userEnteredFields |= FieldOrigin::BL;
                else                      m_userEnteredFields &= ~FieldOrigin::BL;
                updateOriginColors();
            }
        });
        vbox->addWidget(box);
    }

    // ── Geometry & Volume ─────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Geometry & Equivalent Volume");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_Dd_mm  = mkSpin(0.0, 800.0,  1, 0.5,   " mm");
        m_Sd_cm2 = mkSpin(0.0, 5000.0, 2, 0.5,   " cm²");
        m_Vas_L  = mkSpin(0.0, 99999.0,3, 0.001, " L");

        // Dd field: button applies Dd computed from Sd
        // Sd field: button applies Sd computed from Dd
        auto *ddRow  = mkFieldRow(m_Dd_mm,  m_btnDd,  m_fmtDd,  "= 2·√(Sd/π)");
        auto *sdRow  = mkFieldRow(m_Sd_cm2, m_btnSd,  m_fmtSd,  "= π·(Dd/2)²");
        auto *vasRow = mkFieldRow2(m_Vas_L, m_btnVas, m_fmtVas, "= Cms·ρ·c²·Sd²",
                                   m_btnSplVas, m_fmtSplVas, "← from SPL+fs+Qes");

        connect(m_btnDd, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Dd_mm->setValue(m_btnDd->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Dd;
            m_userEnteredFields &= ~FieldOrigin::Dd;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnSd, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Sd_cm2->setValue(m_btnSd->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Sd;
            m_userEnteredFields &= ~FieldOrigin::Sd;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnVas, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Vas_L->setValue(m_btnVas->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Vas;
            m_userEnteredFields &= ~FieldOrigin::Vas;
            m_applyingHint = false;
            updateOriginColors();
        });
        connect(m_btnSplVas, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Vas_L->setValue(m_btnSplVas->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Vas;
            m_userEnteredFields &= ~FieldOrigin::Vas;
            m_applyingHint = false;
            updateOriginColors();
        });

        auto *note = mkFormLabel("Enter either Dᵈ or Sᵈ — the other is computed automatically.");
        note->setStyleSheet("color:#666; font-size:8pt; font-style:italic;");

        addRow(form, "Dᵈ – effective piston diameter:", ddRow);
        addRow(form, "Sᵈ – effective piston area:",     sdRow);
        addRow(form, "Vas – equivalent air volume:",    vasRow);
        form->addRow(note);

        for (auto *spin : {m_Dd_mm, m_Sd_cm2, m_Vas_L})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &DatasheetEntryWidget::updateHints);
        connect(m_Dd_mm,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Dd;
                if (m_Dd_mm->value() > 0.0) m_userEnteredFields |= FieldOrigin::Dd;
                else                         m_userEnteredFields &= ~FieldOrigin::Dd;
                updateOriginColors();
            }
        });
        connect(m_Sd_cm2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Sd;
                if (m_Sd_cm2->value() > 0.0) m_userEnteredFields |= FieldOrigin::Sd;
                else                           m_userEnteredFields &= ~FieldOrigin::Sd;
                updateOriginColors();
            }
        });
        connect(m_Vas_L,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Vas;
                if (m_Vas_L->value() > 0.0) m_userEnteredFields |= FieldOrigin::Vas;
                else                          m_userEnteredFields &= ~FieldOrigin::Vas;
                updateOriginColors();
            }
        });
        vbox->addWidget(box);
    }

    // ── Sensitivity ───────────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Sensitivity");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_Spl = mkSpin(0.0, 200.0, 1, 0.1, " dB");

        auto *splRow = mkFieldRow(m_Spl, m_btnSpl, m_fmtSpl,
                                  "= 112.1 + 10·log₁₀(4π²·fₛ³·Vas / (c³·Qes))");
        connect(m_btnSpl, &QPushButton::clicked, this, [this]() {
            m_applyingHint = true;
            m_Spl->setValue(m_btnSpl->property("cv").toDouble());
            m_fieldOrigins |= FieldOrigin::Spl;
            m_userEnteredFields &= ~FieldOrigin::Spl;
            m_applyingHint = false;
            updateOriginColors();
        });

        auto *note = mkFormLabel(
            "1W / 1m  (half-space, ρ=1.2 kg/m³, c=343 m/s). "
            "Enter from datasheet or click ← to compute. "
            "Blue buttons on Qes, Vas and BL derive those values from this SPL.");
        note->setStyleSheet("color:#666; font-size:8pt; font-style:italic;");
        note->setWordWrap(true);

        addRow(form, "SPL – reference sensitivity:", splRow);
        form->addRow(note);

        connect(m_Spl, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &DatasheetEntryWidget::updateHints);
        connect(m_Spl, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](){
            if (!m_applyingHint) {
                m_fieldOrigins &= ~FieldOrigin::Spl;
                if (m_Spl->value() > 0.0) m_userEnteredFields |= FieldOrigin::Spl;
                else                       m_userEnteredFields &= ~FieldOrigin::Spl;
                updateOriginColors();
            }
        });
        vbox->addWidget(box);
    }

    // ── Additional linear parameters ──────────────────────────────
    {
        auto *box  = new QGroupBox("Additional Linear Parameters");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_Znom = mkSpin(0.0, 1000.0, 1, 0.5,    " Ω");
        m_fLe  = mkSpin(0.0, 100000.0, 1, 10.0, " Hz");
        m_KLe  = mkSpin(0.0, 100.0,  5, 0.0001, " H·s^(1-n)");

        addRow(form, "Znom – nominal impedance:",        m_Znom);
        addRow(form, "fLe  – Le measurement frequency:", m_fLe);
        addRow(form, "KLe  – semi-inductance coeff.:",   m_KLe);

        for (auto *spin : {m_Znom, m_fLe, m_KLe})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](){ updateOriginColors(); });
        vbox->addWidget(box);
    }

    // ── Large signal parameters ────────────────────────────────────
    {
        auto *box  = new QGroupBox("Large Signal Parameters");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(8);

        m_Xmax = mkSpin(0.0, 200.0, 2, 0.1,   " mm");
        m_Xlim = mkSpin(0.0, 200.0, 2, 0.1,   " mm");
        m_Pe   = mkSpin(0.0, 10000.0, 1, 1.0, " W");
        m_Hg   = mkSpin(0.0, 50.0,  2, 0.1,   " mm");
        m_hintVd = new QLabel;
        m_hintVd->setWordWrap(true);
        m_hintVd->setTextFormat(Qt::RichText);
        m_hintVd->setStyleSheet("color:#555; font-size:8.5pt; font-style:italic;");
        m_hintVd->setText("<i style='color:#999;'>Enter Sᵈ (or Dᵈ) and Xmax to compute Vd</i>");

        addRow(form, "Xmax – peak linear excursion:",  m_Xmax);
        addRow(form, "Xlim – mechanical travel limit:", m_Xlim);
        addRow(form, "Pₑ   – rated continuous power:",  m_Pe);
        addRow(form, "Hg   – magnetic gap height:",     m_Hg);
        form->addRow(mkFormLabel("Vd  – peak volume displacement (auto):"), m_hintVd);

        for (auto *spin : {m_Xmax, m_Xlim})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, &DatasheetEntryWidget::updateHints);
        for (auto *spin : {m_Pe, m_Hg})
            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](){ updateOriginColors(); });
        vbox->addWidget(box);
    }

    // ── Notes ────────────────────────────────────────────────────
    {
        auto *box = new QGroupBox("Notes");
        auto *bl  = new QVBoxLayout(box);
        m_notes = new QTextEdit;
        m_notes->setPlaceholderText(
            "Datasheet URL, revision, date of issue, any known anomalies…");
        m_notes->setFixedHeight(80);
        bl->addWidget(m_notes);
        vbox->addWidget(box);
    }

    // ── Buttons ──────────────────────────────────────────────────
    {
        auto *hb       = new QHBoxLayout;
        auto *btnCheck = new QPushButton("  Integrity Check  ");
        btnCheck->setStyleSheet(
            "QPushButton{background:#8b3a50;color:white;"
            "border-radius:4px;padding:8px 24px;font-weight:bold;}");
        auto *btnSave = new QPushButton("  Save to Database  ");
        btnSave->setStyleSheet(
            "QPushButton{background:#27ae60;color:white;"
            "border-radius:4px;padding:8px 24px;font-weight:bold;}");
        m_btnSaveAsNew = new QPushButton("  Save as New  ");
        m_btnSaveAsNew->setStyleSheet(
            "QPushButton{background:#1a7db5;color:white;"
            "border-radius:4px;padding:8px 24px;font-weight:bold;}");
        m_btnSaveAsNew->setVisible(false);
        auto *btnClear = new QPushButton("Clear");
        btnClear->setStyleSheet(
            "QPushButton{background:#bdc3c7;color:#3a3a3a;"
            "border-radius:4px;padding:8px 16px;}");

        m_statusLbl = new QLabel;
        m_statusLbl->setWordWrap(true);

        hb->addWidget(btnCheck);
        hb->addWidget(btnSave);
        hb->addWidget(m_btnSaveAsNew);
        hb->addWidget(btnClear);
        hb->addStretch();
        vbox->addLayout(hb);
        vbox->addWidget(m_statusLbl);

        connect(btnCheck,       &QPushButton::clicked, this, &DatasheetEntryWidget::onIntegrityCheck);
        connect(btnSave,        &QPushButton::clicked, this, &DatasheetEntryWidget::onSave);
        connect(m_btnSaveAsNew, &QPushButton::clicked, this, &DatasheetEntryWidget::onSaveAsNew);
        connect(btnClear,       &QPushButton::clicked, this, &DatasheetEntryWidget::clear);
    }

    // ── Integrity results panel ───────────────────────────────────
    {
        m_integrityBox = new QGroupBox("Integrity Check Results");
        auto *bl = new QVBoxLayout(m_integrityBox);
        m_integrityResult = new QLabel;
        m_integrityResult->setWordWrap(true);
        m_integrityResult->setTextFormat(Qt::RichText);
        m_integrityResult->setStyleSheet("font-size:9.5pt; padding:4px;");
        bl->addWidget(m_integrityResult);
        m_integrityBox->setVisible(false);
        vbox->addWidget(m_integrityBox);
    }

    vbox->addStretch();

    // ── Scroll area ───────────────────────────────────────────────
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inner);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);
}

// ── Live hint / button computation ───────────────────────────────

void DatasheetEntryWidget::updateHints()
{
    const double Re    = m_Re->value();
    const double fs    = m_fs->value();
    const double Qms   = m_Qms->value();
    const double Qes   = m_Qes->value();
    const double Qts   = m_Qts->value();
    const double mms_g = m_mms_g->value();          // g
    const double mms   = mms_g / 1000.0;            // kg
    const double BL_v  = m_BL->value();             // Tm
    const double Cms_d = m_Cms_mmN->value();         // mm/N (display)
    const double Cms   = Cms_d * 1e-3;              // m/N
    const double Dd    = m_Dd_mm->value() / 1000.0; // m
    const double Sd    = m_Sd_cm2->value() * 1e-4;  // m²
    const double Vas   = m_Vas_L->value();           // L (display)

    // ── Q-factor buttons ──────────────────────────────────────────

    // Qts from Qms and Qes
    if (Qms > 0.0 && Qes > 0.0) {
        const double q = (Qms * Qes) / (Qms + Qes);
        updateSuggestBtn(m_btnQts, q, Qts, 3,
                         QString("%1").arg(q, 0, 'f', 4));
    } else {
        m_btnQts->setText("—"); m_btnQts->setEnabled(false);
    }

    // Qms from Qts and Qes  (Qes must be > Qts for positive result)
    if (Qts > 0.0 && Qes > 0.0) {
        const double denom = Qes - Qts;
        const double q     = (denom > 1e-9) ? (Qts * Qes) / denom : -1.0;
        if (q > 0.0)
            updateSuggestBtn(m_btnQms, q, Qms, 3,
                             QString("%1").arg(q, 0, 'f', 4));
        else { m_btnQms->setText("—"); m_btnQms->setEnabled(false); }
    } else {
        m_btnQms->setText("—"); m_btnQms->setEnabled(false);
    }

    // Qes from Qts and Qms  (Qms must be > Qts for positive result)
    if (Qts > 0.0 && Qms > 0.0) {
        const double denom = Qms - Qts;
        const double q     = (denom > 1e-9) ? (Qts * Qms) / denom : -1.0;
        if (q > 0.0)
            updateSuggestBtn(m_btnQes, q, Qes, 3,
                             QString("%1").arg(q, 0, 'f', 4));
        else { m_btnQes->setText("—"); m_btnQes->setEnabled(false); }
    } else {
        m_btnQes->setText("—"); m_btnQes->setEnabled(false);
    }

    // ── Mechanical buttons ────────────────────────────────────────
    // Each parameter can be derived via two paths; use whichever is available.
    // Priority: direct entry (Cms or mms entered) over indirect (Vas+Sd).

    // Effective Sd for Vas-path (prefer entered Sd; fall back to Dd)
    double sdForVas = Sd;
    if (sdForVas <= 0.0 && Dd > 0.0)
        sdForVas = PI * (Dd / 2.0) * (Dd / 2.0);

    // Cms derived from Vas+Sd (used as fallback when Cms not directly entered)
    const double Vas_SI   = m_Vas_L->value() * 1e-3;  // L → m³
    double cms_from_vas   = 0.0;
    if (Vas_SI > 0.0 && sdForVas > 0.0)
        cms_from_vas = Vas_SI / (RHO * C * C * sdForVas * sdForVas);

    // mms from Cms+fs  (Path 1: direct Cms entered)
    // mms from Vas+Sd+fs  (Path 2: Cms derived from Vas)
    {
        double mms_c = 0.0;
        bool ok = false;
        if (Cms > 0.0 && fs > 0.0) {
            const double ws = 2.0 * PI * fs;
            mms_c = 1000.0 / (ws * ws * Cms);   // g
            ok = true;
        } else if (cms_from_vas > 0.0 && fs > 0.0) {
            const double ws = 2.0 * PI * fs;
            mms_c = 1000.0 / (ws * ws * cms_from_vas);   // g
            ok = true;
        }
        if (ok)
            updateSuggestBtn(m_btnMms, mms_c, mms_g, 3,
                             QString("%1 g").arg(mms_c, 0, 'f', 3));
        else { m_btnMms->setText("—"); m_btnMms->setEnabled(false); }
    }

    // Cms from mms+fs  (Path 1: direct mms entered)
    // Cms from Vas+Sd   (Path 2: direct Vas+Sd entered)
    {
        double cms_c = 0.0;
        bool ok = false;
        if (mms > 0.0 && fs > 0.0) {
            const double ws = 2.0 * PI * fs;
            cms_c = 1000.0 / (ws * ws * mms);   // mm/N
            ok = true;
        } else if (cms_from_vas > 0.0) {
            cms_c = cms_from_vas * 1000.0;       // mm/N
            ok = true;
        }
        if (ok)
            updateSuggestBtn(m_btnCms, cms_c, Cms_d, 3,
                             QString("%1 mm/N").arg(cms_c, 0, 'f', 4));
        else { m_btnCms->setText("—"); m_btnCms->setEnabled(false); }
    }

    // Rms from mms+fs+Qms  (Path 1: direct mms entered)
    // Rms from Vas+Sd+fs+Qms  (Path 2: mms derived via Vas)
    {
        double rms_c = 0.0;
        bool ok = false;
        if (mms > 0.0 && fs > 0.0 && Qms > 0.0) {
            rms_c = (2.0 * PI * fs * mms) / Qms;
            ok = true;
        } else if (cms_from_vas > 0.0 && fs > 0.0 && Qms > 0.0) {
            const double ws     = 2.0 * PI * fs;
            const double mms_v  = 1.0 / (ws * ws * cms_from_vas);
            rms_c = (ws * mms_v) / Qms;
            ok = true;
        }
        if (ok)
            updateSuggestBtn(m_btnRms, rms_c, m_Rms->value(), 4,
                             QString("%1 kg/s").arg(rms_c, 0, 'f', 5));
        else { m_btnRms->setText("—"); m_btnRms->setEnabled(false); }
    }

    // ── Re from BL, fs, mms, Qes ─────────────────────────────────
    // Re and BL are stored/entered as "as-connected" values.
    // The T/S formula gives Re_connected directly — no wiring factor needed.
    // Per-coil Rvc is derived from Re_connected:
    //   DVC Parallel: Re = Rvc/2  →  Rvc = 2·Re
    //   DVC Series:   Re = 2·Rvc  →  Rvc = Re/2
    if (BL_v > 0.0 && fs > 0.0 && mms > 0.0 && Qes > 0.0) {
        const double re_c = (BL_v * BL_v * Qes) / (2.0 * PI * fs * mms);
        if (m_rbDVC && m_rbDVC->isChecked()) {
            const double rvc = (m_rbSeries && m_rbSeries->isChecked())
                               ? re_c / 2.0    // series: Rvc = Re/2
                               : re_c * 2.0;   // parallel: Rvc = 2·Re
            if (m_perCoilReLbl)
                m_perCoilReLbl->setText(QString("Per-coil Rvc: %1 Ω").arg(rvc, 0, 'f', 3));
        } else {
            if (m_perCoilReLbl) m_perCoilReLbl->setText("Per-coil Rvc: —");
        }
        updateSuggestBtn(m_btnRe, re_c, Re, 3, QString("%1 Ω").arg(re_c, 0, 'f', 3));
    } else {
        m_btnRe->setText("—"); m_btnRe->setEnabled(false);
        if (m_perCoilReLbl) m_perCoilReLbl->setText("Per-coil Rvc: —");
    }

    // ── Force factor button ───────────────────────────────────────
    // BL_connected = √(2π·fs·mms·Re_connected / Qes) — no wiring factor.
    if (Re > 0.0 && fs > 0.0 && mms > 0.0 && Qes > 0.0) {
        const double bl2 = (2.0 * PI * fs * mms * Re) / Qes;
        if (bl2 > 0.0) {
            const double bl = std::sqrt(bl2);
            updateSuggestBtn(m_btnBL, bl, m_BL->value(), 3,
                             QString("%1 Tm").arg(bl, 0, 'f', 3));
        } else {
            m_btnBL->setText("—"); m_btnBL->setEnabled(false);
        }
    } else {
        m_btnBL->setText("—"); m_btnBL->setEnabled(false);
    }

    // ── Geometry buttons ──────────────────────────────────────────

    // Sd from Dd  →  apply to m_Sd_cm2
    if (Dd > 0.0) {
        const double sd_cm2 = PI * (Dd / 2.0) * (Dd / 2.0) * 1e4;
        updateSuggestBtn(m_btnSd, sd_cm2, m_Sd_cm2->value(), 2,
                         QString("%1 cm²").arg(sd_cm2, 0, 'f', 2));
    } else {
        m_btnSd->setText("—"); m_btnSd->setEnabled(false);
    }

    // Dd from Sd  →  apply to m_Dd_mm
    if (Sd > 0.0) {
        const double dd_mm = 2.0 * std::sqrt(Sd / PI) * 1000.0;
        updateSuggestBtn(m_btnDd, dd_mm, m_Dd_mm->value(), 1,
                         QString("%1 mm").arg(dd_mm, 0, 'f', 1));
    } else {
        m_btnDd->setText("—"); m_btnDd->setEnabled(false);
    }

    // Vas from Cms and Sd (prefer Sd field; fall back to Dd)
    // sdForVas already computed above for Vas-path computations
    if (Cms > 0.0 && sdForVas > 0.0) {
        const double v_L = Cms * RHO * C * C * sdForVas * sdForVas * 1000.0;
        updateSuggestBtn(m_btnVas, v_L, Vas, 3,
                         QString("%1 L").arg(v_L, 0, 'f', 3));
    } else {
        m_btnVas->setText("—"); m_btnVas->setEnabled(false);
    }

    // ── Vd auto-label ─────────────────────────────────────────────
    const double Xmax_m = m_Xmax->value() * 1e-3;
    double sdForVd = Sd;
    if (sdForVd <= 0.0 && Dd > 0.0)
        sdForVd = PI * (Dd / 2.0) * (Dd / 2.0);

    if (sdForVd > 0.0 && Xmax_m > 0.0) {
        const double vd_cm3 = sdForVd * Xmax_m * 1e6;
        m_hintVd->setText(
            QString("Sᵈ × Xmax = <b>%1 cm³</b>").arg(vd_cm3, 0, 'f', 2));
    } else {
        m_hintVd->setText("<i style='color:#999;'>Enter Sᵈ (or Dᵈ) and Xmax to compute Vd</i>");
    }

    // ── Sensitivity SPL ───────────────────────────────────────────
    // K_SPL = 10·log10(ρ·c / (2π·p_ref²)),  p_ref = 20 µPa → p_ref² = 4×10⁻¹⁰ Pa²
    // η₀ (T/S path)  = 4π²·fs³·Vas_m3 / (c³·Qes)
    // η₀ (BL path)   = ρ·BL²·Sd² / (2π·c·mms²·Re)
    // Reverse: given SPL → η₀ → Qes, Vas, or BL (blue secondary buttons)
    if (m_Spl) {
        static const double K_SPL = 10.0 * std::log10(RHO * C / (2.0 * PI * 4.0e-10));
        const double Vas_m3  = Vas * 1e-3;
        const double spl_cur = m_Spl->value();

        // Forward: compute SPL from available params
        bool   spl_ok = false;
        double spl_c  = 0.0;
        if (fs > 0.0 && Vas_m3 > 0.0 && Qes > 0.0) {
            const double eta0 = (4.0*PI*PI * fs*fs*fs * Vas_m3) / (C*C*C * Qes);
            if (eta0 > 0.0) { spl_c = K_SPL + 10.0*std::log10(eta0); spl_ok = true; }
        } else if (BL_v > 0.0 && Re > 0.0 && mms > 0.0 && Sd > 0.0) {
            const double eta0 = (RHO * BL_v*BL_v * Sd*Sd) / (2.0*PI*C * mms*mms * Re);
            if (eta0 > 0.0) { spl_c = K_SPL + 10.0*std::log10(eta0); spl_ok = true; }
        }
        if (spl_ok)
            updateSuggestBtn(m_btnSpl, spl_c, spl_cur, 1,
                             QString("%1 dB").arg(spl_c, 0, 'f', 1));
        else {
            m_btnSpl->setText("—"); m_btnSpl->setEnabled(false);
        }

        // Reverse: derive other params from entered SPL
        if (spl_cur > 0.0) {
            const double eta0_in = std::pow(10.0, (spl_cur - K_SPL) / 10.0);

            // Qes from SPL + fs + Vas
            if (fs > 0.0 && Vas_m3 > 0.0) {
                const double qes_c = (4.0*PI*PI * fs*fs*fs * Vas_m3) / (C*C*C * eta0_in);
                updateSuggestBtn(m_btnSplQes, qes_c, Qes, 3,
                                 QString("%1").arg(qes_c, 0, 'f', 3));
            } else {
                m_btnSplQes->setText("—"); m_btnSplQes->setEnabled(false);
            }

            // Vas from SPL + fs + Qes
            if (fs > 0.0 && Qes > 0.0) {
                const double vas_L = eta0_in * C*C*C * Qes / (4.0*PI*PI * fs*fs*fs) * 1000.0;
                updateSuggestBtn(m_btnSplVas, vas_L, Vas, 3,
                                 QString("%1 L").arg(vas_L, 0, 'f', 3));
            } else {
                m_btnSplVas->setText("—"); m_btnSplVas->setEnabled(false);
            }

            // BL from SPL + Re + mms + Sd
            if (Re > 0.0 && mms > 0.0 && Sd > 0.0) {
                const double bl2 = eta0_in * 2.0*PI*C * mms*mms * Re / (RHO * Sd*Sd);
                if (bl2 > 0.0)
                    updateSuggestBtn(m_btnSplBL, std::sqrt(bl2), BL_v, 3,
                                     QString("%1 Tm").arg(std::sqrt(bl2), 0, 'f', 3));
                else { m_btnSplBL->setText("—"); m_btnSplBL->setEnabled(false); }
            } else {
                m_btnSplBL->setText("—"); m_btnSplBL->setEnabled(false);
            }
        } else {
            m_btnSplQes->setText("—"); m_btnSplQes->setEnabled(false);
            m_btnSplVas->setText("—"); m_btnSplVas->setEnabled(false);
            m_btnSplBL ->setText("—"); m_btnSplBL ->setEnabled(false);
        }
    }
}

// ── Origin colour coding ──────────────────────────────────────────

void DatasheetEntryWidget::updateOriginColors()
{
    // Tracked fields: blue = accepted from suggest; green = typed by user; none = unset
    auto applyTracked = [this](QDoubleSpinBox *spin, quint64 bit) {
        if (m_fieldOrigins & bit)
            spin->setStyleSheet("QDoubleSpinBox { color:#1a7db5; }");
        else if (m_userEnteredFields & bit)
            spin->setStyleSheet("QDoubleSpinBox { color:#2e7d32; }");
        else
            spin->setStyleSheet("");
    };
    applyTracked(m_Re,      FieldOrigin::Re);
    applyTracked(m_Qts,     FieldOrigin::Qts);
    applyTracked(m_Qms,     FieldOrigin::Qms);
    applyTracked(m_Qes,     FieldOrigin::Qes);
    applyTracked(m_mms_g,   FieldOrigin::Mms);
    applyTracked(m_Cms_mmN, FieldOrigin::Cms);
    applyTracked(m_Rms,     FieldOrigin::Rms);
    applyTracked(m_BL,      FieldOrigin::BL);
    applyTracked(m_Sd_cm2,  FieldOrigin::Sd);
    applyTracked(m_Dd_mm,   FieldOrigin::Dd);
    applyTracked(m_Vas_L,   FieldOrigin::Vas);
    if (m_Spl) applyTracked(m_Spl, FieldOrigin::Spl);

    // Non-tracked fields: can only be user-entered; green if non-zero
    auto applyPlain = [](QDoubleSpinBox *spin) {
        spin->setStyleSheet(spin->value() > 0.0
            ? "QDoubleSpinBox { color:#2e7d32; }" : "");
    };
    applyPlain(m_fs);
    applyPlain(m_Le_mH);
    applyPlain(m_Znom);
    applyPlain(m_fLe);
    applyPlain(m_KLe);
    applyPlain(m_Xmax);
    applyPlain(m_Xlim);
    applyPlain(m_Pe);
    applyPlain(m_Hg);
}

// ── Integrity check ───────────────────────────────────────────────

void DatasheetEntryWidget::onIntegrityCheck()
{
    const double Re     = m_Re->value();
    const double fs     = m_fs->value();
    const double Qms    = m_Qms->value();
    const double Qes    = m_Qes->value();
    const double Qts    = m_Qts->value();
    const double mms    = m_mms_g->value()   / 1000.0;   // g → kg
    const double Cms    = m_Cms_mmN->value() * 1e-3;     // mm/N → m/N
    const double Rms    = m_Rms->value();
    const double BL     = m_BL->value();
    const double Dd     = m_Dd_mm->value()   / 1000.0;   // mm → m
    const double Sd     = m_Sd_cm2->value()  * 1e-4;     // cm² → m²
    const double Vas    = m_Vas_L->value()   * 1e-3;     // L → m³

    // Derive Sd to use (prefer Sd field; compute from Dd if absent)
    double sdEff = Sd;
    if (sdEff <= 0.0 && Dd > 0.0)
        sdEff = PI * (Dd / 2.0) * (Dd / 2.0);

    static constexpr double TOL = 5.0;   // % tolerance for pass/fail

    struct Check {
        QString  label;
        bool     run;
        double   entered;
        double   computed;
        QString  unit;
        int      decimals;
    };

    // ── Compute all derivable values ──────────────────────────────

    // Qts from Qms, Qes
    double qts_c = 0.0;
    bool   qts_ok = (Qms > 0.0 && Qes > 0.0);
    if (qts_ok) qts_c = (Qms * Qes) / (Qms + Qes);

    // Cms from mms, fs
    double cms_c  = 0.0;
    bool   cms_ok = (mms > 0.0 && fs > 0.0);
    if (cms_ok) { const double ws = 2.0*PI*fs; cms_c = 1.0/(ws*ws*mms); }

    // Rms from mms, fs, Qms
    double rms_c  = 0.0;
    bool   rms_ok = (mms > 0.0 && fs > 0.0 && Qms > 0.0);
    if (rms_ok) rms_c = (2.0*PI*fs*mms)/Qms;

    // BL from Re, fs, mms, Qes
    double bl_c   = 0.0;
    bool   bl_ok  = (Re > 0.0 && fs > 0.0 && mms > 0.0 && Qes > 0.0);
    if (bl_ok) { const double b2 = (2.0*PI*fs*mms*Re)/Qes; bl_c = (b2>0) ? std::sqrt(b2) : 0.0; }

    // Sd from Dd
    double sd_c   = 0.0;
    bool   sd_ok  = (Dd > 0.0 && Sd > 0.0);     // only meaningful when both entered
    if (sd_ok) sd_c = PI*(Dd/2.0)*(Dd/2.0);

    // Vas from Cms, Sd
    double vas_c  = 0.0;
    bool   vas_ok = (Cms > 0.0 && sdEff > 0.0);
    if (vas_ok) vas_c = Cms*RHO*C*C*sdEff*sdEff;

    // Qes from Re, fs, mms, BL
    double qes_c  = 0.0;
    bool   qes_ok = (BL > 0.0 && Re > 0.0 && fs > 0.0 && mms > 0.0);
    if (qes_ok) qes_c = (2.0*PI*fs*mms*Re)/(BL*BL);

    // ── Build result HTML ──────────────────────────────────────────
    QVector<Check> checks = {
        { "Qts  (from Qms+Qes)",        qts_ok && Qts>0,   Qts,         qts_c,        "",      4 },
        { "Cms  (from mms+fₛ)",          cms_ok && Cms>0,   Cms*1000.0,  cms_c*1000.0, "mm/N",  4 },
        { "Rms  (from mms+fₛ+Qms)",     rms_ok && Rms>0,   Rms,         rms_c,        "kg/s",  5 },
        { "BL   (from Rₑ+fₛ+mms+Qes)", bl_ok  && BL>0,    BL,          bl_c,         "Tm",    3 },
        { "Sᵈ   (from Dᵈ)",             sd_ok,             Sd*1e4,      sd_c*1e4,     "cm²",   2 },
        { "Vas  (from Cms+Sᵈ)",          vas_ok && Vas>0,   Vas*1000.0,  vas_c*1000.0, "L",     3 },
        { "Qes  (from Rₑ+fₛ+mms+BL)",  qes_ok && Qes>0,   Qes,         qes_c,        "",      4 },
    };

    QString html;
    html += "<table cellspacing='4' style='width:100%;'>";
    html += "<tr style='background:#dfe6e9;'>"
            "<th align='left'>Check</th>"
            "<th align='right'>Entered</th>"
            "<th align='right'>Computed</th>"
            "<th align='right'>Deviation</th>"
            "<th align='left'>  Status</th>"
            "</tr>";

    int passed = 0, failed = 0, skipped = 0;
    for (const auto &ch : checks) {
        if (!ch.run) {
            ++skipped;
            html += QString(
                "<tr><td>%1</td>"
                "<td colspan='4' align='center' style='color:#888;'>– insufficient data –</td>"
                "</tr>").arg(ch.label);
            continue;
        }

        const double pct = pctDev(ch.entered, ch.computed);
        const bool   ok  = pct <= TOL;
        ok ? ++passed : ++failed;

        const QString color = ok ? "#27ae60" : "#c0392b";
        const QString mark  = ok ? "&#10003; PASS" : "&#10007; FAIL";

        html += QString(
            "<tr>"
            "<td>%1</td>"
            "<td align='right'>%2 %3</td>"
            "<td align='right'>%4 %3</td>"
            "<td align='right' style='color:%5;'>%6 %</td>"
            "<td style='color:%5;font-weight:bold;'>  %7</td>"
            "</tr>")
            .arg(ch.label)
            .arg(ch.entered,  0, 'f', ch.decimals).arg(ch.unit)
            .arg(ch.computed, 0, 'f', ch.decimals)
            .arg(color)
            .arg(pct,  0, 'f', 2)
            .arg(mark);
    }
    html += "</table>";

    // Summary line
    const QString sumColor = (failed == 0) ? "#27ae60" : "#c0392b";
    html += QString("<p style='margin-top:8px;color:%1;font-weight:bold;'>")
            .arg(sumColor);
    if (failed == 0)
        html += QString("All %1 checks passed (tolerance ±%2 %). "
                        "%3 check(s) skipped (missing inputs).")
                .arg(passed).arg(TOL, 0, 'f', 0).arg(skipped);
    else
        html += QString("%1 check(s) FAILED, %2 passed, %3 skipped.  "
                        "Review the values above — check for unit mismatches "
                        "or transcription errors.")
                .arg(failed).arg(passed).arg(skipped);
    html += "</p>";

    m_integrityResult->setText(html);
    m_integrityBox->setVisible(true);
}

// ── Record assembly ───────────────────────────────────────────────

DriverRecord DatasheetEntryWidget::collectRecord() const
{
    DriverRecord r;
    r.id           = m_editId;
    r.make         = m_make->text().trimmed();
    r.model        = m_model->text().trimmed();
    r.dateMeasured = m_date->date();
    r.measuredBy   = m_source->text().trimmed();
    r.notes        = m_notes->toPlainText();

    // Electrical
    r.Re = m_Re->value();
    r.Le = m_Le_mH->value() * 1e-3;   // mH → H

    // Resonance
    r.fs = m_fs->value();

    // Q factors
    r.Qms = m_Qms->value();
    r.Qes = m_Qes->value();
    r.Qts = m_Qts->value();

    // Mechanical (convert display units → SI)
    r.mms = m_mms_g->value()   / 1000.0;   // g → kg
    r.Cms = m_Cms_mmN->value() * 1e-3;     // mm/N → m/N
    r.Rms = m_Rms->value();

    // Force factor
    r.BL = m_BL->value();

    // Geometry – prefer the Sd field; derive Dd/Sd from each other if one is zero
    const double ddRaw = m_Dd_mm->value()  / 1000.0;   // mm → m
    const double sdRaw = m_Sd_cm2->value() * 1e-4;     // cm² → m²

    if (ddRaw > 0.0 && sdRaw <= 0.0) {
        r.Dd = ddRaw;
        r.Sd = PI * (ddRaw / 2.0) * (ddRaw / 2.0);
    } else if (sdRaw > 0.0 && ddRaw <= 0.0) {
        r.Sd = sdRaw;
        r.Dd = 2.0 * std::sqrt(sdRaw / PI);
    } else {
        // Both entered — store both; integrity check will flag any mismatch
        r.Dd = ddRaw;
        r.Sd = sdRaw;
    }

    // Volume
    r.Vas = m_Vas_L->value() * 1e-3;   // L → m³

    // Additional linear parameters
    r.Znom = m_Znom->value();
    r.fLe  = m_fLe->value();
    r.KLe  = m_KLe->value();

    // Large signal parameters
    r.Xmax = m_Xmax->value();           // stored as mm
    r.Xlim = m_Xlim->value();           // stored as mm
    r.Pe   = m_Pe->value();
    r.Hg   = m_Hg->value();            // stored as mm

    // Vd computed from Sd (or Dd) × Xmax [cm³]
    {
        double sdEff = r.Sd;
        if (sdEff <= 0.0 && r.Dd > 0.0)
            sdEff = PI * (r.Dd / 2.0) * (r.Dd / 2.0);
        const double xmax_m = r.Xmax * 1e-3;
        r.Vd = (sdEff > 0.0 && xmax_m > 0.0) ? sdEff * xmax_m * 1e6 : 0.0;
    }

    // Sensitivity
    r.Spl = m_Spl ? m_Spl->value() : 0.0;

    // Voice coil type
    r.isDVC     = m_rbDVC && m_rbDVC->isChecked();
    r.dvcWiring = (m_rbParallel && m_rbParallel->isChecked()) ? 1 : 0;

    // Field origins
    r.fieldOrigins      = m_fieldOrigins;
    r.userEnteredFields = m_userEnteredFields;

    // Preserve raw measurement fields from the originally loaded record
    // so editing a measured driver doesn't destroy its measurement data.
    r.V_meas      = m_loadedRaw.V_meas;
    r.R_s         = m_loadedRaw.R_s;
    r.voltageMode = m_loadedRaw.voltageMode;
    r.Zmax        = m_loadedRaw.Zmax;
    r.f1          = m_loadedRaw.f1;
    r.f2          = m_loadedRaw.f2;
    r.deltaM      = m_loadedRaw.deltaM;
    r.fo          = m_loadedRaw.fo;
    r.Zmin        = m_loadedRaw.Zmin;
    r.f3          = m_loadedRaw.f3;

    return r;
}

void DatasheetEntryWidget::onSave()
{
    DriverRecord r = collectRecord();
    if (r.make.isEmpty() || r.model.isEmpty()) {
        m_statusLbl->setStyleSheet("color:#e74c3c;");
        m_statusLbl->setText("Manufacturer and Model are required.");
        return;
    }
    if (r.fs <= 0.0) {
        m_statusLbl->setStyleSheet("color:#e74c3c;");
        m_statusLbl->setText("fₛ must be > 0.");
        return;
    }
    if (!m_db->saveDriver(r)) {
        QMessageBox::critical(this, "Database error",
            "Could not save:\n" + m_db->lastError());
        return;
    }
    m_editId = r.id;
    m_statusLbl->setStyleSheet("color:#27ae60; font-weight:bold;");
    m_statusLbl->setText(
        QString("Saved: %1 %2  (id=%3)")
        .arg(r.make, r.model).arg(r.id));
    emit driverSaved(r.id);
}

void DatasheetEntryWidget::onSaveAsNew()
{
    DriverRecord r = collectRecord();
    if (r.make.isEmpty() || r.model.isEmpty()) {
        m_statusLbl->setStyleSheet("color:#e74c3c;");
        m_statusLbl->setText("Manufacturer and Model are required.");
        return;
    }
    if (r.fs <= 0.0) {
        m_statusLbl->setStyleSheet("color:#e74c3c;");
        m_statusLbl->setText("fₛ must be > 0.");
        return;
    }
    r.id = -1;  // force insert
    if (!m_db->saveDriver(r)) {
        QMessageBox::critical(this, "Database error",
            "Could not save:\n" + m_db->lastError());
        return;
    }
    m_editId = r.id;
    m_loadedRaw.id = r.id;
    m_statusLbl->setStyleSheet("color:#1a7db5; font-weight:bold;");
    m_statusLbl->setText(
        QString("Saved as new: %1 %2  (id=%3)")
        .arg(r.make, r.model).arg(r.id));
    emit driverSaved(r.id);
}

// ── Clear / load ─────────────────────────────────────────────────

void DatasheetEntryWidget::clear()
{
    m_make->clear(); m_model->clear(); m_source->clear();
    m_date->setDate(QDate::currentDate());
    m_notes->clear();

    for (auto *s : {m_Re, m_Le_mH, m_fs,
                    m_Qms, m_Qes, m_Qts,
                    m_mms_g, m_Cms_mmN, m_Rms,
                    m_BL,
                    m_Dd_mm, m_Sd_cm2, m_Vas_L,
                    m_Spl,
                    m_Znom, m_fLe, m_KLe,
                    m_Xmax, m_Xlim, m_Pe, m_Hg})
        s->setValue(0.0);

    for (auto *btn : {m_btnRe, m_btnQts, m_btnQms, m_btnQes,
                      m_btnMms, m_btnCms, m_btnRms,
                      m_btnBL, m_btnSd, m_btnDd, m_btnVas,
                      m_btnSpl, m_btnSplQes, m_btnSplVas, m_btnSplBL}) {
        btn->setText("—");
        btn->setEnabled(false);
    }

    m_fieldOrigins      = 0;
    m_userEnteredFields = 0;
    updateOriginColors();
    if (m_rbSVC) m_rbSVC->setChecked(true);
    if (m_rbSeries) m_rbSeries->setChecked(true);

    m_hintVd->setText("<i style='color:#999;'>Enter Sᵈ (or Dᵈ) and Xmax to compute Vd</i>");

    m_integrityBox->setVisible(false);
    m_statusLbl->clear();
    m_editId = -1;
    m_loadedRaw = {};
    if (m_btnSaveAsNew) m_btnSaveAsNew->setVisible(false);
}

void DatasheetEntryWidget::loadRecord(const DriverRecord &r)
{
    clear();
    m_editId    = r.id;
    m_loadedRaw = r;

    // Restore origins before setValue calls so the guard is in place
    m_applyingHint      = true;
    m_fieldOrigins      = r.fieldOrigins;
    m_userEnteredFields = r.userEnteredFields;

    m_make->setText(r.make);
    m_model->setText(r.model);
    if (r.dateMeasured.isValid()) m_date->setDate(r.dateMeasured);
    m_source->setText(r.measuredBy);
    m_notes->setPlainText(r.notes);

    m_Re->setValue(r.Re);
    m_Le_mH->setValue(r.Le * 1000.0);      // H → mH
    m_fs->setValue(r.fs);
    m_Qms->setValue(r.Qms);
    m_Qes->setValue(r.Qes);
    m_Qts->setValue(r.Qts);
    m_mms_g->setValue(r.mms * 1000.0);     // kg → g
    m_Cms_mmN->setValue(r.Cms * 1000.0);   // m/N → mm/N
    m_Rms->setValue(r.Rms);
    m_BL->setValue(r.BL);
    m_Dd_mm->setValue(r.Dd * 1000.0);      // m → mm
    m_Sd_cm2->setValue(r.Sd * 1e4);        // m² → cm²
    m_Vas_L->setValue(r.Vas * 1000.0);     // m³ → L

    m_Znom->setValue(r.Znom);
    m_fLe->setValue(r.fLe);
    m_KLe->setValue(r.KLe);
    m_Xmax->setValue(r.Xmax);              // mm
    m_Xlim->setValue(r.Xlim);              // mm
    m_Pe->setValue(r.Pe);
    m_Hg->setValue(r.Hg);                  // mm
    m_Spl->setValue(r.Spl);               // dB

    // Restore DVC state
    if (m_rbSVC && m_rbDVC) {
        if (r.isDVC) m_rbDVC->setChecked(true);
        else         m_rbSVC->setChecked(true);
    }
    if (m_rbSeries && m_rbParallel) {
        if (r.dvcWiring == 1) m_rbParallel->setChecked(true);
        else                  m_rbSeries->setChecked(true);
    }

    m_applyingHint = false;
    if (m_btnSaveAsNew) m_btnSaveAsNew->setVisible(r.id >= 0);
    updateOriginColors();
    updateHints();
}
