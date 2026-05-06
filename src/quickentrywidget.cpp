#include "quickentrywidget.h"
#include "tscalculator.h"
#include "driverrecord.h"
#include "noscrollspinbox.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
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

// ── helpers ──────────────────────────────────────────────────────

static QDoubleSpinBox *mkSpin(double lo, double hi, int dec,
                               double step, const QString &sfx)
{
    auto *s = new NoScrollSpinBox;
    s->setRange(lo,hi); s->setDecimals(dec);
    s->setSingleStep(step); s->setSuffix(sfx);
    s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return s;
}

// Hint label: amber left-border box, hidden initially
static QLabel *mkHintLbl()
{
    auto *l = new QLabel;
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setStyleSheet(
        "QLabel{background:#f8f0f2;border-left:4px solid #8b3a50;"
        "padding:6px 10px;border-radius:3px;color:#4a1a2e;}");
    l->setVisible(false);
    return l;
}

// Explicit form label (fixes white-on-white on dark-theme Linux)
static QLabel *mkFormLabel(const QString &text)
{
    auto *l = new QLabel(text);
    l->setStyleSheet("color:#3a3a3a; background:transparent;");
    return l;
}

static QLabel *mkResLbl()
{
    auto *l = new QLabel("–");
    l->setTextFormat(Qt::RichText);
    l->setStyleSheet("font-weight:bold; font-size:10pt; color:#3a3a3a; padding:2px 0;");
    return l;
}

static void addResRow(QGridLayout *g, int &row,
                      const QString &sym, const QString &name,
                      const QString &unit, QLabel *&out)
{
    auto *s = new QLabel("<b>"+sym+"</b>"); s->setStyleSheet("color:#3a3a3a;");
    auto *n = new QLabel(name);             n->setStyleSheet("color:#555;");
    out = mkResLbl();
    auto *u = new QLabel(unit);             u->setStyleSheet("color:#888;");
    g->addWidget(s,  row, 0);
    g->addWidget(n,  row, 1);
    g->addWidget(out,row, 2);
    g->addWidget(u,  row, 3);
    ++row;
}

// Helper: add a form row with an explicit dark label
static void addRow(QFormLayout *f, const QString &lbl, QWidget *w)
{
    f->addRow(mkFormLabel(lbl), w);
}

// ─────────────────────────────────────────────────────────────────
QuickEntryWidget::QuickEntryWidget(DriverDatabase *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    buildUi();
}

QString QuickEntryWidget::voltageUnit() const
{
    return m_rbPp->isChecked() ? "V pp" : "V rms";
}

