#pragma once
#include "driverrecord.h"
#include "driverdb.h"
#include "driverfilter.h"
#include <QWidget>
#include <QList>

class QDoubleSpinBox;
class QTableWidget;
class QLineEdit;
class QLabel;
class QPushButton;

// ─────────────────────────────────────────────────────────────────
//  DriverListWidget – searchable, sortable table of saved drivers
//  with parametric range filters and a side-by-side compare view.
// ─────────────────────────────────────────────────────────────────
class DriverListWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DriverListWidget(DriverDatabase *db, QWidget *parent = nullptr);

    void refresh();

signals:
    void driverSelected(int id);
    void editRequested(int id);
    void deleteRequested(int id);
    void useRequested(int id);

private slots:
    void onSearch(const QString &text);
    void onRowDoubleClicked(int row, int col);
    void onSelectionChanged();
    void onEditClicked();
    void onDeleteClicked();
    void onUseClicked();
    void onCompareClicked();
    void onExportCsv();
    void onImportCsv();

private:
    void populate(const QList<DriverRecord> &records);
    int  selectedId() const;   ///< sort-safe: reads id from Qt::UserRole of selected row
    QList<int> selectedIds() const;
    DriverFilter currentFilter() const;
    QDoubleSpinBox *mkFilterSpin(double max, int dec, const QString &suffix);
    void restyleControls();          ///< (re)apply theme-derived inline styles
    void updateFilterButton();       ///< arrow direction + active-bound count

    DriverDatabase *m_db;
    QTableWidget   *m_table;
    QLineEdit      *m_search;
    QLabel         *m_countLbl;
    QPushButton    *m_btnEdit    = nullptr;
    QPushButton    *m_btnDelete  = nullptr;
    QPushButton    *m_btnUse     = nullptr;
    QPushButton    *m_btnCompare = nullptr;
    QPushButton    *m_btnRefresh = nullptr;
    QPushButton    *m_btnImport  = nullptr;
    QPushButton    *m_btnExport  = nullptr;
    QPushButton    *m_btnFilters = nullptr;

    // Parametric filter row (collapsed by default)
    QWidget        *m_filterRow  = nullptr;
    QDoubleSpinBox *m_fsMin  = nullptr, *m_fsMax  = nullptr;
    QDoubleSpinBox *m_qtsMin = nullptr, *m_qtsMax = nullptr;
    QDoubleSpinBox *m_vasMin = nullptr, *m_vasMax = nullptr;
    QDoubleSpinBox *m_reMin  = nullptr, *m_reMax  = nullptr;
};
