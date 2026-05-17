#include "driverdetailwidget.h"
#include "theme.h"
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QFrame>

// ── helpers ──────────────────────────────────────────────────────

static QLabel *valLbl()
{
    auto *l = new QLabel("–");
    l->setStyleSheet(themed("font-weight:bold; color:%text2%;"));
    return l;
}

static void addFormRow(QFormLayout *f, const QString &lbl, QLabel *&out)
{
    out = valLbl();
    f->addRow(lbl, out);
}

static QGroupBox *makeSection(const QString &title)
{
    auto *g = new QGroupBox(title);
    g->setStyleSheet(themed(
        "QGroupBox{font-weight:bold;border:1px solid #ddd;"
        "border-radius:6px;margin-top:20px;padding:14px 8px 8px 8px;}"
        "QGroupBox::title{subcontrol-origin:margin;left:10px;"
        "padding:0 6px;color:%accent%;}"));
    return g;
}

// ── result row for T/S grid ───────────────────────────────────────
static void tsRow(QGridLayout *g, int &row,
                  const QString &sym, const QString &name,
                  const QString &unit, QLabel *&out)
{
    auto *s = new QLabel("<b>" + sym + "</b>");
    s->setMinimumWidth(42);
    g->addWidget(s,                             row, 0);
    auto *n = new QLabel(name); n->setStyleSheet("color:#555;");
    g->addWidget(n,                             row, 1);
    out = new QLabel("–");
    out->setStyleSheet(themed("font-weight:bold;color:%text2%;font-size:10pt;"));
    g->addWidget(out,                           row, 2);
    auto *u = new QLabel(unit); u->setStyleSheet("color:#888;");
    g->addWidget(u,                             row, 3);
    ++row;
}

// ─────────────────────────────────────────────────────────────────

DriverDetailWidget::DriverDetailWidget(QWidget *parent) : QWidget(parent)
{
    buildUi();
}

