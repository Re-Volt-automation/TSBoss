#include "driverlistwidget.h"
#include "driverrecord.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QBrush>
#include <QColor>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>

// Column indices
enum Col { C_MAKE=0, C_MODEL, C_DATE, C_BY,
           C_FS, C_QTS, C_VAS, C_BL, C_RE, C_COUNT };

static const QStringList HEADERS = {
    "Make", "Model", "Date", "Measured By",
    "fₛ (Hz)", "Qts", "Vas (L)", "BL (Tm)", "Rₑ (Ω)"
};

DriverListWidget::DriverListWidget(DriverDatabase *db, QWidget *parent)
    : QWidget(parent), m_db(db)
{
    // ── Toolbar
    auto *toolbar = new QHBoxLayout;
    m_search = new QLineEdit;
    m_search->setPlaceholderText("Search make, model, serial, measured by…");
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(280);

    auto *btnRefresh = new QPushButton("Refresh");
    m_btnUse    = new QPushButton("Use for Enclosure");
    m_btnEdit   = new QPushButton("Edit");
    m_btnDelete = new QPushButton("Delete");
    m_btnUse->setEnabled(false);
    m_btnEdit->setEnabled(false);
    m_btnDelete->setEnabled(false);
    auto *btnImport  = new QPushButton("Import CSV");
    auto *btnExport  = new QPushButton("Export CSV");
    m_countLbl = new QLabel;
    m_countLbl->setStyleSheet("color:#666;");

    for (auto *b : {btnRefresh}) {
        b->setStyleSheet("QPushButton{background:#6b2a40;color:white;border-radius:4px;"
                         "padding:5px 14px;}QPushButton:hover{background:#8b3a50;}");
    }
    m_btnUse->setStyleSheet("QPushButton{background:#27ae60;color:white;border-radius:4px;"
                           "padding:5px 14px;}QPushButton:hover{background:#1e8449;}"
                           "QPushButton:disabled{background:#b0d9c0;color:#aaa;}");
    m_btnEdit->setStyleSheet("QPushButton{background:#f39c12;color:white;border-radius:4px;"
                            "padding:5px 14px;}QPushButton:hover{background:#e67e22;}"
                            "QPushButton:disabled{background:#e0d0b0;color:#aaa;}");
    m_btnDelete->setStyleSheet("QPushButton{background:#e74c3c;color:white;border-radius:4px;"
                               "padding:5px 14px;}QPushButton:hover{background:#c0392b;}"
                               "QPushButton:disabled{background:#e0c0c0;color:#aaa;}");
    btnImport->setStyleSheet("QPushButton{background:#95a5a6;color:white;border-radius:4px;"
                             "padding:5px 14px;}QPushButton:hover{background:#7f8c8d;}");
    btnExport->setStyleSheet("QPushButton{background:#95a5a6;color:white;border-radius:4px;"
                             "padding:5px 14px;}QPushButton:hover{background:#7f8c8d;}");

    toolbar->addWidget(m_search);
    toolbar->addWidget(btnRefresh);
    toolbar->addWidget(m_btnUse);
    toolbar->addWidget(m_btnEdit);
    toolbar->addWidget(m_btnDelete);
    toolbar->addWidget(btnImport);
    toolbar->addWidget(btnExport);
    toolbar->addStretch();
    toolbar->addWidget(m_countLbl);

    // ── Table
    m_table = new QTableWidget(0, C_COUNT);
    m_table->setHorizontalHeaderLabels(HEADERS);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setShowGrid(false);
    m_table->setStyleSheet(
        "QTableWidget{border:1px solid #ddd;border-radius:4px;background:#f5f5f5;}"
        "QTableWidget::item{padding:4px 8px;color:#1a1a1a;background:#f5f5f5;}"
        "QTableWidget::item:selected{background:#6b2a40;color:white;}"
    );

    // ── Layout
    auto *vb = new QVBoxLayout(this);
    vb->setContentsMargins(16, 12, 16, 12);
    vb->setSpacing(8);

    auto *heading = new QLabel("Driver Database");
    heading->setStyleSheet("font-size:14pt; font-weight:bold; color:#3a3a3a;");
    vb->addWidget(heading);
    vb->addLayout(toolbar);
    vb->addWidget(m_table);

    // ── Connections
    connect(m_search,  &QLineEdit::textChanged,        this, &DriverListWidget::onSearch);
    connect(m_table,   &QTableWidget::cellDoubleClicked,this, &DriverListWidget::onRowDoubleClicked);
    connect(m_table,   &QTableWidget::itemSelectionChanged, this, &DriverListWidget::onSelectionChanged);
    connect(btnRefresh,&QPushButton::clicked, this, &DriverListWidget::refresh);
    connect(m_btnUse,    &QPushButton::clicked, this, &DriverListWidget::onUseClicked);
    connect(m_btnEdit,   &QPushButton::clicked, this, &DriverListWidget::onEditClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &DriverListWidget::onDeleteClicked);
    connect(btnImport, &QPushButton::clicked, this, &DriverListWidget::onImportCsv);
    connect(btnExport, &QPushButton::clicked, this, &DriverListWidget::onExportCsv);

    refresh();
}

