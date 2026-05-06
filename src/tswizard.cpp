#include "tswizard.h"
#include "tscalculator.h"
#include "noscrollspinbox.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QFont>
#include <QColor>
#include <QFrame>
#include <cmath>

// ════════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════════
static QLabel *makeInstructionLabel(const QString &html)
{
    auto *lbl = new QLabel(html);
    lbl->setWordWrap(true);
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet("QLabel { background:#f5f0f2; border-left:4px solid #6b2a40;"
                       " padding:8px 12px; border-radius:3px; color:#3a3a3a; }");
    return lbl;
}

static QDoubleSpinBox *makeSpin(double min, double max, int dec,
                                double step, const QString &suffix)
{
    auto *s = new NoScrollSpinBox;
    s->setRange(min, max);
    s->setDecimals(dec);
    s->setSingleStep(step);
    s->setSuffix(suffix);
    s->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return s;
}

static QLabel *makeResultLbl()
{
    auto *l = new QLabel("–");
    l->setStyleSheet("font-weight:bold; color:#4a1a2e; font-size:10pt;");
    return l;
}

// Styled hint label (shown below input groups)
static QLabel *makeHintLbl()
{
    auto *l = new QLabel;
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setStyleSheet(
        "QLabel { background:#f8f0f2; border-left:4px solid #8b3a50;"
        " padding:6px 10px; border-radius:3px; color:#4a1a2e; }");
    l->setVisible(false);
    return l;
}

// Voltage across driver in a series-resistor test circuit
static double driverV(double Vsrc, double Rs, double Z)
{
    return TSCalculator::driverVoltage(Vsrc, Rs, Z);
}

// ════════════════════════════════════════════════════════════════
//  ImpedanceDiagram
// ════════════════════════════════════════════════════════════════
ImpedanceDiagram::ImpedanceDiagram(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(250, 155);
    setMaximumHeight(180);
}

void ImpedanceDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF area(44, 8, width() - 54, height() - 30);
    p.fillRect(rect(), QColor("#fafafa"));
    p.fillRect(area.toRect(), Qt::white);

    QPen axisPen(QColor("#333"), 1.8);
    p.setPen(axisPen);
    p.drawLine(area.bottomLeft().toPoint(), area.topLeft().toPoint());
    p.drawLine(area.bottomLeft().toPoint(), area.bottomRight().toPoint());

    QFont small; small.setPointSize(8);
    p.setFont(small);
    p.setPen(QColor("#555"));
    p.save();
    p.translate(12, area.center().y() + 45);
    p.rotate(-90);
    p.drawText(0, 0, "Impedance (Ω)");
    p.restore();
    p.drawText(QRectF(area.left(), area.bottom()+14, area.width(), 14),
               Qt::AlignHCenter, "Frequency (Hz)");

    auto toScene = [&](double nx, double ny) -> QPointF {
        return { area.left() + nx*area.width(), area.top() + ny*area.height() };
    };

    // Impedance curve
    QPainterPath curve;
    curve.moveTo(toScene(0.00, 0.80));
    curve.cubicTo(toScene(0.08,0.82),toScene(0.14,0.84),toScene(0.20,0.83));
    curve.cubicTo(toScene(0.26,0.82),toScene(0.28,0.60),toScene(0.30,0.40));
    curve.cubicTo(toScene(0.32,0.18),toScene(0.34,0.04),toScene(0.36,0.02));
    curve.cubicTo(toScene(0.38,0.04),toScene(0.40,0.20),toScene(0.43,0.40));
    curve.cubicTo(toScene(0.46,0.62),toScene(0.50,0.90),toScene(0.55,0.93));
    curve.cubicTo(toScene(0.60,0.95),toScene(0.68,0.95),toScene(0.72,0.93));
    curve.cubicTo(toScene(0.78,0.90),toScene(0.85,0.82),toScene(1.00,0.62));
    p.setPen(QPen(QColor("#8b3a50"), 2.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(curve);

    const double yZmax=0.02, yZ12=0.40, yZmin=0.93;
    const double xF1=0.295, xFs=0.36, xF2=0.425, xF3=0.80, yF3curve=0.88;

    auto hLine = [&](double ny, QColor c, const QString &lbl) {
        p.setPen(QPen(c,1.2,Qt::DashLine));
        p.drawLine(toScene(0,ny).toPoint(), toScene(1.0,ny).toPoint());
        p.setPen(c);
        QFont f; f.setPointSize(8); p.setFont(f);
        p.drawText(QRectF(2, toScene(0,ny).y()-8, 40, 16), Qt::AlignRight|Qt::AlignVCenter, lbl);
    };
    auto vLine = [&](double nx, double ny_top, QColor c, const QString &lbl) {
        p.setPen(QPen(c,1.0,Qt::DashLine));
        p.drawLine(toScene(nx,ny_top).toPoint(), toScene(nx,1.0).toPoint());
        p.setPen(c);
        QFont f; f.setPointSize(8); p.setFont(f);
        p.drawText(QRectF(toScene(nx,0).x()-10, toScene(0,1.0).y()+2, 22, 14),
                   Qt::AlignHCenter, lbl);
    };

    hLine(yZmax, QColor("#c0392b"), "Zmax");
    hLine(yZ12,  QColor("#e67e22"), "Z₁=Z₂");  // corrected label
    hLine(yZmin, QColor("#27ae60"), "Zmin");

    p.setPen(QPen(QColor("#16a085"),1.0,Qt::DotLine));
    p.drawLine(toScene(xF3,yF3curve).toPoint(), toScene(1.0,yF3curve).toPoint());
    p.setPen(QColor("#16a085"));
    QFont sf; sf.setPointSize(7); p.setFont(sf);
    p.drawText(QRectF(toScene(xF3+0.01,yF3curve).x(), toScene(0,yF3curve).y()-10, 50,14),
               Qt::AlignLeft, "+3 dB");

    vLine(xF1, yZ12, QColor("#e67e22"), "f₁");
    vLine(xFs, yZmax, QColor("#c0392b"), "fₛ");
    vLine(xF2, yZ12, QColor("#e67e22"), "f₂");
    vLine(xF3, yF3curve, QColor("#16a085"), "f₃");

    auto dot = [&](double nx, double ny, QColor c) {
        p.setPen(Qt::NoPen); p.setBrush(c);
        p.drawEllipse(toScene(nx,ny),4.0,4.0); p.setBrush(Qt::NoBrush);
    };
    dot(xFs,yZmax,QColor("#c0392b"));
    dot(xF1,yZ12,QColor("#e67e22"));
    dot(xF2,yZ12,QColor("#e67e22"));
    dot(xF3,yF3curve,QColor("#16a085"));
}

// ════════════════════════════════════════════════════════════════
//  Page 1 – Introduction
// ════════════════════════════════════════════════════════════════
IntroPage::IntroPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("TSBoss – Thiele/Small Measurement Wizard");
    setSubTitle("SB Acoustics delta-mass method  |  Series-resistor constant-voltage technique");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    layout->addWidget(makeInstructionLabel(
        "<b>Method overview</b><br>"
        "This wizard uses the SB Acoustics delta-mass procedure: impedance is measured "
        "with a <b>series resistor</b> (typically 50 Ω) between the amplifier and the "
        "driver. You will need a frequency-swept signal source, a voltmeter or oscilloscope, "
        "and a small quantity of plasticine for the added-mass step."));

    layout->addWidget(makeInstructionLabel(
        "<b>&#9888;  Break-in the driver first</b><br>"
        "Drive near <em>80 % of expected fₛ</em> with a sine wave. Slowly increase until "
        "the suspension reaches maximum displacement. Run for <b>~10 minutes</b>, "
        "then allow to cool to ambient temperature."));

    layout->addWidget(makeInstructionLabel(
        "<b>Mounting</b><br>"
        "Hold the driver <b>firmly in free air, vertically</b>. "
        "Do not seal pole vents, do not rest it on a surface, "
        "and keep it away from walls."));

    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════
//  Page 2 – Driver Identity
// ════════════════════════════════════════════════════════════════
IdentityPage::IdentityPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Driver Identification");
    setSubTitle("Enter the driver's identity information.");

    m_make       = new QLineEdit;
    m_model      = new QLineEdit;
    m_measuredBy = new QLineEdit;
    m_date       = new QDateEdit(QDate::currentDate());
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat("yyyy-MM-dd");

    registerField("make*",        m_make);
    registerField("model*",       m_model);
    registerField("measuredBy",   m_measuredBy);
    registerField("dateMeasured", m_date, "date", SIGNAL(dateChanged(QDate)));

    auto *form = new QFormLayout(this);
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(10);
    form->addRow("Manufacturer / Make:", m_make);
    form->addRow("Model:",               m_model);
    form->addRow("Date measured:",       m_date);
    form->addRow("Measured by:",         m_measuredBy);
}

