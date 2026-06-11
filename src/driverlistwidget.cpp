#include "driverlistwidget.h"
#include "theme.h"
#include "driverrecord.h"
#include "drivercsv.h"
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
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QItemSelectionModel>
#include <QFileDialog>
#include <QMessageBox>
#include <QShortcut>
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
    m_btnUse     = new QPushButton("Use for Enclosure");
    m_btnEdit    = new QPushButton("Edit");
    m_btnDelete  = new QPushButton("Delete");
    m_btnCompare = new QPushButton("Compare");
    m_btnCompare->setToolTip("Select 2–4 drivers (Ctrl+click) to compare side by side");
    m_btnUse->setEnabled(false);
    m_btnEdit->setEnabled(false);
    m_btnDelete->setEnabled(false);
    m_btnCompare->setEnabled(false);
    auto *btnFilters = new QPushButton("Filters ▾");
    btnFilters->setCheckable(true);
    auto *btnImport  = new QPushButton("Import CSV");
    auto *btnExport  = new QPushButton("Export CSV");
    m_countLbl = new QLabel;
    m_countLbl->setStyleSheet(themed("color:%muted%;"));

    for (auto *b : {btnRefresh}) {
        b->setStyleSheet(themed("QPushButton{background:%accentHov%;color:white;border-radius:4px;"
                         "padding:5px 14px;}QPushButton:hover{background:%accentLt%;}"));
    }
    m_btnUse->setStyleSheet("QPushButton{background:#27ae60;color:white;border-radius:4px;"
                           "padding:5px 14px;}QPushButton:hover{background:#1e8449;}"
                           "QPushButton:disabled{background:#b0d9c0;color:#aaa;}");
    m_btnEdit->setStyleSheet("QPushButton{background:#f39c12;color:white;border-radius:4px;"
                            "padding:5px 14px;}QPushButton:hover{background:#e67e22;}"
                            "QPushButton:disabled{background:#e0d0b0;color:#aaa;}");
    m_btnDelete->setStyleSheet(themed("QPushButton{background:%error%;color:white;border-radius:4px;"
                               "padding:5px 14px;}QPushButton:hover{background:%error%;}"
                               "QPushButton:disabled{background:#e0c0c0;color:#aaa;}"));
    btnImport->setStyleSheet("QPushButton{background:#95a5a6;color:white;border-radius:4px;"
                             "padding:5px 14px;}QPushButton:hover{background:#7f8c8d;}");
    btnExport->setStyleSheet("QPushButton{background:#95a5a6;color:white;border-radius:4px;"
                             "padding:5px 14px;}QPushButton:hover{background:#7f8c8d;}");
    m_btnCompare->setStyleSheet("QPushButton{background:#8e44ad;color:white;border-radius:4px;"
                                "padding:5px 14px;}QPushButton:hover{background:#7d3c98;}"
                                "QPushButton:disabled{background:#cdb8d8;color:#aaa;}");
    btnFilters->setStyleSheet("QPushButton{background:#95a5a6;color:white;border-radius:4px;"
                              "padding:5px 14px;}QPushButton:hover{background:#7f8c8d;}"
                              "QPushButton:checked{background:#566573;}");

    toolbar->addWidget(m_search);
    toolbar->addWidget(btnRefresh);
    toolbar->addWidget(btnFilters);
    toolbar->addWidget(m_btnUse);
    toolbar->addWidget(m_btnEdit);
    toolbar->addWidget(m_btnDelete);
    toolbar->addWidget(m_btnCompare);
    toolbar->addWidget(btnImport);
    toolbar->addWidget(btnExport);
    toolbar->addStretch();
    toolbar->addWidget(m_countLbl);

    // ── Parametric filter row (collapsed until Filters is toggled)
    m_filterRow = new QWidget;
    {
        auto *h = new QHBoxLayout(m_filterRow);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        auto addRange = [&](const QString &name, QDoubleSpinBox *&lo,
                            QDoubleSpinBox *&hi, double max, int dec,
                            const QString &suffix) {
            auto *lbl = new QLabel(name + ":");
            lbl->setStyleSheet(themed("color:%muted%;"));
            h->addWidget(lbl);
            lo = mkFilterSpin(max, dec, suffix);
            hi = mkFilterSpin(max, dec, suffix);
            h->addWidget(lo);
            h->addWidget(new QLabel("–"));
            h->addWidget(hi);
            h->addSpacing(10);
        };
        addRange("fs",  m_fsMin,  m_fsMax,  500.0,  1, " Hz");
        addRange("Qts", m_qtsMin, m_qtsMax, 3.0,    2, "");
        addRange("Vas", m_vasMin, m_vasMax, 2000.0, 1, " L");
        addRange("Rₑ",  m_reMin,  m_reMax,  64.0,   1, " Ω");
        auto *btnClear = new QPushButton("Clear");
        btnClear->setStyleSheet("QPushButton{background:#95a5a6;color:white;border-radius:4px;"
                                "padding:3px 10px;}QPushButton:hover{background:#7f8c8d;}");
        connect(btnClear, &QPushButton::clicked, this, [this] {
            for (auto *s : {m_fsMin, m_fsMax, m_qtsMin, m_qtsMax,
                            m_vasMin, m_vasMax, m_reMin, m_reMax}) {
                s->blockSignals(true); s->setValue(0.0); s->blockSignals(false);
            }
            refresh();
        });
        h->addWidget(btnClear);
        h->addStretch();
    }
    m_filterRow->setVisible(false);
    connect(btnFilters, &QPushButton::toggled, m_filterRow, &QWidget::setVisible);

    // ── Table
    m_table = new QTableWidget(0, C_COUNT);
    m_table->setHorizontalHeaderLabels(HEADERS);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setShowGrid(false);
    m_table->setObjectName("driverTable");
    // Structural panel styling lives in global QSS so live theme switching
    // re-applies automatically. See theme.cpp::globalStylesheet().

    // ── Layout
    auto *vb = new QVBoxLayout(this);
    vb->setContentsMargins(16, 12, 16, 12);
    vb->setSpacing(8);

    auto *heading = new QLabel("Driver Database");
    heading->setStyleSheet(themed("font-size:14pt; font-weight:bold; color:%text2%;"));
    vb->addWidget(heading);
    vb->addLayout(toolbar);
    vb->addWidget(m_filterRow);
    vb->addWidget(m_table);

    // ── Connections
    connect(m_search,  &QLineEdit::textChanged,        this, &DriverListWidget::onSearch);
    connect(m_table,   &QTableWidget::cellDoubleClicked,this, &DriverListWidget::onRowDoubleClicked);
    connect(m_table,   &QTableWidget::itemSelectionChanged, this, &DriverListWidget::onSelectionChanged);
    connect(btnRefresh,&QPushButton::clicked, this, &DriverListWidget::refresh);
    connect(m_btnUse,     &QPushButton::clicked, this, &DriverListWidget::onUseClicked);
    connect(m_btnEdit,    &QPushButton::clicked, this, &DriverListWidget::onEditClicked);
    connect(m_btnDelete,  &QPushButton::clicked, this, &DriverListWidget::onDeleteClicked);
    connect(m_btnCompare, &QPushButton::clicked, this, &DriverListWidget::onCompareClicked);
    connect(btnImport, &QPushButton::clicked, this, &DriverListWidget::onImportCsv);
    connect(btnExport, &QPushButton::clicked, this, &DriverListWidget::onExportCsv);

    // Ctrl+F focuses the search box while this page is active
    auto *scFind = new QShortcut(QKeySequence::Find, this);
    scFind->setContext(Qt::WidgetWithChildrenShortcut);
    connect(scFind, &QShortcut::activated, this, [this] {
        m_search->setFocus();
        m_search->selectAll();
    });

    refresh();
}