void QuickEntryWidget::buildUi()
{
    auto *inner = new QWidget;
    auto *vbox  = new QVBoxLayout(inner);
    vbox->setSpacing(10);
    vbox->setContentsMargins(16,16,16,16);

    auto *title = new QLabel("Quick Measurement – Thiele/Small Parameters");
    title->setStyleSheet("font-size:14pt; font-weight:bold; color:#3a3a3a;");
    vbox->addWidget(title);
    auto *sub = new QLabel(
        "Enter all values. Voltage hints appear as you type. "
        "Click <b>Calculate</b> to preview results, then <b>Save</b>.");
    sub->setWordWrap(true);
    sub->setStyleSheet("color:#555; margin-bottom:4px;");
    vbox->addWidget(sub);

    // ── Identity ─────────────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Driver Identification");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight); form->setSpacing(8);
        m_make       = new QLineEdit;
        m_model      = new QLineEdit;
        m_measuredBy = new QLineEdit;
        m_date       = new QDateEdit(QDate::currentDate());
        m_date->setCalendarPopup(true);
        m_date->setDisplayFormat("yyyy-MM-dd");
        addRow(form, "Manufacturer / Make:", m_make);
        addRow(form, "Model:",               m_model);
        addRow(form, "Date measured:",       m_date);
        addRow(form, "Measured by:",         m_measuredBy);
        vbox->addWidget(box);
    }

    // ── Measurement Setup ────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Measurement Setup  (set once — do not change during measurements)");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight); form->setSpacing(8);

        m_rs    = mkSpin(1.0,  1000.0, 1, 1.0, " Ω");
        m_rs->setValue(50.0);
        m_vmeas = mkSpin(0.01, 50.0,   3, 0.1, " V");
        m_vmeas->setValue(1.0);
        m_rbRms = new QRadioButton("RMS  (true-RMS meter / oscilloscope rms mode)");
        m_rbPp  = new QRadioButton("Peak-to-peak  (oscilloscope Vpp mode)");
        m_rbRms->setChecked(true);
        const QString rbStyle =
            "QRadioButton { color:#3a3a3a; padding:4px 8px; border-radius:4px; }"
            "QRadioButton:checked { background:#6b2a40; color:white; font-weight:bold; }";
        m_rbRms->setStyleSheet(rbStyle);
        m_rbPp->setStyleSheet(rbStyle);

        auto *bg = new QButtonGroup(this);
        bg->addButton(m_rbRms,0); bg->addButton(m_rbPp,1);

        addRow(form, "Series resistor R_s:", m_rs);
        addRow(form, "Applied voltage V:",    m_vmeas);
        auto *vbMode = new QVBoxLayout;
        vbMode->addWidget(new QLabel(
            "<span style='color:#3a3a3a;font-weight:bold;'>Voltage format on your instrument:</span>"));
        vbMode->addWidget(m_rbRms);
        vbMode->addWidget(m_rbPp);
        form->addRow(vbMode);

        // Reconnect hints when setup changes
        connect(m_vmeas, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updateFreeAirHints);
        connect(m_vmeas, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updatePhysicalHints);
        connect(m_rs,    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updateFreeAirHints);
        connect(m_rs,    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updatePhysicalHints);
        connect(m_rbPp, &QRadioButton::toggled,
                this, &QuickEntryWidget::updateFreeAirHints);
        connect(m_rbPp, &QRadioButton::toggled,
                this, &QuickEntryWidget::updatePhysicalHints);

        vbox->addWidget(box);
    }

    // ── Voice Coil Configuration ─────────────────────────────────
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
        auto *dvcRow = new QHBoxLayout(m_dvcOptions);
        dvcRow->setContentsMargins(18, 0, 0, 0);
        dvcRow->setSpacing(8);
        m_rbSeries   = new QRadioButton("Series wiring  (Reff = 2·Rₑ)");
        m_rbParallel = new QRadioButton("Parallel wiring  (Reff = Rₑ/2)");
        m_rbSeries->setChecked(true);
        m_rbSeries->setStyleSheet(rbStyle);
        m_rbParallel->setStyleSheet(rbStyle);
        auto *wirGroup = new QButtonGroup(this);
        wirGroup->addButton(m_rbSeries, 0);
        wirGroup->addButton(m_rbParallel, 1);
        dvcRow->addWidget(m_rbSeries);
        dvcRow->addWidget(m_rbParallel);
        dvcRow->addStretch();
        m_dvcOptions->setVisible(false);

        connect(m_rbDVC, &QRadioButton::toggled, this, [this](bool on){
            m_dvcOptions->setVisible(on);
        });

        vl->addWidget(m_rbSVC);
        vl->addWidget(m_rbDVC);
        vl->addWidget(m_dvcOptions);
        vbox->addWidget(box);
    }

    // ── Step 1 – Re ──────────────────────────────────────────────
    {
        auto *box  = new QGroupBox("Step 1 – DC Resistance");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight);
        m_Re = mkSpin(0.10,200.0,3,0.01," Ω");
        addRow(form,"Rₑ – voice coil DC resistance:", m_Re);
        auto *hint = mkFormLabel("4-wire ohmmeter, range 0–20 Ω, accuracy ±0.1 Ω or better.");
        hint->setStyleSheet("color:#666; font-size:8pt; font-style:italic;");
        form->addRow(hint);
        connect(m_Re, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updateFreeAirHints);
        vbox->addWidget(box);
    }

    // ── Step 2 – Free-air impedance ──────────────────────────────
    {
        auto *box  = new QGroupBox("Step 2 – Free-Air Impedance Curve");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight); form->setSpacing(8);

        m_fs     = mkSpin(1.0,  2000.0, 2, 0.1,  " Hz");
        m_Vpeak  = mkSpin(0.001, 50.0, 4, 0.001, " V");
        m_f1     = mkSpin(1.0,  2000.0, 2, 0.1,  " Hz");
        m_f2     = mkSpin(1.0,  2000.0, 2, 0.1,  " Hz");

        m_hintZ12     = mkHintLbl();
        m_hintVtarget = mkHintLbl();

        addRow(form,"fₛ  – free-air resonance:", m_fs);
        addRow(form,"V at peak (voltage across driver at fₛ):", m_Vpeak);

        // Hint labels appear between Zmax and f1/f2
        form->addRow(m_hintZ12);
        form->addRow(m_hintVtarget);

        addRow(form,"f₁  – lower side frequency (Z = Z₁):", m_f1);
        addRow(form,"f₂  – upper side frequency (Z = Z₂):", m_f2);

        auto *note = mkFormLabel("Z₁ = Z₂ = √(Rₑ·Zmax).  Verify: √(f₁·f₂) ≈ fₛ.");
        note->setStyleSheet("color:#666; font-size:8pt; font-style:italic;");
        form->addRow(note);

        connect(m_Vpeak, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updateFreeAirHints);
        vbox->addWidget(box);
    }

    // ── Step 3 – Added mass ──────────────────────────────────────
    {
        auto *box  = new QGroupBox("Step 3 – Delta Mass (Δm)");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight); form->setSpacing(8);
        m_deltaM_g = mkSpin(0.1,500.0,2,0.5, " g");
        m_fo       = mkSpin(1.0,2000.0,2,0.1," Hz");
        addRow(form,"Δm  – added mass (plasticine):", m_deltaM_g);
        addRow(form,"f₀  – resonance with mass:",     m_fo);
        auto *note = mkFormLabel("Add ~70 % of expected mms to cone centre. f₀ must be < fₛ.");
        note->setStyleSheet("color:#666; font-size:8pt; font-style:italic;");
        form->addRow(note);
        vbox->addWidget(box);
    }

    // ── Step 4 – Physical / f3 ───────────────────────────────────
    {
        auto *box  = new QGroupBox("Step 4 – Physical Dimensions & f₃");
        auto *form = new QFormLayout(box);
        form->setLabelAlignment(Qt::AlignRight); form->setSpacing(8);
        m_Dd_mm = mkSpin(10.0,  800.0, 1, 0.5,   " mm");
        m_Zmin  = mkSpin(0.0001, 50.0, 4, 0.001, " V rms");
        m_f3    = mkSpin(100.0,100000.0,0,100.0, " Hz");

        m_hintVmin = mkHintLbl();
        m_hintVf3  = mkHintLbl();

        addRow(form,"Dᵈ  – piston diameter (mid-surround):", m_Dd_mm);
        addRow(form,"V at Zmin – voltage at impedance minimum:", m_Zmin);

        // Hints appear between Zmin voltage and f3
        form->addRow(m_hintVmin);
        form->addRow(m_hintVf3);

        addRow(form,"f₃  – frequency at Zmin + 3 dB:",       m_f3);

        auto *note = mkFormLabel("Enter the voltage measured at the impedance minimum above fₛ.\n"
                                 "Zmin and target voltage for f₃ are calculated automatically.");
        note->setStyleSheet("color:#666; font-size:8pt; font-style:italic;");
        form->addRow(note);

        connect(m_Zmin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &QuickEntryWidget::updatePhysicalHints);
        vbox->addWidget(box);
    }

    // ── Notes ────────────────────────────────────────────────────
    {
        auto *box = new QGroupBox("Measurement Notes");
        auto *bl  = new QVBoxLayout(box);
        m_notes = new QTextEdit;
        m_notes->setPlaceholderText(
            "Setup details, driver condition, temperature, equipment used…");
        m_notes->setFixedHeight(80);
        bl->addWidget(m_notes);
        vbox->addWidget(box);
    }

    // ── Action buttons ───────────────────────────────────────────
    {
        auto *hb      = new QHBoxLayout;
        auto *btnCalc = new QPushButton("  Calculate & Continue to Entry  ");
        btnCalc->setStyleSheet("QPushButton{background:#6b2a40;color:white;"
                               "border-radius:4px;padding:8px 24px;font-weight:bold;}");
        auto *btnClear = new QPushButton("Clear");
        btnClear->setStyleSheet("QPushButton{background:#bdc3c7;color:#3a3a3a;"
                                "border-radius:4px;padding:8px 16px;}");
        m_statusLbl = new QLabel;
        m_statusLbl->setWordWrap(true);
        hb->addWidget(btnCalc);
        hb->addWidget(btnClear);
        hb->addStretch();
        vbox->addLayout(hb);
        vbox->addWidget(m_statusLbl);

        connect(btnCalc,  &QPushButton::clicked, this, &QuickEntryWidget::onCalculate);
        connect(btnClear, &QPushButton::clicked, this, &QuickEntryWidget::clear);
    }

    // ── Results panel ────────────────────────────────────────────
    buildResultsGroup();
    vbox->addWidget(m_resultsBox);
    m_resultsBox->setVisible(false);
    vbox->addStretch();

    // ── Scroll area ──────────────────────────────────────────────
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inner);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0,0,0,0);
    outerLayout->addWidget(scroll);
}