void DriverListWidget::refresh()
{
    const QString q = m_search->text().trimmed();
    const auto records = q.isEmpty() ? m_db->allDrivers()
                                     : m_db->searchDrivers(q);
    populate(records);
}

void DriverListWidget::populate(const QList<DriverRecord> &records)
{
    m_table->setSortingEnabled(false);
    m_table->setRowCount(records.size());

    auto cell = [&](int row, int col, const QString &txt) {
        auto *item = new QTableWidgetItem(txt);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_table->setItem(row, col, item);
    };
    static const QColor kCalcColor("#1a7db5");
    static const QColor kUserColor("#2e7d32");

    auto numCell = [&](int row, int col, double v, int d,
                       bool calculated = false, bool userEntered = false) {
        auto *item = new QTableWidgetItem(QString::number(v,'f',d));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        if (v > 0.0) {
            if (calculated)       item->setForeground(QBrush(kCalcColor));
            else if (userEntered) item->setForeground(QBrush(kUserColor));
        }
        m_table->setItem(row, col, item);
    };

    for (int i = 0; i < records.size(); ++i) {
        const auto &r = records[i];
        // Store driver ID in C_MAKE item so it survives column-sort reordering
        {
            auto *item = new QTableWidgetItem(r.make);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setData(Qt::UserRole, r.id);
            m_table->setItem(i, C_MAKE, item);
        }
        cell(i, C_MODEL, r.model);
        cell(i, C_DATE,  r.dateMeasured.toString("yyyy-MM-dd"));
        cell(i, C_BY,     r.measuredBy);
        numCell(i, C_FS,  r.fs,           1, false,                          r.fs > 0.0);
        numCell(i, C_QTS, r.Qts,          3, r.fieldOrigins & FieldOrigin::Qts, r.userEnteredFields & FieldOrigin::Qts);
        numCell(i, C_VAS, r.Vas * 1000.0, 2, r.fieldOrigins & FieldOrigin::Vas, r.userEnteredFields & FieldOrigin::Vas);
        numCell(i, C_BL,  r.BL,           2, r.fieldOrigins & FieldOrigin::BL,  r.userEnteredFields & FieldOrigin::BL);
        numCell(i, C_RE,  r.Re,           2, r.fieldOrigins & FieldOrigin::Re,   r.userEnteredFields & FieldOrigin::Re);
    }

    m_table->setSortingEnabled(true);
    m_table->resizeColumnsToContents();
    m_table->clearSelection();
    // Buttons disabled until user selects a row
    if (m_btnUse)    m_btnUse->setEnabled(false);
    if (m_btnEdit)   m_btnEdit->setEnabled(false);
    if (m_btnDelete) m_btnDelete->setEnabled(false);
    m_countLbl->setText(QString("%1 driver%2")
                        .arg(records.size())
                        .arg(records.size() == 1 ? "" : "s"));
}

void DriverListWidget::onSearch(const QString &) { refresh(); }

int DriverListWidget::selectedId() const
{
    const auto sel = m_table->selectedItems();
    if (sel.isEmpty()) return -1;
    const int row = m_table->row(sel.first());
    const auto *item = m_table->item(row, C_MAKE);
    return item ? item->data(Qt::UserRole).toInt() : -1;
}

void DriverListWidget::onRowDoubleClicked(int row, int)
{
    const auto *item = m_table->item(row, C_MAKE);
    if (item) emit driverSelected(item->data(Qt::UserRole).toInt());
}

void DriverListWidget::onSelectionChanged()
{
    const bool hasSelection = !m_table->selectedItems().isEmpty();
    m_btnUse->setEnabled(hasSelection);
    m_btnEdit->setEnabled(hasSelection);
    m_btnDelete->setEnabled(hasSelection);
}

void DriverListWidget::onUseClicked()
{
    const int id = selectedId();
    if (id >= 0) emit useRequested(id);
}

void DriverListWidget::onEditClicked()
{
    const int id = selectedId();
    if (id >= 0) emit editRequested(id);
}