// ════════════════════════════════════════════════════════════════
//  Page 3 – DC Resistance
// ════════════════════════════════════════════════════════════════
DCResistancePage::DCResistancePage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 1 – Voice Coil DC Resistance  (Rₑ)");
    setSubTitle("Measure Rₑ with a 4-wire ohmmeter or quality multimeter before the sweep.");

    m_Re = makeSpin(0.10, 200.0, 3, 0.01, " Ω");
    registerField("Re", m_Re, "value", SIGNAL(valueChanged(double)));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(makeInstructionLabel(
        "Use a <b>low resistance range (0–20 Ω)</b>, accuracy better than ±0.1 Ω.<br>"
        "If using a 2-wire multimeter, do <em>not</em> use the supplied leads "
        "– their resistance is often too high. Apply offset correction.<br>"
        "<b>Temperature:</b> a copper voice coil at 20 °C reads ~0.9974× its 25 °C value."));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow("Rₑ  (DC resistance):", m_Re);
    layout->addLayout(form);
    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════
//  Page 4 – Measurement Setup  (NEW)
// ════════════════════════════════════════════════════════════════
MeasurementSetupPage::MeasurementSetupPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 2 – Measurement Setup");
    setSubTitle("Set your test voltage and circuit — do not change these after measurements begin.");

    m_vmeas = makeSpin(0.01, 50.0, 3, 0.1, " V");
    m_vmeas->setValue(1.0);
    m_rs    = makeSpin(1.0, 1000.0, 1, 1.0, " Ω");
    m_rs->setValue(50.0);

    m_rbRms = new QRadioButton("RMS voltage  (e.g. true-RMS multimeter, RMS-capable oscilloscope)");
    m_rbPp  = new QRadioButton("Peak-to-peak  (e.g. oscilloscope in Vpp mode)");
    m_rbRms->setChecked(true);

    auto *bg = new QButtonGroup(this);
    bg->addButton(m_rbRms, 0);
    bg->addButton(m_rbPp,  1);

    // Register voltage mode as a wizard field via a hidden spinbox trick
    // We use an int spinbox hidden, driven by the radio buttons
    auto *modeProxy = new QDoubleSpinBox; // hidden proxy 0=rms,1=pp
    modeProxy->setRange(0,1); modeProxy->setValue(0);
    modeProxy->hide();
    connect(m_rbPp,  &QRadioButton::toggled, this, [modeProxy](bool on){
        modeProxy->setValue(on ? 1.0 : 0.0);
    });

    registerField("vmeas",       m_vmeas,    "value", SIGNAL(valueChanged(double)));
    registerField("rs",          m_rs,       "value", SIGNAL(valueChanged(double)));
    registerField("voltageMode", modeProxy,  "value", SIGNAL(valueChanged(double)));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    layout->addWidget(makeInstructionLabel(
        "<b>Series resistor (R_s)</b><br>"
        "Place a resistor in series between your amplifier and the driver. "
        "SB Acoustics uses <b>150 Ω</b>. This value is used to calculate the "
        "target voltage at your driver terminals for identifying f₁, f₂, and f₃."));

    layout->addWidget(makeInstructionLabel(
        "<b>Measurement voltage</b><br>"
        "Set your signal generator/amplifier to a fixed output voltage. "
        "For a mid-woofer aim for ~1 V rms <em>across the driver</em> at resonance. "
        "<b>Do not adjust this voltage between any of the measurement steps.</b>"));

    auto *box  = new QGroupBox("Test circuit configuration");
    auto *form = new QFormLayout(box);
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);
    form->addRow("Series resistor R_s:", m_rs);
    form->addRow("Applied voltage V:", m_vmeas);

    auto *vbox = new QVBoxLayout;
    vbox->setSpacing(4);
    vbox->addWidget(new QLabel("<b>Voltage format used on your instrument:</b>"));
    vbox->addWidget(m_rbRms);
    vbox->addWidget(m_rbPp);
    form->addRow(vbox);
    form->addRow(modeProxy);   // hidden but in layout

    layout->addWidget(box);
    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════
