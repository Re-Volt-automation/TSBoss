#include "pdfreport.h"
#include "enclosurewidget.h"   // for BoxModel
#include "driverdb.h"          // for DriverDatabase
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>

bool PdfReport::exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db)
{
    Q_UNUSED(models);
    Q_UNUSED(opts);
    Q_UNUSED(db);
    QMessageBox::information(parent, "PDF Export",
        "PDF export is not yet implemented.");
    return false;
}
