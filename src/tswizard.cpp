#include "tswizard.h"
#include "theme.h"
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
#include <QSettings>
#include <cmath>

// ════════════════════════════════════════════════════════════════
//  Helpers
// ════════════════════════════════════════════════════════════════
static QLabel *makeInstructionLabel(const QString &html)
{
    auto *lbl = new QLabel(html);
    lbl->setWordWrap(true);
    lbl->setTextFormat(Qt::RichText);
    lbl->setStyleSheet(themed("QLabel { background:%panel%; border-left:4px solid %accent%;"
                       " padding:8px 12px; border-radius:3px; color:%text2%; }"));
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
    l->setStyleSheet(themed("font-weight:bold; color:%accent%; font-size:10pt;"));
    return l;
}

// Styled hint label (shown below input groups)
static QLabel *makeHintLbl()
{
    auto *l = new QLabel;
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setStyleSheet(themed(
        "QLabel { background:%panel%; border-left:4px solid %accentLt%;"
        " padding:6px 10px; border-radius:3px; color:%accent%; }"));
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

    // Bottom margin must hold the 14 px axis caption drawn at bottom+12.
    const QRectF area(44, 8, width() - 54, height() - 36);
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
    p.drawText(QRectF(area.left(), area.bottom()+12, area.width(), 14),
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
//  SetupDiagram – sine source → series resistor → driver, meter across
// ════════════════════════════════════════════════════════════════
SetupDiagram::SetupDiagram(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 120);
    setMaximumHeight(150);
}

void SetupDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#fafafa"));

    const double w = width(), h = height();
    const double yT = h*0.30, yB = h*0.74;                       // top / bottom rails
    const double xGen = w*0.10, xR0 = w*0.26, xR1 = w*0.42,
                 xDrv = w*0.66, xMet = w*0.88, xEnd = w*0.92;

    QPen wire(QColor("#333"), 1.8);
    p.setPen(wire);

    // sine source: circle with a ~ inside
    const double gr = qMin(h*0.20, 20.0);
    const QPointF gc(xGen, (yT+yB)/2.0);
    p.drawEllipse(gc, gr, gr);
    QPainterPath sine;
    sine.moveTo(gc.x()-gr*0.55, gc.y());
    sine.cubicTo(gc.x()-gr*0.25, gc.y()-gr*0.95,
                 gc.x()+gr*0.25, gc.y()+gr*0.95,
                 gc.x()+gr*0.55, gc.y());
    p.drawPath(sine);
    p.drawLine(QPointF(xGen, gc.y()-gr), QPointF(xGen, yT));
    p.drawLine(QPointF(xGen, gc.y()+gr), QPointF(xGen, yB));

    // rails (top rail broken by the resistor box)
    p.drawLine(QPointF(xGen, yT), QPointF(xR0, yT));
    p.drawLine(QPointF(xR1, yT), QPointF(xEnd, yT));
    p.drawLine(QPointF(xGen, yB), QPointF(xEnd, yB));
    p.drawLine(QPointF(xEnd, yT), QPointF(xEnd, yB));            // far-end tie (meter node)

    // series resistor
    p.setBrush(Qt::white);
    p.drawRect(QRectF(xR0, yT-7, xR1-xR0, 14));
    p.setBrush(Qt::NoBrush);

    // driver bridging the rails: voice-coil box + cone lines
    const double dw = w*0.028;
    p.setBrush(Qt::white);
    p.drawRect(QRectF(xDrv-dw, yT, dw*2, yB-yT));
    p.setBrush(Qt::NoBrush);
    p.drawLine(QPointF(xDrv+dw, yT+(yB-yT)*0.20),
               QPointF(xDrv+dw+w*0.045, yT-h*0.05));
    p.drawLine(QPointF(xDrv+dw, yB-(yB-yT)*0.20),
               QPointF(xDrv+dw+w*0.045, yB+h*0.05));

    // voltmeter across the driver (same nodes, far right)
    const double mr = qMin(h*0.15, 15.0);
    const QPointF mc(xMet, (yT+yB)/2.0);
    p.drawLine(QPointF(xMet, yT), QPointF(xMet, mc.y()-mr));
    p.drawLine(QPointF(xMet, mc.y()+mr), QPointF(xMet, yB));
    p.setPen(QPen(QColor("#8b3a50"), 2.0));
    p.setBrush(Qt::white);
    p.drawEllipse(mc, mr, mr);
    p.setBrush(Qt::NoBrush);

    // junction dots where driver and meter tap the rails
    auto dot = [&](double x, double y) {
        p.setPen(Qt::NoPen); p.setBrush(QColor("#333"));
        p.drawEllipse(QPointF(x, y), 2.6, 2.6);
        p.setBrush(Qt::NoBrush);
    };
    dot(xDrv, yT); dot(xDrv, yB);
    dot(xMet, yT); dot(xMet, yB);

    // labels
    QFont f; f.setPointSize(8);
    p.setFont(f);
    p.setPen(QColor("#555"));
    p.drawText(QRectF(xGen-52, yB+6, 104, 14), Qt::AlignHCenter, "sine source");
    p.drawText(QRectF(xR0-24, yT-24, (xR1-xR0)+48, 14), Qt::AlignHCenter, "Rₛ  50–100 Ω");
    p.drawText(QRectF(xDrv-40, yB+6, 80, 14), Qt::AlignHCenter, "driver");
    p.drawText(QRectF(xMet-58, yB+6, 116, 14), Qt::AlignHCenter, "true-RMS meter");
    QFont fb = f; fb.setBold(true); fb.setPointSize(9);
    p.setFont(fb);
    p.setPen(QColor("#8b3a50"));
    p.drawText(QRectF(mc.x()-mr, mc.y()-mr, mr*2, mr*2), Qt::AlignCenter, "V");
}

