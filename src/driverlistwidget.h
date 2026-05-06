#pragma once
#include "driverrecord.h"
#include "driverdb.h"
#include <QWidget>
#include <QList>

class QTableWidget;
class QLineEdit;
class QLabel;
class QPushButton;

// ─────────────────────────────────────────────────────────────────
//  DriverListWidget – searchable, sortable table of saved drivers.
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
    void onExportCsv();
    void onImportCsv();

private:
    void populate(const QList<DriverRecord> &records);
    int  selectedId() const;  ///< sort-safe: reads id from Qt::UserRole of selected row

    DriverDatabase *m_db;
    QTableWidget   *m_table;
    QLineEdit      *m_search;
    QLabel         *m_countLbl;
    QPushButton    *m_btnEdit   = nullptr;
    QPushButton    *m_btnDelete = nullptr;
    QPushButton    *m_btnUse    = nullptr;
};