//  Page 5 – Free-Air Impedance
// ════════════════════════════════════════════════════════════════
FreeAirPage::FreeAirPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 3 – Free-Air Impedance Curve");
    setSubTitle("Run your sweep and read fₛ, Zmax, then locate f₁ and f₂ using the target voltage.");

    m_fs     = makeSpin(1.0,   2000.0, 2, 0.1,   " Hz");
    m_Vpeak  = makeSpin(0.001, 50.0,  4, 0.001,  " V");
    m_f1     = makeSpin(1.0,   2000.0, 2, 0.1,   " Hz");
    m_f2     = makeSpin(1.0,   2000.0, 2, 0.1,   " Hz");

    registerField("fs",    m_fs,    "value", SIGNAL(valueChanged(double)));
    registerField("Vpeak", m_Vpeak, "value", SIGNAL(valueChanged(double)));
    registerField("f1",    m_f1,    "value", SIGNAL(valueChanged(double)));
    registerField("f2",    m_f2,    "value", SIGNAL(valueChanged(double)));

    m_lblZ12     = makeResultLbl();
    m_lblVerify  = makeResultLbl();
    m_lblVtarget = makeHintLbl();

    connect(m_fs,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FreeAirPage::updateLiveCalc);
    connect(m_Vpeak, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FreeAirPage::updateLiveCalc);
    connect(m_f1,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FreeAirPage::updateLiveCalc);
    connect(m_f2,    QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &FreeAirPage::updateLiveCalc);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(6);

    layout->addWidget(makeInstructionLabel(
        "≥ 1/24 oct. resolution. Keep the same voltage you configured in Step 2. "
        "Sweep the full range, identify the resonance peak, then read fₛ and Zmax. "
        "Z₁ and Z₂ are the <em>two frequencies on either side of the peak</em> "
        "where impedance = √(Rₑ · Zmax) — use the target voltage shown below."));

    // Side-by-side: diagram left, form right
    auto *hbox = new QHBoxLayout;
    hbox->setSpacing(14);

    auto *diag = new ImpedanceDiagram;
    diag->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    hbox->addWidget(diag, 3);

    auto *formWidget = new QWidget;
    auto *form = new QFormLayout(formWidget);
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(6);
    form->setContentsMargins(0,0,0,0);
    form->addRow("fₛ  – resonance:", m_fs);
    form->addRow("V at peak (driver voltage at fₛ):", m_Vpeak);

    auto *calcBox = new QGroupBox("Live calculation");
    calcBox->setStyleSheet(
        "QGroupBox{font-weight:bold;font-size:9pt;border:1px solid #ccd;"
        "border-radius:4px;margin-top:14px;padding:8px 6px 4px 6px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;"
        "color:#4a1a2e;}");
    auto *calcForm = new QFormLayout(calcBox);
    calcForm->setSpacing(3);
    calcForm->setContentsMargins(4,2,4,2);
    calcForm->addRow("Z₁ = Z₂ = √(Rₑ·Zmax):", m_lblZ12);
    calcForm->addRow("√(f₁·f₂) verify:",        m_lblVerify);
    form->addRow(calcBox);

    form->addRow("f₁  – lower side:", m_f1);
    form->addRow("f₂  – upper side:", m_f2);
    hbox->addWidget(formWidget, 2);

    layout->addLayout(hbox);
    layout->addWidget(m_lblVtarget);
    layout->addStretch();
}

