#pragma once
#include "driverrecord.h"
#include "driverdb.h"
#include <QMainWindow>
#include <QVector>
#include <QString>

class QStackedWidget;
class QSplitter;
class QLabel;
class QPushButton;
class DriverListWidget;
class DriverDetailWidget;
class QuickEntryWidget;
class DatasheetEntryWidget;
class EnclosureWidget;

// ─────────────────────────────────────────────────────────────────
//  MainWindow – application shell.
//
//  Layout:
//    ┌──────────┬──────────────────────────────────────┐
//    │          │                                      │
//    │ Sidebar  │   QStackedWidget (central content)   │
//    │  nav     │   0 – EnclosureWidget  (default)     │
//    │  buttons │   1 – DriverListWidget               │
//    │          │   2 – DriverDetailWidget             │
//    │          │   3 – QuickEntryWidget               │
//    │          │   4 – DatasheetEntryWidget           │
//    └──────────┴──────────────────────────────────────┘
//
//  The guided wizard is a modal QDialog (TSWizard) launched from
//  the New Entry menu. Both quick and wizard results route to
//  DatasheetEntryWidget (index 4) for final review and save.
// ─────────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void showDatabase();
    void showDetail(int id);
    void showQuickMeasurement();
    void showDatasheetEntry();
    void showEnclosureModel();
    void launchWizard();
    void onRecordReady(DriverRecord record);
    void onDriverSaved(int id);
    void onEditRequested(int id);
    void onDeleteRequested(int id);
    void showAbout();
    void onUseDriverRequested(int id);
    void showAdvancedSettings();

private:
    void buildSidebar();
    void buildCentralWidget();
    void updateStatusBar();
    void toggleSidebar();
    void applySidebarState();

    DriverDatabase       *m_db            = nullptr;
    QStackedWidget       *m_stack         = nullptr;
    DriverListWidget     *m_listWidget    = nullptr;
    DriverDetailWidget   *m_detailWidget  = nullptr;
    QuickEntryWidget     *m_quickEntry    = nullptr;
    DatasheetEntryWidget *m_datasheetEntry= nullptr;
    EnclosureWidget      *m_enclosure     = nullptr;
    QLabel               *m_dbStatusLbl   = nullptr;

    // Sidebar collapse state
    QSplitter    *m_splitter         = nullptr;
    QWidget      *m_sidebar          = nullptr;
    QPushButton  *m_appTitle         = nullptr;
    QLabel       *m_appSub           = nullptr;
    bool          m_sidebarExpanded  = true;

    struct NavEntry { QPushButton *btn; QString icon; QString label; };
    QVector<NavEntry> m_navEntries;
};
