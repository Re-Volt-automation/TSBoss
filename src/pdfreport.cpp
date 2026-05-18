#include "pdfreport.h"
#include "enclosurewidget.h"   // for BoxModel
#include "driverdb.h"          // for DriverDatabase
#include <cmath>
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

QString buildModelSection(const BoxModel& m,
                          DriverDatabase* db,
                          double appliedPower)
{
    QString driverDesc = "(no driver linked)";
    if (m.driverId >= 0 && db && db->isOpen()) {
        const auto r = db->loadDriver(m.driverId);
        if (!r.make.isEmpty() || !r.model.isEmpty())
            driverDesc = QString("%1 %2").arg(r.make, r.model).trimmed();
        else
            driverDesc = "(driver not found in database)";
    }

    QString h;
    h += "<div class='pagebreak'></div>";
    h += QStringLiteral("<h2>%1</h2>").arg(m.name.toHtmlEscaped());
    h += QStringLiteral("<div class='meta'>%1 &middot; %2</div>")
         .arg(encTypeLabel(m.encType).toHtmlEscaped(), driverDesc.toHtmlEscaped());

    // Table A — Driver T/S
    h += "<h3>Driver T/S</h3><table border='1' cellpadding='4' cellspacing='0'>";
    h += QString("<tr><td>fs</td><td>%1 Hz</td></tr>").arg(fmt(m.fs, 1));
    h += QString("<tr><td>Vas</td><td>%1 L</td></tr>").arg(fmt(m.Vas_L, 2));
    h += QString("<tr><td>Qts</td><td>%1</td></tr>").arg(fmt(m.Qts, 3));
    h += QString("<tr><td>Qes</td><td>%1</td></tr>").arg(fmt(m.Qes, 3));
    h += QString("<tr><td>Qms</td><td>%1</td></tr>").arg(fmt(m.Qms, 3));
    h += QString("<tr><td>Re</td><td>%1 &Omega;</td></tr>").arg(fmt(m.Re, 2));
    h += QString("<tr><td>mms</td><td>%1 g</td></tr>").arg(fmt(m.mms_g, 2));
    h += QString("<tr><td>BL</td><td>%1 Tm</td></tr>").arg(fmt(m.BL, 2));
    h += QString("<tr><td>Sd</td><td>%1 cm&sup2;</td></tr>").arg(fmt(m.Sd_cm2, 1));
    h += QString("<tr><td>Drivers</td><td>%1</td></tr>").arg(m.numDrivers);
    h += "</table>";

    // Table B — Enclosure
    h += "<h3>Enclosure</h3><table border='1' cellpadding='4' cellspacing='0'>";
    h += QString("<tr><td>Type</td><td>%1</td></tr>").arg(encTypeLabel(m.encType).toHtmlEscaped());
    h += QString("<tr><td>Volume</td><td>%1 L</td></tr>").arg(fmt(m.volumeL, 2));
    if (hasPort(m.encType)) {
        h += QString("<tr><td>fb</td><td>%1 Hz</td></tr>").arg(fmt(m.fb, 1));
        h += QString("<tr><td>QL</td><td>%1</td></tr>").arg(fmt(m.QL, 2));
    }
    if (m.encType == BoxModel::EncType::Bandpass4 ||
        m.encType == BoxModel::EncType::Bandpass6) {
        h += QString("<tr><td>Front volume</td><td>%1 L</td></tr>").arg(fmt(m.volumeFront_L, 2));
        h += QString("<tr><td>Front fb</td><td>%1 Hz</td></tr>").arg(fmt(m.fbFront, 1));
    }
    if (m.encType == BoxModel::EncType::Sealed) {
        h += QString("<tr><td>alpha</td><td>%1</td></tr>").arg(fmt(m.alpha, 3));
        h += QString("<tr><td>Fc</td><td>%1 Hz</td></tr>").arg(fmt(m.Fc, 1));
        h += QString("<tr><td>Qtc</td><td>%1</td></tr>").arg(fmt(m.Qtc, 3));
    }
    h += QString("<tr><td>f3</td><td>%1 Hz</td></tr>").arg(fmt(m.f3, 1));
    h += QString("<tr><td>&eta;&#8320;</td><td>%1 %</td></tr>").arg(fmt(m.eta, 3));
    h += QString("<tr><td>Reference SPL (1W/1m)</td><td>%1 dB</td></tr>").arg(fmt(m.spl, 1));
    h += "</table>";

    // Table C — Port geometry (only if ported)
    if (hasPort(m.encType)) {
        h += "<h3>Port Geometry (rear)</h3><table border='1' cellpadding='4' cellspacing='0'>";
        h += QString("<tr><td>Shape</td><td>%1</td></tr>").arg(portShapeLabel(m.portShape));
        if (m.portShape == 0) {
            h += QString("<tr><td>Diameter</td><td>%1 mm</td></tr>").arg(fmt(m.portWidth_mm, 1));
        } else {
            h += QString("<tr><td>Width</td><td>%1 mm</td></tr>").arg(fmt(m.portWidth_mm, 1));
            h += QString("<tr><td>Height</td><td>%1 mm</td></tr>").arg(fmt(m.portHeight_mm, 1));
            h += QString("<tr><td>Shared walls</td><td>%1</td></tr>").arg(m.portWalls);
        }
        h += QString("<tr><td>Number of ports</td><td>%1</td></tr>").arg(m.numPorts);
        if (m.portF2H > 0)
            h += QString("<tr><td>2nd pipe harmonic</td><td>%1 Hz</td></tr>").arg(fmt(m.portF2H, 1));
        h += "</table>";
    }

    // Table D — Applied power & predicted output
    h += "<h3>Applied Power &amp; Predicted Output</h3>";
    h += "<table border='1' cellpadding='4' cellspacing='0'>";
    h += QString("<tr><td>Applied power</td><td>%1 W</td></tr>").arg(fmt(appliedPower, 2));
    h += QString("<tr><td>Predicted passband SPL</td><td>%1 dB</td></tr>")
         .arg(fmt(m.spl + 10.0 * std::log10(std::max(appliedPower, 1e-6)), 1));
    if (m.xmax_mm > 0)
        h += QString("<tr><td>Driver Xmax</td><td>%1 mm</td></tr>").arg(fmt(m.xmax_mm, 2));
    if (m.xlim_mm > 0)
        h += QString("<tr><td>Driver Xlim</td><td>%1 mm</td></tr>").arg(fmt(m.xlim_mm, 2));
    h += "</table>";

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

QString buildLegendHtml(const QList<BoxModel>& filtered) {
    QString h = "<div style='font-size: 9pt; margin-bottom: 6pt;'>";
    for (int i = 0; i < filtered.size(); ++i) {
        const auto& m = filtered[i];
        if (i > 0) h += "&nbsp;&nbsp;";
        h += QStringLiteral(
            "<span style='display:inline-block; width:10pt; height:10pt; "
            "background:%1; vertical-align:middle;'></span> %2")
            .arg(m.color.name(), m.name.toHtmlEscaped());
    }
    h += "</div>";
    return h;
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
        "h2 { font-size: 16pt; margin-top: 18pt; border-bottom: 1px solid #888; padding-bottom: 2pt; }"
        ".meta { color: #555; font-size: 10pt; margin-bottom: 14pt; }"
        ".plot { margin-bottom: 14pt; }"
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
    for (int i = 0; i < 5; ++i) {
        html += QStringLiteral("<div class='pagebreak'></div>");
        html += QStringLiteral("<h2>%1</h2>").arg(titles[i]);
        html += buildLegendHtml(filtered);
        html += QStringLiteral("<img class='plot' src='plot://%1' width='720'>").arg(keys[i]);
    }

    for (const auto& m : filtered)
        html += buildModelSection(m, db, opts.appliedPower);

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

    const QString defaultName = (opts.projectName.isEmpty()
                                 ? QStringLiteral("enclosure")
                                 : opts.projectName) + ".pdf";
    const QString path = QFileDialog::getSaveFileName(
        parent, "Export PDF", QDir::homePath() + "/" + defaultName,
        "PDF Document (*.pdf)");
    if (path.isEmpty()) return false;

    const QSize plotPx(1600, 900);
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