void FreeAirPage::updateLiveCalc()
{
    const double Re    = field("Re").toDouble();
    const double Vpeak = m_Vpeak->value();
    const double f1    = m_f1->value();
    const double f2    = m_f2->value();
    const double fs    = m_fs->value();
    const double Vmeas = field("vmeas").toDouble();
    const double Rs    = field("rs").toDouble();
    const QString vu   = (field("voltageMode").toInt() == 1) ? "V pp" : "V rms";

    const double denom = Vmeas - Vpeak;
    if (Re > 0 && Vpeak > 0 && denom > 0 && Rs > 0) {
        const double Zmax = Rs * Vpeak / denom;
        const double Z12  = std::sqrt(Re * Zmax);
        m_lblZ12->setText(
            QString("Zmax = <b>%1 Ω</b>  →  Z₁=Z₂ = <b>%2 Ω</b>")
            .arg(Zmax, 0, 'f', 2).arg(Z12, 0, 'f', 3));

        const double V12 = driverV(Vmeas, Rs, Z12);
        m_lblVtarget->setText(
            QString("&#128269; <b>Target voltage to find f₁ and f₂:</b>  "
                    "<span style='font-size:12pt;color:#c0392b;'>%1 %2</span>"
                    "<br>Sweep either side of the resonance peak — mark the two "
                    "frequencies where your voltmeter reads this value. "
                    "These are f₁ (below fₛ) and f₂ (above fₛ).")
            .arg(V12, 0, 'f', 4).arg(vu));
        m_lblVtarget->setVisible(true);
    } else {
        m_lblZ12->setText("–");
        m_lblVtarget->setVisible(false);
    }

    if (f1 > 0 && f2 > 0) {
        const double verify = std::sqrt(f1 * f2);
        const double pct    = (fs > 0) ? 100.0*(verify-fs)/fs : 0.0;
        const QString color = (std::fabs(pct) < 5.0) ? "#27ae60" : "#e67e22";
        m_lblVerify->setText(
            QString("<span style='color:%1'>%2 Hz  (%3%4 %)</span>")
            .arg(color).arg(verify,0,'f',2)
            .arg(pct>=0?"+":"").arg(pct,0,'f',1));
    }
}

bool FreeAirPage::validatePage()
{
    const double Re    = field("Re").toDouble();
    const double fs    = m_fs->value();
    const double Vpeak = m_Vpeak->value();
    const double Vmeas = field("vmeas").toDouble();
    const double Rs    = field("rs").toDouble();
    const double f1    = m_f1->value();
    const double f2    = m_f2->value();

    const double denom = Vmeas - Vpeak;
    if (Vpeak <= 0 || denom <= 0) {
        QMessageBox::warning(this,"Input error",
            "V at peak must be greater than 0 and less than the applied voltage V.");
        return false;
    }
    const double Zmax = Rs * Vpeak / denom;
    if (Zmax <= Re) {
        QMessageBox::warning(this,"Input error",
            "Calculated Zmax (" + QString::number(Zmax,'f',2) +
            " Ω) must be greater than Rₑ (" + QString::number(Re,'f',3) + " Ω).\n"
            "Check your peak voltage entry.");
        return false;
    }
    if (f1 <= 0 || f2 <= 0 || f1 >= f2) {
        QMessageBox::warning(this,"Input error","f₁ must be less than f₂, both > 0.");
        return false;
    }
    if (f1 >= fs || f2 <= fs) {
        QMessageBox::warning(this,"Input error","fₛ must lie between f₁ and f₂.");
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════
//  Page 6 – Added Mass
// ════════════════════════════════════════════════════════════════
AddedMassPage::AddedMassPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 4 – Added Mass  (Δm)");
    setSubTitle("Attach mass to the cone, re-sweep at the same voltage, find the new resonance.");

    m_deltaM_g = makeSpin(0.1, 500.0, 2, 0.5, " g");
    m_fo       = makeSpin(1.0, 2000.0, 2, 0.1, " Hz");

    registerField("deltaM_g", m_deltaM_g, "value", SIGNAL(valueChanged(double)));
    registerField("fo",       m_fo,       "value", SIGNAL(valueChanged(double)));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    layout->addWidget(makeInstructionLabel(
        "Weigh an accurate amount of plasticine (≈ 70 % of expected mms).<br>"
        "• Attach to the <b>centre of the cone / dust cap</b> — not the surround.<br>"
        "• Do <em>not</em> use magnets — they disturb the motor.<br>"
        "• Ensure the mass is fully stuck; no part must vibrate freely.<br>"
        "• <b>Keep the measurement voltage identical to Step 2.</b><br>"
        "Find the new, <em>lower</em> resonance peak (f₀ &lt; fₛ)."));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);
    form->addRow("Δm  – added mass:",      m_deltaM_g);
    form->addRow("f₀  – shifted resonance:", m_fo);
    layout->addLayout(form);
    layout->addStretch();
}

