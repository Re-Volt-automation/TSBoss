#include "pdfreport.h"
#include "enclosurewidget.h"   // for BoxModel
#include "driverdb.h"          // for DriverDatabase
#include <cmath>
#include <functional>
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
#include <QAbstractItemView>
#include <QPixmap>
#include <QDir>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QTextDocument>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>

namespace {

QString fmt(double v, int decimals = 2) {
    return QString::number(v, 'f', decimals);
}
QString encTypeLabel(BoxModel::EncType t) {
    switch (t) {
    case BoxModel::EncType::Sealed:    return "Sealed";
    case BoxModel::EncType::Vented:    return "Vented";
    case BoxModel::EncType::IB:        return "Infinite Baffle";
    case BoxModel::EncType::Bandpass4: return "Bandpass (4th order)";
    case BoxModel::EncType::Bandpass6: return "Bandpass (6th order)";
    }
    return "Unknown";
}
bool hasPort(BoxModel::EncType t) {
    return t == BoxModel::EncType::Vented
        || t == BoxModel::EncType::Bandpass4
        || t == BoxModel::EncType::Bandpass6;
}
QString portShapeLabel(int s) { return s == 0 ? "Round" : "Rectangular"; }
bool isBandpass(BoxModel::EncType t) {
    return t == BoxModel::EncType::Bandpass4
        || t == BoxModel::EncType::Bandpass6;
}
QString naCell() { return QStringLiteral("&mdash;"); }
QString modelLabel(int i) { return QStringLiteral("M%1").arg(i + 1); }

// One legend, referenced by both the plots and the comparison tables:
// colored swatch + short M# label + full model name.
QString buildSharedLegend(const QList<BoxModel>& filtered) {
    QString h = "<table cellpadding='2' cellspacing='0' "
                "style='font-size:8pt; margin-bottom:8pt;'>";
    for (int i = 0; i < filtered.size(); ++i) {
        const auto& m = filtered[i];
        const QString name = m.name.isEmpty()
            ? QStringLiteral("(unnamed)") : m.name.toHtmlEscaped();
        h += QStringLiteral(
            "<tr>"
            "<td><span style='display:inline-block; width:10pt; height:10pt; "
            "background:%1;'></span></td>"
            "<td><b>%2</b></td>"
            "<td>%3</td>"
            "</tr>")
            .arg(m.color.name(), modelLabel(i), name);
    }
    h += "</table>";
    return h;
}

// A single comparison row: a label and a per-model value producer.
struct CmpRow {
    QString label;
    std::function<QString(const BoxModel&)> value;
};

// Render one grouped table. Rows whose value is naCell() for EVERY model
// are dropped, so type-specific rows (alpha, front fb, ports) only appear
// when at least one included model has them. Models are columns (M1..Mn).
QString buildCmpTable(const QString& heading,
                      const QList<CmpRow>& rows,
                      const QList<BoxModel>& filtered)
{
    QString h = QStringLiteral("<h3>%1</h3>").arg(heading);
    h += "<table border='1' cellpadding='2' cellspacing='0' "
         "style='font-size:8pt;'>";

    h += "<tr><td style='background:#eee;'><b>Parameter</b></td>";
    for (int i = 0; i < filtered.size(); ++i)
        h += QStringLiteral("<td style='background:#eee;'><b>%1</b></td>")
             .arg(modelLabel(i));
    h += "</tr>";

    for (const auto& row : rows) {
        bool anyReal = false;
        QStringList cells;
        for (const auto& m : filtered) {
            const QString v = row.value(m);
            if (v != naCell()) anyReal = true;
            cells << v;
        }
        if (!anyReal) continue;

        h += QStringLiteral("<tr><td>%1</td>").arg(row.label);
        for (const QString& c : cells)
            h += QStringLiteral("<td align='right'>%1</td>").arg(c);
        h += "</tr>";
    }
    h += "</table>";
    return h;
}

QString buildComparativeTables(const QList<BoxModel>& filtered,
                               DriverDatabase* db,
                               double appliedPower)
{
    Q_UNUSED(db);

    QString h = "<div class='pagebreak'></div>";
    h += "<h2>Model Comparison</h2>";
    h += buildSharedLegend(filtered);

    const QList<CmpRow> driver = {
        {"fs (Hz)",   [](const BoxModel& m){ return fmt(m.fs, 1); }},
        {"Vas (L)",   [](const BoxModel& m){ return fmt(m.Vas_L, 2); }},
        {"Qts",       [](const BoxModel& m){ return fmt(m.Qts, 3); }},
        {"Qes",       [](const BoxModel& m){ return fmt(m.Qes, 3); }},
        {"Qms",       [](const BoxModel& m){ return fmt(m.Qms, 3); }},
        {"Re (Ohm)",  [](const BoxModel& m){ return fmt(m.Re, 2); }},
        {"mms (g)",   [](const BoxModel& m){ return fmt(m.mms_g, 2); }},
        {"BL (Tm)",   [](const BoxModel& m){ return fmt(m.BL, 2); }},
        {"Sd (cm2)",  [](const BoxModel& m){ return fmt(m.Sd_cm2, 1); }},
        {"# drivers", [](const BoxModel& m){ return QString::number(m.numDrivers); }},
    };
    h += buildCmpTable("Driver T/S", driver, filtered);

    const QList<CmpRow> enclosure = {
        {"Type",            [](const BoxModel& m){ return encTypeLabel(m.encType).toHtmlEscaped(); }},
        {"Volume (L)",      [](const BoxModel& m){ return fmt(m.volumeL, 2); }},
        {"fb (Hz)",         [](const BoxModel& m){ return hasPort(m.encType) ? fmt(m.fb, 1) : naCell(); }},
        {"QL",              [](const BoxModel& m){ return hasPort(m.encType) ? fmt(m.QL, 2) : naCell(); }},
        {"Front vol (L)",   [](const BoxModel& m){ return isBandpass(m.encType) ? fmt(m.volumeFront_L, 2) : naCell(); }},
        {"Front fb (Hz)",   [](const BoxModel& m){ return isBandpass(m.encType) ? fmt(m.fbFront, 1) : naCell(); }},
        {"alpha",           [](const BoxModel& m){ return m.encType == BoxModel::EncType::Sealed ? fmt(m.alpha, 3) : naCell(); }},
        {"Fc (Hz)",         [](const BoxModel& m){ return m.encType == BoxModel::EncType::Sealed ? fmt(m.Fc, 1) : naCell(); }},
        {"Qtc",             [](const BoxModel& m){ return m.encType == BoxModel::EncType::Sealed ? fmt(m.Qtc, 3) : naCell(); }},
        {"Port shape",      [](const BoxModel& m){ return hasPort(m.encType) ? portShapeLabel(m.portShape) : naCell(); }},
        {"Port size (mm)",  [](const BoxModel& m) -> QString {
            if (!hasPort(m.encType)) return naCell();
            return m.portShape == 0
                ? QStringLiteral("D %1").arg(fmt(m.portWidth_mm, 1))
                : QStringLiteral("%1 x %2").arg(fmt(m.portWidth_mm, 1), fmt(m.portHeight_mm, 1));
        }},
        {"Shared walls",    [](const BoxModel& m){ return (hasPort(m.encType) && m.portShape != 0) ? QString::number(m.portWalls) : naCell(); }},
        {"# ports",         [](const BoxModel& m){ return hasPort(m.encType) ? QString::number(m.numPorts) : naCell(); }},
        {"2nd harmonic (Hz)", [](const BoxModel& m){ return (hasPort(m.encType) && m.portF2H > 0) ? fmt(m.portF2H, 1) : naCell(); }},
    };
    h += buildCmpTable("Enclosure &amp; Port", enclosure, filtered);

    const double power = appliedPower;
    const QList<CmpRow> results = {
        {"f3 (Hz)",            [](const BoxModel& m){ return fmt(m.f3, 1); }},
        {"eta0 (%)",           [](const BoxModel& m){ return fmt(m.eta, 3); }},
        {"Ref SPL 1W/1m (dB)", [](const BoxModel& m){ return fmt(m.spl, 1); }},
        {"Applied power (W)",  [power](const BoxModel&){ return fmt(power, 2); }},
        {"Passband SPL (dB)",  [power](const BoxModel& m){
            return fmt(m.spl + 10.0 * std::log10(std::max(power, 1e-6)), 1); }},
        {"Xmax (mm)",          [](const BoxModel& m){ return m.xmax_mm > 0 ? fmt(m.xmax_mm, 2) : naCell(); }},
        {"Xlim (mm)",          [](const BoxModel& m){ return m.xlim_mm > 0 ? fmt(m.xlim_mm, 2) : naCell(); }},
    };
    h += buildCmpTable("Results & Output", results, filtered);

    return h;
}

/// Build a filtered model list containing only the chosen indices.
QList<BoxModel> filterModels(const QList<BoxModel>& models,
                             const QList<int>& chosen)
{
    QList<BoxModel> out;
    out.reserve(chosen.size());
    for (int idx : chosen)
        if (idx >= 0 && idx < models.size())
            out.append(models[idx]);
    return out;
}

QPixmap renderSpl(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    ResponsePlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderGd(const QList<BoxModel>& m, QSize sz) {
    GroupDelayPlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderVolt(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    VoltagePlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderExc(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    ExcursionPlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderPv(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    PortVelocityPlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}

QString buildHtml(const PdfReportOptions& opts,
                  const QList<BoxModel>& filtered,
                  DriverDatabase* db)
{
    QStringList modelNames;
    for (const auto& m : filtered)
        modelNames << (m.name.isEmpty() ? QStringLiteral("(unnamed)") : m.name);

    const QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    const QString title = opts.projectName.isEmpty()
                          ? QStringLiteral("TSBoss Report") : opts.projectName;

    QString html;
    html += QStringLiteral(
        "<html><head><style>"
        "body { font-family: sans-serif; color: #111; }"
        "h1 { font-size: 22pt; margin-bottom: 4pt; }"
        "h2 { font-size: 16pt; margin-top: 6pt; margin-bottom: 4pt; }"
        "h3 { font-size: 12pt; margin-top: 12pt; margin-bottom: 3pt; }"
        ".meta { color: #555; font-size: 10pt; margin-bottom: 14pt; }"
        ".plot { margin-bottom: 10pt; }"
        ".pagebreak { page-break-before: always; }"
        "</style></head><body>");

    html += QStringLiteral("<h1>%1</h1>").arg(title.toHtmlEscaped());
    html += QStringLiteral("<div class='meta'>Exported %1 &middot; %2 model(s): %3</div>")
            .arg(today, QString::number(filtered.size()),
                 modelNames.join(", ").toHtmlEscaped());

    static const char* keys[]   = { "spl", "gd", "volt", "exc", "pv" };
    static const char* titles[] = {
        "SPL Response", "Group Delay", "Voltage Demand",
        "Cone Excursion", "Port Velocity"
    };
    // Two plots per page: force a page break before plots 0, 2, 4.
    // The shared legend sits once at the top of the first plots page.
    for (int i = 0; i < 5; ++i) {
        if (i % 2 == 0)
            html += QStringLiteral("<div class='pagebreak'></div>");
        if (i == 0)
            html += buildSharedLegend(filtered);
        html += QStringLiteral("<h2>%1</h2>").arg(titles[i]);
        html += QStringLiteral("<img class='plot' src='plot://%1' width='1000'>")
                .arg(keys[i]);
    }

    html += buildComparativeTables(filtered, db, opts.appliedPower);

    html += QStringLiteral("</body></html>");
    return html;
}

/// Show a modal dialog that lets the user pick which models to include.
/// `defaultIndices` pre-selects rows when "Selected models" is chosen.
/// Returns the chosen indices on accept, or an empty list on cancel
/// (or on accept with zero rows checked under "Selected models").
QList<int> runScopeDialog(QWidget* parent,
                          const QList<BoxModel>& models,
                          const QList<int>& defaultIndices)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("Export PDF Report");

    auto* root = new QVBoxLayout(&dlg);
    root->addWidget(new QLabel("Which models should be included in the report?"));

    auto* rbAll = new QRadioButton("All models");
    auto* rbSel = new QRadioButton("Selected models");
    rbAll->setChecked(true);
    root->addWidget(rbAll);
    root->addWidget(rbSel);

    auto* list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (int i = 0; i < models.size(); ++i) {
        auto* it = new QListWidgetItem(models[i].name.isEmpty()
                                       ? QString("Model %1").arg(i + 1)
                                       : models[i].name);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(defaultIndices.contains(i)
                          ? Qt::Checked : Qt::Unchecked);
        list->addItem(it);
    }
    list->setEnabled(false);
    root->addWidget(list);

    QObject::connect(rbSel, &QRadioButton::toggled,
                     list,  &QListWidget::setEnabled);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return {};

    QList<int> out;
    if (rbAll->isChecked()) {
        for (int i = 0; i < models.size(); ++i) out.append(i);
    } else {
        for (int i = 0; i < models.size(); ++i)
            if (list->item(i)->checkState() == Qt::Checked) out.append(i);
    }
    return out;
}

} // namespace

bool PdfReport::exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db)
{
    if (models.isEmpty()) {
        QMessageBox::information(parent, "PDF Export",
            "Add at least one model before exporting.");
        return false;
    }

    const QList<int> chosen = runScopeDialog(parent, models, opts.selectedIndices);
    if (chosen.isEmpty()) return false;  // cancel OR zero checked

    const auto filtered = filterModels(models, chosen);

    if (filtered.size() > 10) {
        QMessageBox::information(parent, "PDF Export",
            QStringLiteral("This report compares up to 10 models. "
                           "You selected %1 — please choose 10 or fewer.")
                .arg(filtered.size()));
        return false;
    }

    const QString defaultName = (opts.projectName.isEmpty()
                                 ? QStringLiteral("enclosure")
                                 : opts.projectName) + ".pdf";
    const QString path = QFileDialog::getSaveFileName(
        parent, "Export PDF", QDir::homePath() + "/" + defaultName,
        "PDF Document (*.pdf)");
    if (path.isEmpty()) return false;

    const QSize plotPx(2000, 1150);   // ~1.74:1 -> 1000px wide scales to ~575px tall
    const QPixmap pxSpl  = renderSpl (filtered, opts.appliedPower, opts.perDriverMode, plotPx);
    const QPixmap pxGd   = renderGd  (filtered, plotPx);
    const QPixmap pxVolt = renderVolt(filtered, opts.appliedPower, opts.perDriverMode, plotPx);
    const QPixmap pxExc  = renderExc (filtered, opts.appliedPower, opts.perDriverMode, plotPx);
    const QPixmap pxPv   = renderPv  (filtered, opts.appliedPower, opts.perDriverMode, plotPx);

    QTextDocument doc;
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://spl"),  pxSpl);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://gd"),   pxGd);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://volt"), pxVolt);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://exc"),  pxExc);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://pv"),   pxPv);
    doc.setHtml(buildHtml(opts, filtered, db));

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    writer.setResolution(150);
    doc.setPageSize(QSizeF(writer.width(), writer.height()));
    doc.print(&writer);

    QMessageBox::information(parent, "PDF Export",
        "Report exported to " + QFileInfo(path).fileName());
    return true;
}