void DriverListWidget::refresh()
{
    const QString q = m_search->text().trimmed();
    auto records = q.isEmpty() ? m_db->allDrivers()
                               : m_db->searchDrivers(q);
    const DriverFilter f = currentFilter();
    if (f.isActive()) {
        QList<DriverRecord> kept;
        kept.reserve(records.size());
        for (const auto &r : records)
            if (f.matches(r)) kept.append(r);
        records = kept;
    }
    populate(records);
    if (f.isActive()) m_countLbl->setText(m_countLbl->text() + "  (filtered)");
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
    const int n = m_table->selectionModel()
                ? m_table->selectionModel()->selectedRows().size() : 0;
    m_btnUse->setEnabled(n == 1);
    m_btnEdit->setEnabled(n == 1);
    m_btnDelete->setEnabled(n == 1);
    m_btnCompare->setEnabled(n >= 2 && n <= 4);
}

void DriverListWidget::onUseClicked()
{
    const int id = selectedId();
    if (id >= 0) emit useRequested(id);
}

QDoubleSpinBox *DriverListWidget::mkFilterSpin(double max, int dec, const QString &suffix)
{
    auto *s = new QDoubleSpinBox;
    s->setRange(0.0, max);
    s->setDecimals(dec);
    s->setSuffix(suffix);
    s->setSpecialValueText("any");      // 0 = bound unset
    s->setFixedWidth(86);
    s->setKeyboardTracking(false);      // refilter on commit, not every keystroke
    connect(s, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double) { refresh(); });
    return s;
}

