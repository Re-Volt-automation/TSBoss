#include "pdfreport.h"
#include "enclosurewidget.h"   // for BoxModel
#include "driverdb.h"          // for DriverDatabase
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
                  const QList<BoxModel>& filtered)
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
        html += QStringLiteral("<img class='plot' src='plot://%1' width='720'>").arg(keys[i]);
    }

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
    Q_UNUSED(db);

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
    doc.setHtml(buildHtml(opts, filtered));

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