void DriverListWidget::onDeleteClicked()
{
    const int id = selectedId();
    if (id < 0) return;

    const auto r = m_db->loadDriver(id);
    const auto ans = QMessageBox::question(this, "Delete driver",
        QString("Delete  %1 %2  (id=%3)?  This cannot be undone.")
        .arg(r.make, r.model).arg(id));
    if (ans == QMessageBox::Yes) {
        m_db->deleteDriver(id);
        emit deleteRequested(id);
        refresh();
    }
}

// Parse a single CSV line, handling double-quoted fields (RFC 4180-ish)
static QStringList parseCsvLine(const QString &line)
{
    QStringList fields;
    QString cur;
    bool inQuote = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (inQuote) {
            if (c == '"') {
                // Peek: escaped quote?
                if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; }
                else inQuote = false;
            } else { cur += c; }
        } else {
            if      (c == '"') { inQuote = true; }
            else if (c == ',') { fields << cur; cur.clear(); }
            else               { cur += c; }
        }
    }
    fields << cur;
    return fields;
}

void DriverListWidget::onImportCsv()
{
    // ── Pick file ─────────────────────────────────────────────────
    const QString path = QFileDialog::getOpenFileName(
        this, "Import CSV", QDir::homePath(), "CSV files (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Import error", "Cannot open file for reading.");
        return;
    }

    QTextStream in(&f);

    // ── Read and validate header ──────────────────────────────────
    const QString header = in.readLine();
    const QStringList cols = parseCsvLine(header);

    // Build a column-name → index map (case-insensitive)
    QMap<QString, int> colIdx;
    for (int i = 0; i < cols.size(); ++i)
        colIdx[cols[i].trimmed().toLower()] = i;

    // Required columns (must at minimum have make + model + one result)
    const QStringList required = {"make","model","re","fs","qts"};
    for (const auto &req : required) {
        if (!colIdx.contains(req)) {
            QMessageBox::critical(this, "Import error",
                QString("Column '%1' not found in CSV header.\n"
                        "Expected format matches TSBoss Export CSV.").arg(req));
            return;
        }
    }

    // ── Parse data rows ───────────────────────────────────────────
    QList<DriverRecord> records;
    int lineNum = 1;
    while (!in.atEnd()) {
        ++lineNum;
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;
        const QStringList f = parseCsvLine(line);

        auto dbl = [&](const QString &col) -> double {
            const int i = colIdx.value(col, -1);
            return (i >= 0 && i < f.size()) ? f[i].toDouble() : 0.0;
        };
        auto str = [&](const QString &col) -> QString {
            const int i = colIdx.value(col, -1);
            return (i >= 0 && i < f.size()) ? f[i].trimmed() : QString{};
        };

        DriverRecord r;
        r.id           = -1;             // always insert as new
        r.make         = str("make");
        r.model        = str("model");
        r.measuredBy   = str("measured_by");
        r.notes        = str("notes");
        const QString ds = str("date");
        r.dateMeasured = ds.isEmpty() ? QDate::currentDate()
                                      : QDate::fromString(ds, "yyyy-MM-dd");
        // Raw measurements
        r.Re     = dbl("re");
        r.fs     = dbl("fs");
        r.Zmax   = dbl("zmax");
        r.f1     = dbl("f1");
        r.f2     = dbl("f2");
        r.deltaM = dbl("deltam_kg");
        r.fo     = dbl("fo");
        r.Dd     = dbl("dd_m");
        r.Zmin   = dbl("zmin");
        r.f3     = dbl("f3");
        // Computed results
        r.Z12    = dbl("z12");
        r.R0     = dbl("r0");
        r.Qms    = dbl("qms");
        r.Qes    = dbl("qes");
        r.Qts    = dbl("qts");
        r.mms    = dbl("mms_kg");
        r.Rms    = dbl("rms");
        r.BL     = dbl("bl");
        r.Cms    = dbl("cms");
        r.Sd     = dbl("sd_m2");
        r.Vas    = dbl("vas_m3");
        r.Le     = dbl("le");
        // Additional linear parameters
        r.Znom   = dbl("znom");
        r.fLe    = dbl("fle");
        r.KLe    = dbl("kle");
        // Large signal parameters
        r.Xmax   = dbl("xmax_mm");
        r.Xlim   = dbl("xlim_mm");
        r.Pe     = dbl("pe");
        r.Hg     = dbl("hg_mm");
        r.Vd     = dbl("vd_cm3");

        if (r.make.isEmpty() && r.model.isEmpty()) continue; // skip blank rows
        records.append(r);
    }

    if (records.isEmpty()) {
        QMessageBox::warning(this, "Import", "No valid driver rows found in the file.");
        return;
    }

    // ── Ask add or overwrite ──────────────────────────────────────
    const int existing = m_db->driverCount();
    QMessageBox dlg(this);
    dlg.setWindowTitle("Import CSV");
    dlg.setText(QString("Found <b>%1 driver%2</b> in the file.<br><br>"
                        "How should they be imported?")
                .arg(records.size()).arg(records.size() == 1 ? "" : "s"));
    dlg.setInformativeText(QString("The database currently has <b>%1 driver%2</b>.")
                           .arg(existing).arg(existing == 1 ? "" : "s"));
    auto *btnAdd  = dlg.addButton("Add to database",      QMessageBox::AcceptRole);
    auto *btnOver = dlg.addButton("Overwrite database",   QMessageBox::DestructiveRole);
    dlg.addButton(QMessageBox::Cancel);
    dlg.setDefaultButton(btnAdd);
    dlg.exec();

    if (dlg.clickedButton() == btnOver) {
        const auto confirm = QMessageBox::warning(
            this, "Confirm overwrite",
            QString("This will permanently delete all %1 existing driver%2 "
                    "and replace them with the %3 imported driver%4.\n\n"
                    "This cannot be undone. Proceed?")
            .arg(existing).arg(existing == 1 ? "" : "s")
            .arg(records.size()).arg(records.size() == 1 ? "" : "s"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (confirm != QMessageBox::Yes) return;

        if (!m_db->clearAllDrivers()) {
            QMessageBox::critical(this, "Import error",
                "Failed to clear the database:\n" + m_db->lastError());
            return;
        }
    } else if (dlg.clickedButton() != btnAdd) {
        return;  // cancelled
    }

    // ── Insert records ────────────────────────────────────────────
    int imported = 0, failed = 0;
    for (auto &r : records) {
        r.id = -1;   // force insert
        if (m_db->saveDriver(r)) ++imported;
        else                     ++failed;
    }

    refresh();
    emit deleteRequested(-1);  // trigger status bar refresh in MainWindow

    QString msg = QString("Imported <b>%1 driver%2</b> successfully.")
                  .arg(imported).arg(imported == 1 ? "" : "s");
    if (failed > 0)
        msg += QString("<br><span style='color:red;'>%1 row%2 failed to save.</span>")
               .arg(failed).arg(failed == 1 ? "" : "s");
    QMessageBox::information(this, "Import complete", msg);
}

void DriverListWidget::onExportCsv()
{
    const QString path = QFileDialog::getSaveFileName(
        this, "Export to CSV", QDir::homePath() + "/drivers.csv",
        "CSV files (*.csv)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Export error", "Cannot open file for writing.");
        return;
    }
    QTextStream out(&f);
    // Header
    out << "id,make,model,date,measured_by,"
           "Re,fs,Zmax,f1,f2,deltaM_kg,fo,Dd_m,Zmin,f3,"
           "Z12,R0,Qms,Qes,Qts,mms_kg,Rms,BL,Cms,Sd_m2,Vas_m3,Le,"
           "Znom,fLe,KLe,Xmax_mm,Xlim_mm,Pe,Hg_mm,Vd_cm3,notes\n";

    for (const auto &r : m_db->allDrivers()) {
        out << r.id << ","
            << "\"" << r.make  << "\","
            << "\"" << r.model << "\","
            << r.dateMeasured.toString("yyyy-MM-dd") << ","
            << "\"" << r.measuredBy   << "\","
            << r.Re   << "," << r.fs   << "," << r.Zmax << ","
            << r.f1   << "," << r.f2   << "," << r.deltaM << ","
            << r.fo   << "," << r.Dd   << "," << r.Zmin  << "," << r.f3 << ","
            << r.Z12  << "," << r.R0   << "," << r.Qms   << ","
            << r.Qes  << "," << r.Qts  << "," << r.mms   << ","
            << r.Rms  << "," << r.BL   << "," << r.Cms   << ","
            << r.Sd   << "," << r.Vas  << "," << r.Le    << ","
            << r.Znom << "," << r.fLe  << "," << r.KLe   << ","
            << r.Xmax << "," << r.Xlim << "," << r.Pe    << ","
            << r.Hg   << "," << r.Vd   << ","
            << "\"" << QString(r.notes).replace('"','\'') << "\"\n";
    }
    QMessageBox::information(this, "Export complete",
        QString("Exported %1 drivers to:\n%2").arg(m_db->driverCount()).arg(path));
}