void DriverDetailWidget::buildUi()
{
    auto *inner  = new QWidget;
    auto *layout = new QVBoxLayout(inner);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    // ── Title row
    auto *titleRow = new QHBoxLayout;
    auto *title    = new QLabel("Driver Detail");
    title->setStyleSheet(themed("font-size:14pt; font-weight:bold; color:%text2%;"));
    titleRow->addWidget(title);
    titleRow->addStretch();
    auto *btnUse    = new QPushButton("Use for Enclosure");
    auto *btnEdit   = new QPushButton("Edit");
    auto *btnDelete = new QPushButton("Delete");
    btnUse->setStyleSheet("QPushButton{background:#27ae60;color:white;border-radius:4px;"
                          "padding:5px 16px;}QPushButton:hover{background:#1e8449;}");
    btnEdit->setStyleSheet("QPushButton{background:#f39c12;color:white;border-radius:4px;"
                           "padding:5px 16px;}QPushButton:hover{background:#e67e22;}");
    btnDelete->setStyleSheet(themed("QPushButton{background:%error%;color:white;border-radius:4px;"
                             "padding:5px 16px;}QPushButton:hover{background:%error%;}"));
    titleRow->addWidget(btnUse);
    titleRow->addWidget(btnEdit);
    titleRow->addWidget(btnDelete);
    layout->addLayout(titleRow);

    connect(btnUse,    &QPushButton::clicked, this, [this]{ if (m_currentId>=0) emit useRequested(m_currentId); });
    connect(btnEdit,   &QPushButton::clicked, this, [this]{ if (m_currentId>=0) emit editRequested(m_currentId); });
    connect(btnDelete, &QPushButton::clicked, this, [this]{ if (m_currentId>=0) emit deleteRequested(m_currentId); });

    // ── Identity section
    {
        auto *sec  = makeSection("Identity");
        auto *form = new QFormLayout(sec);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(6);
        addFormRow(form, "Manufacturer:",  m_lblMake);
        addFormRow(form, "Model:",         m_lblModel);
        addFormRow(form, "Date measured:", m_lblDate);
        addFormRow(form, "Measured by:",   m_lblBy);
        layout->addWidget(sec);
    }

    // ── Measurement setup section
    {
        auto *sec  = makeSection("Measurement Setup");
        auto *form = new QFormLayout(sec);
        form->setLabelAlignment(Qt::AlignRight);
        form->setSpacing(6);
        addFormRow(form, "Applied voltage:",  m_lblVmeas);
        addFormRow(form, "Series resistor:",  m_lblRs);
        addFormRow(form, "Voltage format:",   m_lblVmode);
        layout->addWidget(sec);
    }

    // ── Raw measurements section
    {
        auto *sec  = makeSection("Raw Measurements");
        auto *grid = new QGridLayout(sec);
        grid->setSpacing(6);
        int r = 0;
        tsRow(grid, r, "Rₑ",   "DC resistance",                  "Ω",    m_lblRe);
        tsRow(grid, r, "fₛ",   "Free-air resonance",             "Hz",   m_lblFs);
        tsRow(grid, r, "Zmax", "Peak impedance",                  "Ω",    m_lblZmax);
        tsRow(grid, r, "f₁",   "Lower side frequency",           "Hz",   m_lblF1);
        tsRow(grid, r, "f₂",   "Upper side frequency",           "Hz",   m_lblF2);
        tsRow(grid, r, "Δm",   "Added mass (plasticine)",         "g",    m_lblDeltaM);
        tsRow(grid, r, "f₀",   "Shifted resonance (with Δm)",    "Hz",   m_lblFo);
        tsRow(grid, r, "Dᵈ",   "Piston diameter",                "mm",   m_lblDd);
        tsRow(grid, r, "Zmin", "Min impedance above resonance",  "Ω",    m_lblZmin);
        tsRow(grid, r, "f₃",   "Frequency at Zmin + 3 dB",       "Hz",   m_lblF3);
        layout->addWidget(sec);
    }

    // ── T/S parameters section
    {
        auto *sec  = makeSection("Thiele/Small Parameters");
        auto *grid = new QGridLayout(sec);
        grid->setColumnStretch(1, 2);
        grid->setSpacing(6);

        auto hdr = [&](int col, const QString &t){
            auto *l = new QLabel("<b>" + t + "</b>");
            l->setStyleSheet("color:#888;font-size:8pt;");
            grid->addWidget(l,0,col);
        };
        hdr(0,"Sym"); hdr(1,"Parameter"); hdr(2,"Value"); hdr(3,"Unit");

        int r = 1;
        tsRow(grid, r, "Z₁₂",  "Side-freq impedance √(Rₑ·Zmax)", "Ω",      m_lblZ12);
        tsRow(grid, r, "√f₁f₂","Resonance verification",         "Hz",     m_lblFsVerify);
        tsRow(grid, r, "R₀",   "Zmax / Rₑ",                      "–",      m_lblR0);
        tsRow(grid, r, "Qms",  "Mechanical Q-factor",             "–",      m_lblQms);
        tsRow(grid, r, "Qes",  "Electrical Q-factor",             "–",      m_lblQes);
        tsRow(grid, r, "Qts",  "Total Q-factor",                  "–",      m_lblQts);
        tsRow(grid, r, "mms",  "Moving mass (incl. air)",         "g",      m_lblMms);
        tsRow(grid, r, "Cms",  "Suspension compliance",           "mm/N",   m_lblCms);
        tsRow(grid, r, "Rms",  "Mechanical loss resistance",      "kg/s",   m_lblRms);
        tsRow(grid, r, "BL",   "Force factor",                    "Tm",     m_lblBL);
        tsRow(grid, r, "Sᵈ",   "Effective piston area",           "cm²",    m_lblSd);
        tsRow(grid, r, "Vas",  "Equivalent air volume",           "litres", m_lblVas);
        tsRow(grid, r, "SPL",  "Reference sensitivity (1W/1m)",   "dB",     m_lblSpl);
        tsRow(grid, r, "Lₑ",   "Voice-coil inductance (emp.)",    "mH",     m_lblLe);
        layout->addWidget(sec);
    }

    // ── Additional linear parameters section
    {
        auto *sec  = makeSection("Additional Linear Parameters");
        auto *grid = new QGridLayout(sec);
        grid->setColumnStretch(1, 2);
        grid->setSpacing(6);

        int r = 0;
        tsRow(grid, r, "Znom", "Nominal impedance",            "Ω",         m_lblZnom);
        tsRow(grid, r, "fLe",  "Le measurement frequency",     "Hz",        m_lblFLe);
        tsRow(grid, r, "KLe",  "Semi-inductance coefficient",  "H·s^(1-n)", m_lblKLe);
        layout->addWidget(sec);
    }

    // ── Large signal parameters section
    {
        auto *sec  = makeSection("Large Signal Parameters");
        auto *grid = new QGridLayout(sec);
        grid->setColumnStretch(1, 2);
        grid->setSpacing(6);

        int r = 0;
        tsRow(grid, r, "Xmax", "Peak linear excursion",        "mm",  m_lblXmax);
        tsRow(grid, r, "Xlim", "Mechanical travel limit",      "mm",  m_lblXlim);
        tsRow(grid, r, "Pₑ",   "Rated continuous power",       "W",   m_lblPe);
        tsRow(grid, r, "Hg",   "Magnetic gap height",          "mm",  m_lblHg);
        tsRow(grid, r, "Vd",   "Peak volume displacement",     "cm³", m_lblVd);
        layout->addWidget(sec);
    }

    // ── Notes section
    {
        auto *sec = makeSection("Notes");
        auto *bl  = new QVBoxLayout(sec);
        m_lblNotes = new QLabel("–");
        m_lblNotes->setWordWrap(true);
        m_lblNotes->setStyleSheet("color:#444; padding:4px;");
        bl->addWidget(m_lblNotes);
        layout->addWidget(sec);
    }

    layout->addStretch();

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inner);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0,0,0,0);
    outerLayout->addWidget(scroll);
}