// ════════════════════════════════════════════════════════════════
//  PistonDiagram – cross-section, Dd measured mid-surround → mid-surround
// ════════════════════════════════════════════════════════════════
PistonDiagram::PistonDiagram(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 150);
    setMaximumHeight(190);
}

void PistonDiagram::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#fafafa"));

    const double w = width(), h = height();
    const double cx = w*0.5;
    const double R    = w*0.30;                // cone rim half-width
    const double sw   = w*0.055;               // surround width
    const double yRim = h*0.42, yApex = h*0.76, yDim = h*0.16;

    QPen ink(QColor("#333"), 1.8);

    // mounting flange either side of the surround
    p.setPen(ink);
    p.drawLine(QPointF(cx-R-sw-w*0.05, yRim), QPointF(cx-R-sw, yRim));
    p.drawLine(QPointF(cx+R+sw, yRim), QPointF(cx+R+sw+w*0.05, yRim));

    // surround: half-round bumps
    p.setPen(QPen(QColor("#8b3a50"), 2.2));
    p.drawArc(QRectF(cx-R-sw, yRim-sw*0.55, sw, sw*1.1), 0, 180*16);
    p.drawArc(QRectF(cx+R,    yRim-sw*0.55, sw, sw*1.1), 0, 180*16);

    // cone + dust cap
    p.setPen(ink);
    const double rvc = w*0.055;
    p.drawLine(QPointF(cx-R, yRim), QPointF(cx-rvc, yApex));
    p.drawLine(QPointF(cx+R, yRim), QPointF(cx+rvc, yApex));
    p.drawArc(QRectF(cx-rvc*1.4, yApex-rvc*1.1, rvc*2.8, rvc*2.0), 0, 180*16);

    // basket hint
    p.setPen(QPen(QColor("#999"), 1.2));
    p.drawLine(QPointF(cx-R-sw-w*0.05, yRim), QPointF(cx-rvc*1.8, h*0.86));
    p.drawLine(QPointF(cx+R+sw+w*0.05, yRim), QPointF(cx+rvc*1.8, h*0.86));

    // dimension line: middle of one surround to the middle of the other
    const double xL = cx-R-sw*0.5, xRt = cx+R+sw*0.5;
    p.setPen(QPen(QColor("#c0392b"), 1.0, Qt::DashLine));
    p.drawLine(QPointF(xL,  yDim), QPointF(xL,  yRim-sw*0.65));
    p.drawLine(QPointF(xRt, yDim), QPointF(xRt, yRim-sw*0.65));
    p.setPen(QPen(QColor("#c0392b"), 1.4));
    p.drawLine(QPointF(xL, yDim), QPointF(xRt, yDim));
    auto arrow = [&](QPointF tip, int dir) {
        QPainterPath a;
        a.moveTo(tip);
        a.lineTo(tip.x()+dir*7, tip.y()-3.5);
        a.lineTo(tip.x()+dir*7, tip.y()+3.5);
        a.closeSubpath();
        p.fillPath(a, QColor("#c0392b"));
    };
    arrow(QPointF(xL,  yDim), +1);
    arrow(QPointF(xRt, yDim), -1);

    QFont fb; fb.setPointSize(9); fb.setBold(true);
    p.setFont(fb);
    p.setPen(QColor("#c0392b"));
    p.drawText(QRectF(cx-60, yDim-18, 120, 14), Qt::AlignHCenter, "Dᵈ");

    QFont f; f.setPointSize(8);
    p.setFont(f);
    p.setPen(QColor("#555"));
    p.drawText(QRectF(0, h*0.90, w, 14), Qt::AlignHCenter,
               "middle of the surround on one side  →  middle of the surround on the other");
}

