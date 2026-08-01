#include "driverdetailwidget.h"
#include "paramcheck.h"
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
        "QGroupBox{font-weight:bold;border:1px solid %border%;"
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
    auto *n = new QLabel(name); n->setStyleSheet(themed("color:%muted%;"));
    g->addWidget(n,                             row, 1);
    out = new QLabel("–");
    out->setStyleSheet(themed("font-weight:bold;color:%text2%;font-size:10pt;"));
    g->addWidget(out,                           row, 2);
    auto *u = new QLabel(unit); u->setStyleSheet(themed("color:%unit%;"));
    g->addWidget(u,                             row, 3);
    ++row;
}

// Value-origin colours — same theme-aware palette the driver list uses for
// its table cells, so a driver reads consistently across pages.
static QColor calcColor()
{
    return Theme::instance().mode() == Theme::Mode::Dark ? QColor("#4aa3d8")
                                                         : QColor("#1a7db5");
}
static QColor userColor()
{
    return Theme::instance().mode() == Theme::Mode::Dark ? QColor("#66bb6a")
                                                         : QColor("#2e7d32");
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
    btnUse->setStyleSheet(themed("QPushButton{background:%accent%;color:%onAccent%;border-radius:4px;"
                          "padding:5px 16px;font-weight:bold;}QPushButton:hover{background:%accentHov%;}"));
    btnEdit->setStyleSheet(themed("QPushButton{background:%input%;color:%text2%;border:1px solid %border2%;"
                           "border-radius:4px;padding:5px 16px;}QPushButton:hover{background:%inputFocus%;}"));
    btnDelete->setStyleSheet(themed("QPushButton{background:%input%;color:%text2%;border:1px solid %border2%;"
                             "border-radius:4px;padding:5px 16px;}"
                             "QPushButton:hover{background:%error%;color:white;border-color:%error%;}"));
    titleRow->addWidget(btnUse);
    titleRow->addWidget(btnEdit);
    titleRow->addWidget(btnDelete);
    layout->addLayout(titleRow);

    // Sanity-check banner — physics-identity warnings from paramcheck.h
    m_warnBanner = new QLabel;
    m_warnBanner->setWordWrap(true);
    m_warnBanner->setStyleSheet(themed(
        "QLabel{color:%warn%; background:%panel%; border:1px solid %warn%;"
        "border-radius:4px; padding:8px 12px;}"));
    m_warnBanner->setVisible(false);
    layout->addWidget(m_warnBanner);

    // Value-origin legend (moved here from the driver-database toolbar);
    // colours refresh in loadDriver() so a theme change is picked up.
    m_legendLbl = new QLabel;
    m_legendLbl->setToolTip("Coloured values: calculated = derived from other fields, "
                            "user-entered = typed in by hand.");
    m_legendLbl->setAlignment(Qt::AlignRight);
    layout->addWidget(m_legendLbl);

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

        // Send the stored measurements back into the Quick Measurement form
        // (same driver id — saving there updates this record, no duplicate).
        m_btnReopenQuick = new QPushButton("Reopen as Quick Measurement");
        m_btnReopenQuick->setToolTip(
            "Load these raw measurements back into the Quick Measurement form "
            "to re-check or redo values — saving updates this same driver.");
        m_btnReopenQuick->setStyleSheet(themed(
            "QPushButton{background:%input%;color:%text2%;border:1px solid %border2%;"
            "border-radius:4px;padding:5px 14px;}"
            "QPushButton:hover{background:%inputFocus%;}"));
        connect(m_btnReopenQuick, &QPushButton::clicked, this,
                [this]{ if (m_currentId >= 0) emit reopenQuickRequested(m_currentId); });
        grid->addWidget(m_btnReopenQuick, r, 0, 1, 4, Qt::AlignLeft);
        ++r;
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
            l->setStyleSheet(themed("color:%muted%;font-size:8pt;"));
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
        m_lblNotes->setStyleSheet(themed("color:%text2%; padding:4px;"));
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

    // Physics-identity sanity checks — flags placeholder Rₑ, slipped
    // decimals and mutually inconsistent Q values.
    if (m_warnBanner) {
        paramcheck::DriverParams p;
        p.fs = r.fs;  p.Qms = r.Qms; p.Qes = r.Qes; p.Qts = r.Qts;
        p.Re = r.Re;  p.mms_g = r.mms * 1000.0; p.BL = r.BL;
        p.Sd_cm2 = r.Sd * 1e4; p.Vas_L = r.Vas * 1000.0;
        QStringList w = paramcheck::driverParamWarnings(p);
        if (w.isEmpty()) {
            m_warnBanner->clear();
            m_warnBanner->setVisible(false);
        } else {
            for (auto &s : w) s.prepend(QStringLiteral("⚠ "));
            m_warnBanner->setText(w.join(QStringLiteral("\n")));
            m_warnBanner->setVisible(true);
        }
    }

    m_lblMake  ->setText(r.make.isEmpty()         ? "–" : r.make);
    m_lblModel ->setText(r.model.isEmpty()        ? "–" : r.model);
    m_lblDate  ->setText(r.dateMeasured.isValid() ? r.dateMeasured.toString("dd MMM yyyy") : "–");
    m_lblBy    ->setText(r.measuredBy.isEmpty()   ? "–" : r.measuredBy);
    m_lblNotes ->setText(r.notes.isEmpty()        ? "(none)" : r.notes);

    // The grid rows carry a dedicated unit column, so values are bare
    // numbers; anything never measured/provided (≤ 0) shows as "–".
    auto num = [&](QLabel *l, double v, int d) {
        l->setText(v > 0.0 ? fmt(v, d) : QStringLiteral("–"));
    };

    // Measurement setup only exists for measured drivers — hide the whole
    // section for datasheet-entered records instead of showing zeros.
    const bool hasSetup = r.V_meas > 0.0 || r.R_s > 0.0;
    if (auto *sec = m_lblVmeas->parentWidget()) sec->setVisible(hasSetup);
    // Reopen-as-quick only makes sense when raw measurements actually exist
    // (datasheet-entered records have no f1/f2 and would reopen as zeros).
    m_btnReopenQuick->setVisible(r.f1 > 0.0 && r.f2 > 0.0);
    const QString vu = (r.voltageMode == 1) ? "Peak-to-peak" : "RMS";
    m_lblVmeas ->setText(r.V_meas > 0 ? QString("%1 V  (%2)").arg(r.V_meas,0,'f',3).arg(vu) : "–");
    m_lblRs    ->setText(r.R_s   > 0 ? QString("%1 Ω").arg(r.R_s,0,'f',1) : "–");
    m_lblVmode ->setText(vu);

    num(m_lblRe,     r.Re,             3);
    num(m_lblFs,     r.fs,             2);
    num(m_lblZmax,   r.Zmax,           2);
    num(m_lblF1,     r.f1,             2);
    num(m_lblF2,     r.f2,             2);
    num(m_lblDeltaM, r.deltaM * 1000.0,2);
    num(m_lblFo,     r.fo,             2);
    num(m_lblDd,     r.Dd * 1000.0,    1);
    num(m_lblZmin,   r.Zmin,           3);
    num(m_lblF3,     r.f3,             0);

    num(m_lblZ12,  r.Z12,            3);
    num(m_lblR0,   r.R0,             3);
    num(m_lblQms,  r.Qms,            3);
    num(m_lblQes,  r.Qes,            3);
    num(m_lblQts,  r.Qts,            3);
    num(m_lblMms,  r.mms  * 1000.0,  3);
    num(m_lblCms,  r.Cms  * 1000.0,  3);
    num(m_lblRms,  r.Rms,            4);
    num(m_lblBL,   r.BL,             3);
    num(m_lblSd,   r.Sd   * 10000.0, 2);
    num(m_lblVas,  r.Vas  * 1000.0,  3);
    num(m_lblSpl,  r.Spl,            1);
    num(m_lblLe,   r.Le   * 1000.0,  4);

    // Colour the origin-tracked values (see the legend under the title):
    // blue = accepted from a suggest button / calculated, green = typed.
    m_legendLbl->setStyleSheet(themed("color:%muted%; font-size:8pt;"));
    m_legendLbl->setText(QString("<span style='color:%1;'>●</span> calculated"
                                 "&nbsp;&nbsp;<span style='color:%2;'>●</span> user-entered")
                         .arg(calcColor().name(), userColor().name()));
    auto paint = [&](QLabel *l, quint64 bit, double v) {
        QString sty = QStringLiteral("font-weight:bold;font-size:10pt;");
        if      (v > 0.0 && (r.fieldOrigins      & bit)) sty += "color:" + calcColor().name() + ";";
        else if (v > 0.0 && (r.userEnteredFields & bit)) sty += "color:" + userColor().name() + ";";
        else                                             sty += themed("color:%text2%;");
        l->setStyleSheet(sty);
    };
    paint(m_lblRe,  FieldOrigin::Re,  r.Re);
    paint(m_lblQms, FieldOrigin::Qms, r.Qms);
    paint(m_lblQes, FieldOrigin::Qes, r.Qes);
    paint(m_lblQts, FieldOrigin::Qts, r.Qts);
    paint(m_lblMms, FieldOrigin::Mms, r.mms);
    paint(m_lblCms, FieldOrigin::Cms, r.Cms);
    paint(m_lblRms, FieldOrigin::Rms, r.Rms);
    paint(m_lblBL,  FieldOrigin::BL,  r.BL);
    paint(m_lblSd,  FieldOrigin::Sd,  r.Sd);
    paint(m_lblDd,  FieldOrigin::Dd,  r.Dd);
    paint(m_lblVas, FieldOrigin::Vas, r.Vas);
    paint(m_lblSpl, FieldOrigin::Spl, r.Spl);

    // Additional linear parameters
    num(m_lblZnom, r.Znom, 1);
    num(m_lblFLe,  r.fLe,  1);
    num(m_lblKLe,  r.KLe,  5);

    // Large signal parameters
    num(m_lblXmax, r.Xmax, 2);
    num(m_lblXlim, r.Xlim, 2);
    num(m_lblPe,   r.Pe,   1);
    num(m_lblHg,   r.Hg,   2);
    num(m_lblVd,   r.Vd,   2);

    // Verification with colour coding — only meaningful when f₁/f₂ were
    // actually measured; datasheet records show "–" instead of a red error.
    if (r.fsVerify > 0.0 && r.fs > 0.0) {
        const double pct = 100.0 * (r.fsVerify - r.fs) / r.fs;
        const bool   ok  = std::fabs(pct) < 3.0;
        m_lblFsVerify->setText(
            QString("<span style='color:%1'>%2  (%3%4 %)</span>")
            .arg(ok ? "#27ae60" : hex(Theme::instance().statusError()))
            .arg(r.fsVerify, 0,'f',2)
            .arg(pct>=0?"+":"").arg(pct,0,'f',2));
    } else {
        m_lblFsVerify->setText("–");
    }
}

void DriverDetailWidget::clear()
{
    m_currentId = -1;
    if (m_warnBanner) { m_warnBanner->clear(); m_warnBanner->setVisible(false); }
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