bool AddedMassPage::validatePage()
{
    if (m_fo->value() >= field("fs").toDouble()) {
        QMessageBox::warning(this,"Input error",
            "f₀ must be lower than fₛ — the added mass shifts resonance downward.");
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════
//  Page 7 – Physical Dimensions
// ════════════════════════════════════════════════════════════════
PhysicalPage::PhysicalPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 5 – Physical Dimensions & f₃");
    setSubTitle("Measure the piston diameter and enter the voltage at Zmin from the original sweep.");

    m_Dd_mm = makeSpin(10.0,  800.0, 1, 0.5,     " mm");
    m_Zmin  = makeSpin(0.0001, 50.0, 4, 0.001,   " V rms");
    m_f3    = makeSpin(100.0, 100000.0, 0, 100.0, " Hz");

    registerField("Dd_mm", m_Dd_mm, "value", SIGNAL(valueChanged(double)));
    registerField("Zmin",  m_Zmin,  "value", SIGNAL(valueChanged(double)));
    registerField("f3",    m_f3,    "value", SIGNAL(valueChanged(double)));

    m_lblVmin = makeHintLbl();
    m_lblVf3  = makeHintLbl();

    connect(m_Zmin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PhysicalPage::updateVoltageHints);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    layout->addWidget(makeInstructionLabel(
        "<b>Piston diameter Dᵈ</b> — measure from mid-surround to mid-surround."));

    layout->addWidget(makeInstructionLabel(
        "<b>Zmin voltage and f₃</b> — from the <em>original</em> (no added mass) sweep:<br>"
        "• <b>V at Zmin</b> — enter the voltage your meter shows at the impedance minimum "
        "<em>above</em> the resonance peak (the lowest point on the high-frequency side). "
        "Zmin is computed automatically.<br>"
        "• <b>f₃</b> — the frequency where impedance has risen 3 dB above Zmin "
        "(Z_f₃ = √2 · Zmin). The target voltage is shown below as soon as you enter V at Zmin."));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);
    form->addRow("Dᵈ  – piston diameter:", m_Dd_mm);
    form->addRow("V at Zmin – voltage at impedance min:", m_Zmin);
    layout->addLayout(form);

    layout->addWidget(m_lblVmin);
    layout->addWidget(m_lblVf3);

    auto *form2 = new QFormLayout;
    form2->setLabelAlignment(Qt::AlignRight);
    form2->setSpacing(8);
    form2->addRow("f₃  – freq. at Zmin + 3 dB:", m_f3);
    layout->addLayout(form2);

    layout->addStretch();
}

void PhysicalPage::initializePage()
{
    updateVoltageHints();
}

void PhysicalPage::updateVoltageHints()
{
    // m_Zmin now holds the voltage at the impedance minimum
    const double VZmin = m_Zmin->value();
    const double Vmeas = field("vmeas").toDouble();
    const double Rs    = field("rs").toDouble();
    const QString vu   = (field("voltageMode").toInt() == 1) ? "V pp" : "V rms";

    // Keep suffix in sync with voltage mode
    m_Zmin->setSuffix(" " + vu);

    const double denom = Vmeas - VZmin;
    if (VZmin > 0 && denom > 0 && Vmeas > 0 && Rs > 0) {
        const double Zmin = Rs * VZmin / denom;
        const double Zf3  = std::sqrt(2.0) * Zmin;
        const double Vf3  = driverV(Vmeas, Rs, Zf3);

        m_lblVmin->setText(
            QString("Computed Zmin = Rₛ · V / (V_applied − V) = <b>%1 Ω</b>")
            .arg(Zmin, 0, 'f', 3));
        m_lblVmin->setVisible(true);

        m_lblVf3->setText(
            QString("&#128269; <b>Target voltage for f₃ (3 dB above Zmin):</b>  "
                    "<span style='font-size:12pt;color:#c0392b;'>%1 %2</span>"
                    "<br>Z_f₃ = √2 × %3 = %4 Ω.  "
                    "Scan above fₛ — mark where your voltmeter first reaches this value.")
            .arg(Vf3,0,'f',4).arg(vu).arg(Zmin,0,'f',3).arg(Zf3,0,'f',3));
        m_lblVf3->setVisible(true);
    } else {
        m_lblVmin->setVisible(false);
        m_lblVf3->setVisible(false);
    }
}