// ════════════════════════════════════════════════════════════════
//  Page 1 – Introduction
// ════════════════════════════════════════════════════════════════
IntroPage::IntroPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("TSBoss – Thiele/Small Measurement Wizard");
    setSubTitle("Delta-mass method  |  Series-resistor technique");

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    layout->addWidget(makeInstructionLabel(
        "<b>Method overview</b><br>"
        "Impedance is measured with a <b>series resistor</b> between the amplifier and "
        "the driver, dialling in <em>one frequency at a time</em> by hand. You will need "
        "an adjustable sine source — a frequency generator, or a tone-generator app on a "
        "phone feeding a small amplifier — a <b>true-RMS multimeter</b>, a series "
        "resistor, and some plasticine or Blu-Tack for the added-mass step.<br>"
        "At each step the wizard calculates the exact meter reading to hunt for — you "
        "only change the frequency until the meter shows it."));

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
    setSubTitle("Last step — identify the driver these results belong to.");

    m_make       = new QLineEdit;
    m_model      = new QLineEdit;
    m_measuredBy = new QLineEdit;
    m_measuredBy->setText(QSettings().value("wizard/userName").toString());
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
//  Page 3 – Piston Diameter
// ════════════════════════════════════════════════════════════════
PistonPage::PistonPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 1 – Piston Diameter  (Dᵈ)");
    setSubTitle("One measurement with a ruler — easiest done before anything is wired up.");

    m_Dd_mm = makeSpin(10.0, 800.0, 1, 0.5, " mm");
    registerField("Dd_mm", m_Dd_mm, "value", SIGNAL(valueChanged(double)));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    auto *diag = new PistonDiagram;
    diag->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(diag);

    layout->addWidget(makeInstructionLabel(
        "Measure straight across the cone, from the <b>middle of the surround on one "
        "side to the middle of the surround on the other</b> — half of the surround "
        "moves with the cone, so it counts. This sets the piston area Sᵈ, which Vas "
        "and the SPL figures all scale from, so measure carefully."));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow("Dᵈ  – piston diameter:", m_Dd_mm);
    layout->addLayout(form);
    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════
//  Page 4 – DC Resistance
// ════════════════════════════════════════════════════════════════
DCResistancePage::DCResistancePage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 2 – Voice Coil DC Resistance  (Rₑ)");
    setSubTitle("A plain DC measurement with an ohmmeter — no signal source is involved yet.");

    m_Re = makeSpin(0.10, 200.0, 3, 0.01, " Ω");
    registerField("Re", m_Re, "value", SIGNAL(valueChanged(double)));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);
    layout->addWidget(makeInstructionLabel(
        "Use a <b>low resistance range (0–20 Ω)</b>, accuracy better than ±0.1 Ω.<br>"
        "<b>Lead correction:</b> a 2-wire meter also reads its own leads. Short the "
        "probes together, note the reading (often 0.2–0.5 Ω), and subtract it from "
        "your measurement — or press the meter's REL / zero button while the probes "
        "are shorted, if it has one.<br>"
        "<b>Temperature:</b> a copper voice coil at 20 °C reads ~0.9974× its 25 °C value."));

    layout->addWidget(makeInstructionLabel(
        "<b>Dual voice coil?</b> Measure the two coils wired <b>in series</b>, and if "
        "that is how the driver will be used, keep exactly that wiring for the whole "
        "test. If it will be wired in <b>parallel</b>, the parallel Rₑ is the series "
        "reading <b>÷ 4</b> — enter the Rₑ that matches the wiring you actually test with."));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->addRow("Rₑ  (DC resistance):", m_Re);
    layout->addLayout(form);
    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════