void QuickEntryWidget::buildResultsGroup()
{
    m_resultsBox = new QGroupBox("Calculated Thiele/Small Parameters");
    auto *grid   = new QGridLayout(m_resultsBox);
    grid->setColumnStretch(1,2); grid->setSpacing(4);

    auto hdr = [&](int col, const QString &t) {
        auto *l = new QLabel("<b>"+t+"</b>"); l->setStyleSheet("color:#777;font-size:8pt;");
        grid->addWidget(l,0,col);
    };
    hdr(0,"Sym"); hdr(1,"Parameter"); hdr(2,"Value"); hdr(3,"Unit");

    int r=1;
    addResRow(grid,r,"Rₑ","DC resistance","Ω",m_resRe);
    addResRow(grid,r,"fₛ","Free-air resonance","Hz",m_resFs);
    addResRow(grid,r,"Qms","Mechanical Q-factor","–",m_resQms);
    addResRow(grid,r,"Qes","Electrical Q-factor","–",m_resQes);
    addResRow(grid,r,"Qts","Total Q-factor","–",m_resQts);
    addResRow(grid,r,"mms","Moving mass incl. air","g",m_resMms);
    addResRow(grid,r,"Cms","Suspension compliance","mm/N",m_resCms);
    addResRow(grid,r,"Rms","Mechanical resistance","kg/s",m_resRms);
    addResRow(grid,r,"BL","Force factor","Tm",m_resBL);
    addResRow(grid,r,"Sᵈ","Effective piston area","cm²",m_resSd);
    addResRow(grid,r,"Vas","Equivalent air volume","litres",m_resVas);
    addResRow(grid,r,"Lₑ","Voice-coil inductance","mH",m_resLe);

    grid->addWidget(new QLabel("<b>√(f₁·f₂)</b>"),r,0);
    grid->addWidget(new QLabel("Resonance verify"),r,1);
    m_resVerify = new QLabel("–"); m_resVerify->setTextFormat(Qt::RichText);
    grid->addWidget(m_resVerify,r,2,1,2);
}