// ════════════════════════════════════════════════════════════════
//  Helper: result row in grid
// ════════════════════════════════════════════════════════════════
static void addResultRow(QGridLayout *g, int &row,
                         const QString &symbol, const QString &name,
                         const QString &unit, QLabel *&out)
{
    auto *sym = new QLabel("<b>" + symbol + "</b>"); sym->setMinimumWidth(40);
    auto *nm  = new QLabel(name); nm->setStyleSheet("color:#555;");
    out = new QLabel("–");
    out->setStyleSheet("font-weight:bold; color:#3a3a3a; font-size:10pt;");
    auto *un  = new QLabel(unit); un->setStyleSheet("color:#888;");
    g->addWidget(sym, row, 0);
    g->addWidget(nm,  row, 1);
    g->addWidget(out, row, 2);
    g->addWidget(un,  row, 3);
    ++row;
}

// ════════════════════════════════════════════════════════════════
//  Page 8 – Results
// ════════════════════════════════════════════════════════════════
ResultsPage::ResultsPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle("Thiele/Small Parameters – Results");
    setSubTitle("Review the calculated parameters, add notes, then click Finish to save.");
    setFinalPage(true);

    m_notes = new QTextEdit;
    m_notes->setPlaceholderText("Measurement notes, setup details, condition of driver…");
    m_notes->setFixedHeight(64);

    auto *resBox = new QGroupBox("Calculated Parameters");
    resBox->setStyleSheet(
        "QGroupBox{font-weight:bold;border:1px solid #ddd;border-radius:5px;"
        "margin-top:16px;padding:12px 6px 6px 6px;background:white;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 6px;color:#4a1a2e;}");
    auto *grid = new QGridLayout(resBox);
    grid->setColumnStretch(1,2); grid->setColumnStretch(2,1); grid->setColumnStretch(3,1);
    grid->setVerticalSpacing(2); grid->setHorizontalSpacing(8);

    auto hdr = [&](int col, const QString &t){
        auto *l = new QLabel("<b>"+t+"</b>"); l->setStyleSheet("color:#888;font-size:8pt;");
        grid->addWidget(l,0,col);
    };
    hdr(0,"Sym"); hdr(1,"Parameter"); hdr(2,"Value"); hdr(3,"Unit");
    int row=1;

    addResultRow(grid,row,"Rₑ","DC resistance","Ω",m_lblRe);
    addResultRow(grid,row,"fₛ","Free-air resonance","Hz",m_lblFs);
    addResultRow(grid,row,"Qms","Mechanical Q","–",m_lblQms);
    addResultRow(grid,row,"Qes","Electrical Q","–",m_lblQes);
    addResultRow(grid,row,"Qts","Total Q","–",m_lblQts);
    addResultRow(grid,row,"mms","Moving mass (incl. air)","g",m_lblMms);
    addResultRow(grid,row,"Cms","Compliance","mm/N",m_lblCms);
    addResultRow(grid,row,"Rms","Mech. resistance","kg/s",m_lblRms);
    addResultRow(grid,row,"BL","Force factor","Tm",m_lblBL);
    addResultRow(grid,row,"Sᵈ","Piston area","cm²",m_lblSd);
    addResultRow(grid,row,"Vas","Equivalent volume","L",m_lblVas);
    addResultRow(grid,row,"Lₑ","Voice-coil inductance","mH",m_lblLe);

    m_lblVerify = new QLabel; m_lblVerify->setTextFormat(Qt::RichText);
    grid->addWidget(new QLabel("<span style='color:#888;font-size:8pt;'><b>√(f₁·f₂)</b></span>"),row,0);
    grid->addWidget(new QLabel("Resonance verify"),row,1);
    grid->addWidget(m_lblVerify,row,2,1,2);

    auto *notesBox = new QGroupBox("Notes");
    notesBox->setStyleSheet(
        "QGroupBox{font-weight:bold;border:1px solid #ddd;border-radius:5px;"
        "margin-top:16px;padding:12px 6px 6px 6px;background:white;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 6px;color:#4a1a2e;}");
    auto *nb = new QVBoxLayout(notesBox);
    nb->setContentsMargins(4,2,4,4);
    nb->addWidget(m_notes);

    auto *scrollContent = new QWidget;
    auto *vb = new QVBoxLayout(scrollContent);
    vb->setContentsMargins(0,0,4,4); vb->setSpacing(6);
    vb->addWidget(resBox);
    vb->addWidget(notesBox);
    vb->addStretch();

    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setWidget(scrollContent);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *main = new QVBoxLayout(this);
    main->setContentsMargins(0,0,0,0); main->setSpacing(0);
    main->addWidget(scroll);
}