//  Page 5 – Measurement Setup
// ════════════════════════════════════════════════════════════════
MeasurementSetupPage::MeasurementSetupPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 3 – Measurement Setup");
    setSubTitle("Set your test voltage and circuit — do not change these after measurements begin.");

    // Starting values come from Advanced Settings → Measurement Defaults.
    const QSettings s;
    m_vmeas = makeSpin(0.01, 50.0, 3, 0.1, " V");
    m_vmeas->setValue(s.value("wizard/defaultVmeas", 1.0).toDouble());
    m_rs    = makeSpin(1.0, 1000.0, 1, 1.0, " Ω");
    m_rs->setValue(s.value("wizard/defaultRs", 50.0).toDouble());

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

    auto *diag = new SetupDiagram;
    diag->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(diag);

    layout->addWidget(makeInstructionLabel(
        "<b>Series resistor (Rₛ)</b><br>"
        "Place a resistor in series between the amplifier and the driver — "
        "<b>50 or 100 Ω</b> is a good choice for a typical 4 Ω driver. The wizard uses "
        "this value to calculate the target voltages for f₁, f₂ and f₃.<br>"
        "<b>Very low impedance driver?</b> With a 1 Ω-wired subwoofer the readings sink "
        "to the bottom of the meter's range. <b>Four 100 Ω resistors in parallel "
        "(25 Ω)</b> lifts the readings and shares the heat — most meters are at their "
        "most accurate with the reading sitting in the <b>200 mV range</b>."));

    layout->addWidget(makeInstructionLabel(
        "<b>Signal source &amp; measurement voltage</b><br>"
        "Any clean adjustable sine source works: a frequency generator, or a "
        "tone-generator app on a phone feeding a small amplifier. Set it to a fixed "
        "output level — for a mid-woofer aim for ~1 V rms <em>across the driver</em> at "
        "resonance. <b>Do not adjust this voltage again until every step is complete.</b>"));

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

    layout->addWidget(makeInstructionLabel(
        "<b>Meter:</b> a <b>true-RMS</b> meter is the minimum requirement here. At these "
        "millivolt levels a good true-RMS multimeter is often <em>more</em> accurate "
        "than reading peak-to-peak on an oscilloscope."));

    layout->addStretch();
}