void DriverDetailWidget::loadDriver(const DriverRecord &r)
{
    m_currentId = r.id;
    auto fmt = [](double v, int d){ return QString::number(v,'f',d); };

    m_lblMake  ->setText(r.make.isEmpty()         ? "–" : r.make);
    m_lblModel ->setText(r.model.isEmpty()        ? "–" : r.model);
    m_lblDate  ->setText(r.dateMeasured.isValid() ? r.dateMeasured.toString("dd MMM yyyy") : "–");
    m_lblBy    ->setText(r.measuredBy.isEmpty()   ? "–" : r.measuredBy);
    m_lblNotes ->setText(r.notes.isEmpty()        ? "(none)" : r.notes);

    const QString vu = (r.voltageMode == 1) ? "Peak-to-peak" : "RMS";
    m_lblVmeas ->setText(QString("%1 V  (%2)").arg(r.V_meas,0,'f',3).arg(vu));
    m_lblRs    ->setText(QString("%1 Ω").arg(r.R_s,0,'f',1));
    m_lblVmode ->setText(vu);

    m_lblRe    ->setText(fmt(r.Re,    3) + " Ω");
    m_lblFs    ->setText(fmt(r.fs,    2) + " Hz");
    m_lblZmax  ->setText(fmt(r.Zmax,  2) + " Ω");
    m_lblF1    ->setText(fmt(r.f1,    2) + " Hz");
    m_lblF2    ->setText(fmt(r.f2,    2) + " Hz");
    m_lblDeltaM->setText(fmt(r.deltaM * 1000.0, 2) + " g");
    m_lblFo    ->setText(fmt(r.fo,    2) + " Hz");
    m_lblDd    ->setText(fmt(r.Dd    * 1000.0, 1) + " mm");
    m_lblZmin  ->setText(fmt(r.Zmin,  3) + " Ω");
    m_lblF3    ->setText(fmt(r.f3,    0) + " Hz");

    m_lblZ12    ->setText(fmt(r.Z12,   3) + " Ω");
    m_lblR0     ->setText(fmt(r.R0,    3));
    m_lblQms    ->setText(fmt(r.Qms,   3));
    m_lblQes    ->setText(fmt(r.Qes,   3));
    m_lblQts    ->setText(fmt(r.Qts,   3));
    m_lblMms    ->setText(fmt(r.mms  * 1000.0, 3) + " g");
    m_lblCms    ->setText(fmt(r.Cms  * 1000.0, 3) + " mm/N");
    m_lblRms    ->setText(fmt(r.Rms,   4) + " kg/s");
    m_lblBL     ->setText(fmt(r.BL,    3) + " Tm");
    m_lblSd     ->setText(fmt(r.Sd   * 10000.0, 2) + " cm²");
    m_lblVas    ->setText(fmt(r.Vas  * 1000.0, 3) + " L");
    m_lblSpl    ->setText(r.Spl > 0 ? fmt(r.Spl, 1) + " dB" : "–");
    m_lblLe     ->setText(fmt(r.Le   * 1000.0, 4) + " mH");

    // Additional linear parameters
    m_lblZnom->setText(r.Znom > 0 ? fmt(r.Znom, 1) + " Ω"         : "–");
    m_lblFLe ->setText(r.fLe  > 0 ? fmt(r.fLe,  1) + " Hz"        : "–");
    m_lblKLe ->setText(r.KLe  > 0 ? fmt(r.KLe,  5) + " H·s^(1-n)" : "–");

    // Large signal parameters
    m_lblXmax->setText(r.Xmax > 0 ? fmt(r.Xmax, 2) + " mm"  : "–");
    m_lblXlim->setText(r.Xlim > 0 ? fmt(r.Xlim, 2) + " mm"  : "–");
    m_lblPe  ->setText(r.Pe   > 0 ? fmt(r.Pe,   1) + " W"   : "–");
    m_lblHg  ->setText(r.Hg   > 0 ? fmt(r.Hg,   2) + " mm"  : "–");
    m_lblVd  ->setText(r.Vd   > 0 ? fmt(r.Vd,   2) + " cm³" : "–");

    // Verification with colour coding
    const double pct = (r.fs > 0) ? 100.0 * (r.fsVerify - r.fs) / r.fs : 0.0;
    const bool   ok  = std::fabs(pct) < 3.0;
    m_lblFsVerify->setText(
        QString("<span style='color:%1'>%2 Hz  (%3%4 %)</span>")
        .arg(ok ? "#27ae60" : "#e74c3c")
        .arg(r.fsVerify, 0,'f',2)
        .arg(pct>=0?"+":"").arg(pct,0,'f',2));
}

void DriverDetailWidget::clear()
{
    m_currentId = -1;
    for (auto *l : {m_lblVmeas, m_lblRs, m_lblVmode}) l->setText("–");
    for (auto *l : {m_lblMake, m_lblModel, m_lblDate, m_lblBy,
                    m_lblRe, m_lblFs, m_lblZmax, m_lblF1, m_lblF2,
                    m_lblDeltaM, m_lblFo, m_lblDd, m_lblZmin, m_lblF3,
                    m_lblZ12, m_lblR0, m_lblFsVerify, m_lblQms, m_lblQes,
                    m_lblQts, m_lblMms, m_lblCms, m_lblRms, m_lblBL,
                    m_lblSd, m_lblVas, m_lblSpl, m_lblLe,
                    m_lblZnom, m_lblFLe, m_lblKLe,
                    m_lblXmax, m_lblXlim, m_lblPe, m_lblHg, m_lblVd,
                    m_lblNotes})
        l->setText("–");
}