DriverFilter DriverListWidget::currentFilter() const
{
    auto bound = [](QDoubleSpinBox *s) -> std::optional<double> {
        return (s && s->value() > 0.0) ? std::optional<double>(s->value())
                                       : std::nullopt;
    };
    DriverFilter f;
    f.fsMin = bound(m_fsMin);   f.fsMax = bound(m_fsMax);
    f.qtsMin = bound(m_qtsMin); f.qtsMax = bound(m_qtsMax);
    f.vasLMin = bound(m_vasMin); f.vasLMax = bound(m_vasMax);
    f.reMin = bound(m_reMin);   f.reMax = bound(m_reMax);
    return f;
}

QList<int> DriverListWidget::selectedIds() const
{
    QList<int> ids;
    if (!m_table->selectionModel()) return ids;
    const auto rows = m_table->selectionModel()->selectedRows();
    for (const auto &idx : rows) {
        const auto *item = m_table->item(idx.row(), C_MAKE);
        if (item) ids.append(item->data(Qt::UserRole).toInt());
    }
    return ids;
}

void DriverListWidget::onCompareClicked()
{
    const QList<int> ids = selectedIds();
    if (ids.size() < 2) return;

    QList<DriverRecord> drivers;
    for (int id : ids) {
        const auto r = m_db->loadDriver(id);
        if (r.isValid()) drivers.append(r);
    }
    if (drivers.size() < 2) return;

    struct Row { const char *name; const char *unit; int dec;
                 std::function<double(const DriverRecord &)> get; };
    const QList<Row> rows = {
        { "fs",   "Hz",  1, [](const DriverRecord &r){ return r.fs; } },
        { "Rₑ",   "Ω",   2, [](const DriverRecord &r){ return r.Re; } },
        { "Qms",  "",    3, [](const DriverRecord &r){ return r.Qms; } },
        { "Qes",  "",    3, [](const DriverRecord &r){ return r.Qes; } },
        { "Qts",  "",    3, [](const DriverRecord &r){ return r.Qts; } },
        { "Vas",  "L",   2, [](const DriverRecord &r){ return r.Vas * 1000.0; } },
        { "mms",  "g",   2, [](const DriverRecord &r){ return r.mms * 1000.0; } },
        { "Cms",  "mm/N",3, [](const DriverRecord &r){ return r.Cms * 1000.0; } },
        { "Rms",  "kg/s",3, [](const DriverRecord &r){ return r.Rms; } },
        { "BL",   "Tm",  2, [](const DriverRecord &r){ return r.BL; } },
        { "Sd",   "cm²", 1, [](const DriverRecord &r){ return r.Sd * 1e4; } },
        { "Lₑ",   "mH",  3, [](const DriverRecord &r){ return r.Le * 1000.0; } },
        { "Znom", "Ω",   0, [](const DriverRecord &r){ return r.Znom; } },
        { "Xmax", "mm",  1, [](const DriverRecord &r){ return r.Xmax; } },
        { "Xlim", "mm",  1, [](const DriverRecord &r){ return r.Xlim; } },
        { "Pe",   "W",   0, [](const DriverRecord &r){ return r.Pe; } },
        { "SPL",  "dB",  1, [](const DriverRecord &r){ return r.Spl; } },
    };

    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("Compare Drivers");
    auto *vb = new QVBoxLayout(dlg);

    auto *tbl = new QTableWidget(rows.size(), drivers.size());
    QStringList headers;
    for (const auto &d : drivers)
        headers << (d.make + "\n" + d.model);
    tbl->setHorizontalHeaderLabels(headers);
    QStringList vheaders;
    for (const auto &row : rows)
        vheaders << (row.unit[0] ? QString("%1 (%2)").arg(row.name, row.unit)
                                 : QString(row.name));
    tbl->setVerticalHeaderLabels(vheaders);
    tbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tbl->setSelectionMode(QAbstractItemView::NoSelection);
    tbl->setAlternatingRowColors(true);

    for (int ri = 0; ri < rows.size(); ++ri) {
        for (int ci = 0; ci < drivers.size(); ++ci) {
            const double v = rows[ri].get(drivers[ci]);
            auto *item = new QTableWidgetItem(
                v > 0.0 ? QString::number(v, 'f', rows[ri].dec) : QStringLiteral("–"));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            tbl->setItem(ri, ci, item);
        }
    }
    tbl->resizeColumnsToContents();
    tbl->resizeRowsToContents();
    vb->addWidget(tbl);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    vb->addWidget(bb);

    // Wide enough for all columns without manual stretching
    dlg->resize(220 + 130 * drivers.size(), 560);
    dlg->exec();
    dlg->deleteLater();
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
    const QString text = QTextStream(&f).readAll();

    // ── Parse (drivercsv handles quoting, multi-line fields, issues) ─
    const auto res = drivercsv::importCsv(text);
    if (!res.ok) {
        QMessageBox::critical(this, "Import error", res.error);
        return;
    }
    if (res.records.isEmpty()) {
        QMessageBox::warning(this, "Import", "No valid driver rows found in the file.");
        return;
    }

    // Unreadable cells would be imported as 0 — surface them, don't be silent.
    if (!res.issues.isEmpty()) {
        QString detail;
        const qsizetype shown = qMin<qsizetype>(res.issues.size(), 12);
        for (qsizetype i = 0; i < shown; ++i) {
            const auto &is = res.issues[i];
            detail += QString("• row %1, column '%2': \"%3\"\n")
                      .arg(is.row).arg(is.column, is.value);
        }
        if (res.issues.size() > shown)
            detail += QString("…and %1 more.\n").arg(res.issues.size() - shown);
        const auto ans = QMessageBox::warning(this, "Import warnings",
            QString("%1 value%2 could not be read and would be imported as 0:\n\n%3\n"
                    "Import anyway?")
            .arg(res.issues.size()).arg(res.issues.size() == 1 ? "" : "s").arg(detail),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (ans != QMessageBox::Yes) return;
    }

    QList<DriverRecord> records = res.records;

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
        if (m_db->saveDriver(r)) ++imported;   // importCsv already set id = -1
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
    out << drivercsv::exportCsv(m_db->allDrivers());
    QMessageBox::information(this, "Export complete",
        QString("Exported %1 drivers to:\n%2").arg(m_db->driverCount()).arg(path));
}