// ════════════════════════════════════════════════════════════════
//  Page 6 – Resonance  (fₛ, f₁, f₂)
// ════════════════════════════════════════════════════════════════
FreeAirPage::FreeAirPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 4 – Find the Resonance  (fₛ, f₁, f₂)");
    setSubTitle("Change the frequency by hand — the highest voltage on the meter marks the resonance.");

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
        "Keep the voltage exactly as set in Step 3. Start well <b>below</b> the expected "
        "resonance and <b>raise the frequency slowly</b> while watching the meter: the "
        "reading climbs, peaks, then falls again. The frequency where the voltage is "
        "<b>highest</b> is the resonance <b>fₛ</b> — that is where Zmax sits. Enter that "
        "frequency and the highest voltage, and the target voltage for finding f₁ and f₂ "
        "will be calculated below."));

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
    calcBox->setStyleSheet(themed(
        "QGroupBox{font-weight:bold;font-size:9pt;border:1px solid #ccd;"
        "border-radius:4px;margin-top:14px;padding:8px 6px 4px 6px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:8px;padding:0 4px;"
        "color:%accent%;}"));
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
            QString("&#128269; <b>Target voltage for f₁ and f₂:</b>  "
                    "<span style='font-size:12pt;color:#c0392b;'>%1 %2</span>"
                    "<br>Tune <b>down</b> from fₛ until the meter reads exactly this — "
                    "that frequency is f₁. Then tune <b>up</b>, back through fₛ, until "
                    "the same reading appears on the other side — that is f₂.")
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
//  Page 8 – Added Mass  (done last — the cone gets sticky)
// ════════════════════════════════════════════════════════════════
AddedMassPage::AddedMassPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 6 – Added Mass  (Δm)");
    setSubTitle("Stick a known mass to the cone and find the new, lower resonance — mass accuracy is everything.");

    m_deltaM_g = makeSpin(0.1, 500.0, 2, 0.5, " g");
    m_fo       = makeSpin(1.0, 2000.0, 2, 0.1, " Hz");

    registerField("deltaM_g", m_deltaM_g, "value", SIGNAL(valueChanged(double)));
    registerField("fo",       m_fo,       "value", SIGNAL(valueChanged(double)));

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    layout->addWidget(makeInstructionLabel(
        "Stick plasticine / Blu-Tack to the <b>centre of the cone / dust cap</b> — never "
        "the surround. No magnets — they disturb the motor. The mass must be fully "
        "stuck down; nothing may flap or vibrate.<br>"
        "• <b>How much?</b> Add mass until the resonance — found again as the highest "
        "meter reading, at the <b>same voltage as Step 3</b> — lands <b>⅓ to ½ below "
        "fₛ</b> (e.g. a 30 Hz driver pulled down to 15–20 Hz).<br>"
        "• <b>Weigh the piece afterwards</b>, including every scrap of Blu-Tack that "
        "went onto the cone. The weight error goes straight into mms, Vas and BL — a "
        "digital kitchen scale is often <em>not</em> good enough; a 0.1 g pocket or "
        "jeweller's scale is worth borrowing.<br>"
        "Enter the exact added mass and the new, lower resonance f₀."));

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
//  Page 7 – Minimum Impedance & f₃
// ════════════════════════════════════════════════════════════════
PhysicalPage::PhysicalPage(QWidget *parent) : QWizardPage(parent)
{
    setTitle("Step 5 – Minimum Impedance & f₃");
    setSubTitle("Climb above the resonance to find the lowest voltage — f₃ follows from it.");

    m_Zmin  = makeSpin(0.0001, 50.0, 4, 0.001,   " V rms");
    // f3 can sit well under 100 Hz on a big low-fs subwoofer; a floor of 100
    // silently clamped legitimate entries (e.g. 52 → 100). 0 = not yet entered;
    // validation enforces f3 > fs at the results step.
    m_f3    = makeSpin(0.0, 100000.0, 1, 1.0, " Hz");

    registerField("Zmin",  m_Zmin,  "value", SIGNAL(valueChanged(double)));
    registerField("f3",    m_f3,    "value", SIGNAL(valueChanged(double)));

    m_lblVmin = makeHintLbl();
    m_lblVf3  = makeHintLbl();

    connect(m_Zmin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &PhysicalPage::updateVoltageHints);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    layout->addWidget(makeInstructionLabel(
        "We need the <b>minimum impedance</b> before f₃ can be worked out — and for "
        "that, the <b>absolute lowest voltage</b>. From above the resonance peak, "
        "<b>climb slowly upward</b> in frequency: the reading keeps falling, flattens "
        "out, and eventually starts rising again. The minimum sits <em>much higher "
        "up</em> than you might expect, so take it slowly. <b>Ignore the frequency "
        "where it happens</b> — enter only the lowest voltage you saw:"));

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);
    form->addRow("V at Zmin – lowest voltage above fₛ:", m_Zmin);
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
                    "<br>Z_f₃ = √2 × %3 = %4 Ω.  Keep climbing until the reading has "
                    "risen back <em>above</em> this value, then come back <b>down</b> — "
                    "the frequency where the meter reads it is f₃.")
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
    auto *nm  = new QLabel(name); nm->setStyleSheet(themed("color:%text2%;"));
    out = new QLabel("–");
    out->setStyleSheet(themed("font-weight:bold; color:%text2%; font-size:10pt;"));
    auto *un  = new QLabel(unit); un->setStyleSheet(themed("color:%unit%;"));
    g->addWidget(sym, row, 0);
    g->addWidget(nm,  row, 1);
    g->addWidget(out, row, 2);
    g->addWidget(un,  row, 3);
    ++row;
}