void ResultsPage::populateLabels(const DriverRecord &r)
{
    auto fmt = [](double v, int d){ return QString::number(v,'f',d); };
    m_lblRe ->setText(fmt(r.Re,  3));
    m_lblFs ->setText(fmt(r.fs,  2));
    m_lblQms->setText(fmt(r.Qms, 3));
    m_lblQes->setText(fmt(r.Qes, 3));
    m_lblQts->setText(fmt(r.Qts, 3));
    m_lblMms->setText(fmt(r.mms*1000.0, 3));
    m_lblCms->setText(fmt(r.Cms*1000.0, 3));
    m_lblRms->setText(fmt(r.Rms, 4));
    m_lblBL ->setText(fmt(r.BL,  3));
    m_lblSd ->setText(fmt(r.Sd*10000.0, 2));
    m_lblVas->setText(fmt(r.Vas*1000.0, 3));
    m_lblLe ->setText(fmt(r.Le*1000.0,  4));

    const double pct = (r.fs>0) ? 100.0*(r.fsVerify-r.fs)/r.fs : 0.0;
    const QString col = (std::fabs(pct)<3.0) ? "#27ae60" : "#e74c3c";
    m_lblVerify->setText(
        QString("<span style='color:%1'>%2 Hz  (%3%4 % deviation)</span>")
        .arg(col).arg(r.fsVerify,0,'f',2).arg(pct>=0?"+":"").arg(pct,0,'f',2));
}

void ResultsPage::initializePage()
{
    m_record = DriverRecord{};
    m_record.make         = field("make").toString();
    m_record.model        = field("model").toString();
    m_record.dateMeasured = field("dateMeasured").toDate();
    m_record.measuredBy   = field("measuredBy").toString();
    m_record.V_meas       = field("vmeas").toDouble();
    m_record.R_s          = field("rs").toDouble();
    m_record.voltageMode  = static_cast<int>(field("voltageMode").toDouble());
    m_record.Re           = field("Re").toDouble();
    m_record.fs           = field("fs").toDouble();
    {
        const double vp  = field("Vpeak").toDouble();
        const double d   = m_record.V_meas - vp;
        m_record.Zmax    = (d > 0.0) ? (m_record.R_s * vp / d) : 0.0;
    }
    m_record.f1           = field("f1").toDouble();
    m_record.f2           = field("f2").toDouble();
    m_record.deltaM       = field("deltaM_g").toDouble() / 1000.0;
    m_record.fo           = field("fo").toDouble();
    m_record.Dd           = field("Dd_mm").toDouble() / 1000.0;
    {
        const double VZmin = field("Zmin").toDouble();   // field stores voltage
        const double d     = m_record.V_meas - VZmin;
        m_record.Zmin = (VZmin > 0.0 && d > 0.0) ? (m_record.R_s * VZmin / d) : 0.0;
    }
    m_record.f3           = field("f3").toDouble();

    TSCalculator::calculate(m_record);
    populateLabels(m_record);
}

bool ResultsPage::validatePage()
{
    m_record.notes = m_notes->toPlainText();
    QString err;
    if (!TSCalculator::validateInputs(m_record, err)) {
        QMessageBox::critical(this, "Validation error", err);
        return false;
    }
    return true;
}

// ════════════════════════════════════════════════════════════════
//  TSWizard
// ════════════════════════════════════════════════════════════════
TSWizard::TSWizard(QWidget *parent) : QWizard(parent)
{
    setWindowTitle("TSBoss – T/S Parameter Wizard");
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(720, 620);

    addPage(new IntroPage);
    addPage(new IdentityPage);
    addPage(new DCResistancePage);
    addPage(new MeasurementSetupPage);
    addPage(new FreeAirPage);
    addPage(new AddedMassPage);
    addPage(new PhysicalPage);
    m_resultsPage = new ResultsPage;
    addPage(m_resultsPage);

    setButtonText(QWizard::FinishButton, "Continue to Entry");

    connect(this, &QWizard::accepted, this, [this]() {
        emit recordReady(m_resultsPage->record());
    });
}