// ── Live voltage hints ────────────────────────────────────────────

void QuickEntryWidget::updateFreeAirHints()
{
    const double Re    = m_Re->value();
    const double Vpeak = m_Vpeak->value();
    const double Vmeas = m_vmeas->value();
    const double Rs    = m_rs->value();
    const QString vu   = voltageUnit();

    // Derive Zmax from the measured voltage at the resonance peak
    const double denom = Vmeas - Vpeak;
    if (Re > 0 && Vpeak > 0 && denom > 0 && Rs > 0) {
        const double Zmax = Rs * Vpeak / denom;
        const double Z12  = std::sqrt(Re * Zmax);
        m_hintZ12->setText(
            QString("Zmax = Rₛ · Vpeak / (V − Vpeak) = <b>%1 Ω</b>  →  "
                    "Z₁ = Z₂ = √(Rₑ · Zmax) = <b>%2 Ω</b>")
            .arg(Zmax,0,'f',2).arg(Z12,0,'f',3));
        m_hintZ12->setVisible(true);

        const double V12 = TSCalculator::driverVoltage(Vmeas, Rs, Z12);
        m_hintVtarget->setText(
            QString("&#128269; <b>Target voltage at f₁ and f₂:</b>  "
                    "<span style='font-size:11pt;color:#c0392b;'>%1 %2</span>"
                    "  — sweep either side of the resonance peak and mark the two "
                    "frequencies where your meter reads this value.")
            .arg(V12,0,'f',4).arg(vu));
        m_hintVtarget->setVisible(true);
    } else {
        m_hintZ12->setVisible(false);
        m_hintVtarget->setVisible(false);
    }
}