// ════════════════════════════════════════════════════════════════
//  Page 9 – Results
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
    const QString defNotes = QSettings().value("wizard/defaultNotes").toString();
    if (!defNotes.isEmpty())
        m_notes->setPlainText(defNotes);

    auto *resBox = new QGroupBox("Calculated Parameters");
    resBox->setStyleSheet(themed(
        "QGroupBox{font-weight:bold;border:1px solid %border%;border-radius:5px;"
        "margin-top:16px;padding:12px 6px 6px 6px;background:%panel%;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 6px;color:%accent%;}"));
    auto *grid = new QGridLayout(resBox);
    grid->setColumnStretch(1,2); grid->setColumnStretch(2,1); grid->setColumnStretch(3,1);
    grid->setVerticalSpacing(2); grid->setHorizontalSpacing(8);

    auto hdr = [&](int col, const QString &t){
        auto *l = new QLabel("<b>"+t+"</b>"); l->setStyleSheet(themed("color:%muted%;font-size:8pt;"));
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
    addResultRow(grid,row,"SPL","Sensitivity (1 W / 1 m)","dB",m_lblSpl);
    addResultRow(grid,row,"SPL","Sensitivity (2.83 V / 1 m)","dB",m_lblSpl283);

    m_lblVerify = new QLabel; m_lblVerify->setTextFormat(Qt::RichText);
    grid->addWidget(new QLabel(themed("<span style='color:%muted%;font-size:8pt;'><b>√(f₁·f₂)</b></span>")),row,0);
    grid->addWidget(new QLabel("Resonance verify"),row,1);
    grid->addWidget(m_lblVerify,row,2,1,2);

    auto *notesBox = new QGroupBox("Notes");
    notesBox->setStyleSheet(themed(
        "QGroupBox{font-weight:bold;border:1px solid %border%;border-radius:5px;"
        "margin-top:16px;padding:12px 6px 6px 6px;background:%panel%;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;padding:0 6px;color:%accent%;}"));
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
    m_lblSpl->setText(fmt(r.Spl, 2));
    // 2.83 V into Re is 8/Re watts, so the voltage-referenced figure sits
    // 10·log10(8/Re) above the 1 W figure (identical for an 8 Ω driver).
    const double spl283 = (r.Spl > 0.0 && r.Re > 0.0)
                        ? r.Spl + 10.0 * std::log10(8.0 / r.Re) : 0.0;
    m_lblSpl283->setText(fmt(spl283, 2));

    const double pct = (r.fs>0) ? 100.0*(r.fsVerify-r.fs)/r.fs : 0.0;
    const Theme &th = Theme::instance();
    const QString col = (std::fabs(pct)<3.0)
        ? (th.mode()==Theme::Mode::Light ? QStringLiteral("#15803D") : QStringLiteral("#86EFAC"))
        : hex(th.statusError());
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
    // Provenance for the detail/entry views: Re and Dd were measured/typed by
    // the user; the whole T/S set (incl. SPL) was calculated from measurements.
    m_record.userEnteredFields = FieldOrigin::Re | FieldOrigin::Dd;
    m_record.fieldOrigins      = FieldOrigin::AllTS | FieldOrigin::Spl;
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
    setMinimumSize(760, 800);

    addPage(new IntroPage);
    addPage(new PistonPage);
    addPage(new DCResistancePage);
    addPage(new MeasurementSetupPage);
    addPage(new FreeAirPage);
    addPage(new PhysicalPage);
    addPage(new AddedMassPage);
    addPage(new IdentityPage);      // identity last — right before the results
    m_resultsPage = new ResultsPage;
    addPage(m_resultsPage);

    setButtonText(QWizard::FinishButton, "Continue to Entry");

    connect(this, &QWizard::accepted, this, [this]() {
        emit recordReady(m_resultsPage->record());
    });
}
