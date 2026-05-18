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
#include <QPainter>
#include <QStandardPaths>
#include <QDir>

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

/// Render the SPL plot for the given filtered models into a QPixmap.
QPixmap renderSplPlot(const QList<BoxModel>& filtered,
                      double appliedPower,
                      bool perDriverMode,
                      QSize sizePx)
{
    ResponsePlot plot;
    plot.setAttribute(Qt::WA_DontShowOnScreen);
    plot.resize(sizePx);
    plot.setPower(appliedPower);
    plot.setPerDriverMode(perDriverMode);
    plot.setModels(filtered, -1);  // no active highlight in the report
    plot.ensurePolished();

    QPixmap px(sizePx);
    px.fill(Qt::white);
    plot.render(&px);
    return px;
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
    const QPixmap px = renderSplPlot(filtered, opts.appliedPower,
                                     opts.perDriverMode, QSize(1600, 900));

    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/tsboss_spl_test.png";
    if (!px.save(tmp, "PNG")) {
        QMessageBox::warning(parent, "PDF Export",
            "Could not write test pixmap to " + tmp);
        return false;
    }
    QMessageBox::information(parent, "PDF Export",
        "SPL plot rendered to " + tmp);
    return true;
}