void QuickEntryWidget::updatePhysicalHints()
{
    // m_Zmin now holds the voltage at the impedance minimum
    const double VZmin = m_Zmin->value();
    const double Vmeas = m_vmeas->value();
    const double Rs    = m_rs->value();
    const QString vu   = voltageUnit();

    // Keep suffix in sync with voltage mode
    m_Zmin->setSuffix(" " + vu);

    const double denom = Vmeas - VZmin;
    if (VZmin > 0 && denom > 0 && Vmeas > 0 && Rs > 0) {
        const double Zmin = Rs * VZmin / denom;
        const double Zf3  = std::sqrt(2.0) * Zmin;
        const double Vf3  = TSCalculator::driverVoltage(Vmeas, Rs, Zf3);

        m_hintVmin->setText(
            QString("Computed Zmin = Rₛ · V / (V_applied − V) = <b>%1 Ω</b>")
            .arg(Zmin, 0, 'f', 3));
        m_hintVmin->setVisible(true);

        m_hintVf3->setText(
            QString("&#128269; <b>Target voltage for f₃ (3 dB above Zmin):</b>  "
                    "<span style='font-size:11pt;color:#c0392b;'>%1 %2</span>"
                    "  — Z_f₃ = √2 × %3 = %4 Ω.  "
                    "Scan above the resonance peak until you first reach this voltage.")
            .arg(Vf3,0,'f',4).arg(vu).arg(Zmin,0,'f',3).arg(Zf3,0,'f',3));
        m_hintVf3->setVisible(true);
    } else {
        m_hintVmin->setVisible(false);
        m_hintVf3->setVisible(false);
    }
}

// ── Record assembly & results ─────────────────────────────────────

DriverRecord QuickEntryWidget::collectRecord() const
{
    DriverRecord r;
    r.id           = m_editId;
    r.make         = m_make->text().trimmed();
    r.model        = m_model->text().trimmed();
    r.dateMeasured = m_date->date();
    r.measuredBy   = m_measuredBy->text().trimmed();
    r.notes        = m_notes->toPlainText();
    r.V_meas       = m_vmeas->value();
    r.R_s          = m_rs->value();
    r.voltageMode  = m_rbPp->isChecked() ? 1 : 0;
    r.Re           = m_Re->value();
    r.fs           = m_fs->value();
    {
        const double vp = m_Vpeak->value();
        const double d  = r.V_meas - vp;
        r.Zmax = (d > 0.0) ? (r.R_s * vp / d) : 0.0;
    }
    r.f1           = m_f1->value();
    r.f2           = m_f2->value();
    r.deltaM       = m_deltaM_g->value() / 1000.0;
    r.fo           = m_fo->value();
    r.Dd           = m_Dd_mm->value() / 1000.0;
    {
        const double VZmin = m_Zmin->value();
        const double d     = r.V_meas - VZmin;
        r.Zmin = (VZmin > 0.0 && d > 0.0) ? (r.R_s * VZmin / d) : 0.0;
    }
    r.f3           = m_f3->value();
    r.isDVC        = m_rbDVC && m_rbDVC->isChecked();
    r.dvcWiring    = (m_rbParallel && m_rbParallel->isChecked()) ? 1 : 0;
    return r;
}

void QuickEntryWidget::showResults(const DriverRecord &r)
{
    auto fmt = [](double v, int d){ return QString::number(v,'f',d); };
    m_resRe ->setText(fmt(r.Re,  3));
    m_resFs ->setText(fmt(r.fs,  2));
    m_resQms->setText(fmt(r.Qms, 3));
    m_resQes->setText(fmt(r.Qes, 3));
    m_resQts->setText(fmt(r.Qts, 3));
    m_resMms->setText(fmt(r.mms*1000.0, 3));
    m_resCms->setText(fmt(r.Cms*1000.0, 3));
    m_resRms->setText(fmt(r.Rms, 4));
    m_resBL ->setText(fmt(r.BL,  3));
    m_resSd ->setText(fmt(r.Sd*10000.0, 2));
    m_resVas->setText(fmt(r.Vas*1000.0, 3));
    m_resLe ->setText(fmt(r.Le*1000.0,  4));

    const double pct = (r.fs>0) ? 100.0*(r.fsVerify-r.fs)/r.fs : 0.0;
    const bool ok    = std::fabs(pct) < 3.0;
    m_resVerify->setText(
        QString("<span style='color:%1'>%2 Hz  (%3%4 % deviation)</span>")
        .arg(ok?"#27ae60":"#e74c3c")
        .arg(r.fsVerify,0,'f',2)
        .arg(pct>=0?"+":"").arg(pct,0,'f',2));

    m_resultsBox->setVisible(true);
}

