#pragma once
#include <QString>
#include <QList>

class QWidget;
class DriverDatabase;
struct BoxModel;

struct PdfReportOptions {
    QString    projectName;       ///< Used for cover page title and default filename stem.
    QList<int> selectedIndices;   ///< Indices into the models list to include.
    double     appliedPower = 1.0;///< Watts to pass to power-aware plots (SPL, Voltage, Excursion, Port Velocity).
    bool       perDriverMode = false; ///< Mirror of the on-screen Per-Driver toggle.
};

class PdfReport {
public:
    /// Show a save-file dialog and write a PDF report to the chosen path.
    /// Returns true on success, false on cancel or I/O error.
    /// Shows its own QMessageBox on failure.
    static bool exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db);
};