void QuickEntryWidget::onCalculate()
{
    m_statusLbl->clear();
    DriverRecord r = collectRecord();
    QString err;
    if (!TSCalculator::validateInputs(r, err)) {
        m_statusLbl->setStyleSheet("color:#e74c3c;");
        m_statusLbl->setText("Validation error: " + err);
        return;
    }
    TSCalculator::calculate(r);
    // All T/S parameters are derived from measurements — mark as calculated
    r.fieldOrigins = FieldOrigin::AllTS;
    m_lastRecord = r;
    showResults(r);
    m_statusLbl->setStyleSheet("color:#27ae60;");
    m_statusLbl->setText("Calculated — opening entry form for review.");
    emit recordCalculated(m_lastRecord);
}


void QuickEntryWidget::clear()
{
    m_make->clear(); m_model->clear(); m_measuredBy->clear();
    m_date->setDate(QDate::currentDate());
    m_notes->clear();
    m_vmeas->setValue(1.0); m_rs->setValue(50.0); m_rbRms->setChecked(true);
    m_Re->setValue(0);
    m_fs->setValue(0);   m_Vpeak->setValue(0);
    m_f1->setValue(0);   m_f2->setValue(0);
    m_deltaM_g->setValue(0); m_fo->setValue(0);
    m_Dd_mm->setValue(0); m_Zmin->setValue(0); m_f3->setValue(0);
    m_resultsBox->setVisible(false);
    m_statusLbl->clear();
    m_hintZ12->setVisible(false);    m_hintVtarget->setVisible(false);
    m_hintVmin->setVisible(false);   m_hintVf3->setVisible(false);
    if (m_rbSVC)    m_rbSVC->setChecked(true);
    if (m_rbSeries) m_rbSeries->setChecked(true);
    m_editId = -1;
    m_lastRecord = {};
}

void QuickEntryWidget::loadRecord(const DriverRecord &r)
{
    clear();
    m_editId = r.id;
    m_make->setText(r.make);
    m_model->setText(r.model);
    if (r.dateMeasured.isValid()) m_date->setDate(r.dateMeasured);
    m_measuredBy->setText(r.measuredBy);
    m_notes->setPlainText(r.notes);
    m_vmeas->setValue(r.V_meas > 0 ? r.V_meas : 1.0);
    m_rs->setValue(r.R_s > 0 ? r.R_s : 50.0);
    if (r.voltageMode == 1) m_rbPp->setChecked(true);
    else                    m_rbRms->setChecked(true);
    m_Re->setValue(r.Re);
    m_fs->setValue(r.fs);
    // Back-calculate Vpeak from stored Zmax
    m_Vpeak->setValue(TSCalculator::driverVoltage(r.V_meas, r.R_s, r.Zmax));
    m_f1->setValue(r.f1);     m_f2->setValue(r.f2);
    m_deltaM_g->setValue(r.deltaM*1000.0);
    m_fo->setValue(r.fo);
    m_Dd_mm->setValue(r.Dd*1000.0);
    {
        // Back-calculate voltage from stored Zmin: V = Zmin * Vmeas / (Zmin + Rs)
        const double vm = r.V_meas > 0 ? r.V_meas : m_vmeas->value();
        const double rs = r.R_s   > 0 ? r.R_s    : m_rs->value();
        const double VZmin = (r.Zmin > 0 && rs > 0) ? (r.Zmin * vm / (r.Zmin + rs)) : 0.0;
        m_Zmin->setValue(VZmin);
    }
    m_f3->setValue(r.f3);
    if (m_rbSVC && m_rbDVC) {
        if (r.isDVC) m_rbDVC->setChecked(true);
        else         m_rbSVC->setChecked(true);
    }
    if (m_rbSeries && m_rbParallel) {
        if (r.dvcWiring == 1) m_rbParallel->setChecked(true);
        else                  m_rbSeries->setChecked(true);
    }
    if (r.hasResults()) { m_lastRecord = r; showResults(r); }
    updateFreeAirHints();
    updatePhysicalHints();
}
