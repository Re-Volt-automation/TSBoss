#include "mainwindow.h"
#include "driverlistwidget.h"
#include "driverdetailwidget.h"
#include "quickentrywidget.h"
#include "datasheetentrywidget.h"
#include "enclosurewidget.h"
#include "tswizard.h"
#include "theme.h"
#include <QApplication>
#include <QButtonGroup>
#include <QRadioButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QMenu>
#include <QFrame>
#include <QStatusBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QCheckBox>
#include <QCloseEvent>
#include <QSettings>
#include <QShortcut>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QTabWidget>
#include <QTextBrowser>
#include <cmath>

static constexpr int kSidebarExpanded  = 210;
static constexpr int kSidebarCollapsed =  48;

// ── Monochrome line icons for the sidebar nav ─────────────────────
// Painted at 2× and packed into one QIcon: Off state = normal colour,
// On state = accent (used by the checked/current-page button).
static QPixmap navPixmap(const QString &kind, const QColor &c)
{
    constexpr int   S   = 36;
    constexpr qreal DPR = 2.0;
    QPixmap px(S, S);
    px.setDevicePixelRatio(DPR);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(c, 1.5);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    if (kind == "box") {                       // speaker cabinet
        p.drawRoundedRect(QRectF(3.0, 3.0, 12.0, 12.0), 1.5, 1.5);
        p.drawEllipse(QPointF(9.0, 9.7), 3.0, 3.0);
        p.setBrush(c);
        p.drawEllipse(QPointF(9.0, 9.7), 0.9, 0.9);
    } else if (kind == "db") {                 // database cylinder
        p.drawEllipse(QRectF(4.0, 2.5, 10.0, 4.0));
        p.drawLine(QPointF(4.0, 4.5),  QPointF(4.0, 13.5));
        p.drawLine(QPointF(14.0, 4.5), QPointF(14.0, 13.5));
        p.drawArc(QRectF(4.0, 11.5, 10.0, 4.0), 180*16, 180*16);
        p.drawArc(QRectF(4.0, 7.0,  10.0, 4.0), 180*16, 180*16);
    } else if (kind == "pencil") {             // new entry
        p.drawLine(QPointF(5.0, 13.0), QPointF(11.6, 6.4));   // body, upper edge
        p.drawLine(QPointF(7.0, 15.0), QPointF(13.6, 8.4));   // body, lower edge
        p.drawLine(QPointF(11.6, 6.4), QPointF(13.6, 8.4));   // back end
        p.drawLine(QPointF(5.0, 13.0), QPointF(4.2, 15.8));   // tip
        p.drawLine(QPointF(4.2, 15.8), QPointF(7.0, 15.0));
    } else if (kind == "gear") {               // settings
        p.drawEllipse(QPointF(9.0, 9.0), 3.6, 3.6);
        p.setBrush(c);
        p.drawEllipse(QPointF(9.0, 9.0), 1.2, 1.2);
        p.setBrush(Qt::NoBrush);
        constexpr double kPi = 3.14159265358979323846;
        for (int i = 0; i < 8; ++i) {
            const double a = i * kPi / 4.0;
            p.drawLine(QPointF(9.0 + 4.4*std::cos(a), 9.0 + 4.4*std::sin(a)),
                       QPointF(9.0 + 6.2*std::cos(a), 9.0 + 6.2*std::sin(a)));
        }
    } else if (kind == "info") {               // about
        p.drawEllipse(QPointF(9.0, 9.0), 6.3, 6.3);
        p.setBrush(c);
        p.drawEllipse(QPointF(9.0, 5.9), 0.9, 0.9);
        p.setBrush(Qt::NoBrush);
        pen.setWidthF(1.8);
        p.setPen(pen);
        p.drawLine(QPointF(9.0, 8.3), QPointF(9.0, 12.4));
    }
    return px;
}

static QIcon navIcon(const QString &kind, const QColor &normal, const QColor &checked)
{
    QIcon ic;
    ic.addPixmap(navPixmap(kind, normal),  QIcon::Normal, QIcon::Off);
    ic.addPixmap(navPixmap(kind, checked), QIcon::Normal, QIcon::On);
    return ic;
}

// ─────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setWindowTitle("TSBoss – Thiele/Small Parameter Manager");
    setMinimumSize(1280, 800);
    resize(1440, 900);

    m_db = new DriverDatabase;
    if (!m_db->open()) {
        QMessageBox::critical(nullptr, "Database error",
            "Cannot open driver database:\n" + m_db->lastError());
    }

    buildCentralWidget();
    buildSidebar();
    buildShortcuts();
    updateStatusBar();
    restoreWindowState();
    applyStoredEnvironment();
}

MainWindow::~MainWindow()
{
    m_db->close();
}

void MainWindow::restoreWindowState()
{
    QSettings s;
    restoreGeometry(s.value("ui/mainGeometry").toByteArray());
    if (!s.value("ui/sidebarExpanded", true).toBool()) {
        m_sidebarExpanded = false;
        m_sidebar->setFixedWidth(kSidebarCollapsed);
        applySidebarState();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings s;
    s.setValue("ui/mainGeometry",    saveGeometry());
    s.setValue("ui/sidebarExpanded", m_sidebarExpanded);
    QMainWindow::closeEvent(event);
}

void MainWindow::buildShortcuts()
{
    auto add = [this](const QKeySequence &seq, auto slot) {
        auto *sc = new QShortcut(seq, this);
        connect(sc, &QShortcut::activated, this, slot);
    };
    add(QKeySequence("Ctrl+1"), &MainWindow::showEnclosureModel);
    add(QKeySequence("Ctrl+2"), &MainWindow::showDatabase);
    add(QKeySequence("Ctrl+3"), &MainWindow::showQuickMeasurement);
    add(QKeySequence("Ctrl+4"), &MainWindow::showDatasheetEntry);
    add(QKeySequence("Ctrl+5"), &MainWindow::launchWizard);
    add(QKeySequence("Ctrl+,"), &MainWindow::showAdvancedSettings);
    add(QKeySequence::HelpContents, &MainWindow::showAbout);   // F1
}

void MainWindow::buildCentralWidget()
{
    m_stack = new QStackedWidget;

    // 0 – enclosure model (default / landing page)
    m_enclosure = new EnclosureWidget(m_db);
    m_stack->addWidget(m_enclosure);
    connect(m_enclosure, &EnclosureWidget::editDriverRequested, this, &MainWindow::onEditRequested);

    // 1 – driver list
    m_listWidget = new DriverListWidget(m_db);
    m_stack->addWidget(m_listWidget);
    connect(m_listWidget, &DriverListWidget::driverSelected, this, &MainWindow::showDetail);
    connect(m_listWidget, &DriverListWidget::editRequested,  this, &MainWindow::onEditRequested);
    connect(m_listWidget, &DriverListWidget::deleteRequested,this, [this](int){ m_listWidget->refresh(); });
    connect(m_listWidget, &DriverListWidget::useRequested,   this, &MainWindow::onUseDriverRequested);

    // 2 – driver detail
    m_detailWidget = new DriverDetailWidget;
    m_stack->addWidget(m_detailWidget);
    connect(m_detailWidget, &DriverDetailWidget::editRequested,   this, &MainWindow::onEditRequested);
    connect(m_detailWidget, &DriverDetailWidget::deleteRequested, this, &MainWindow::onDeleteRequested);
    connect(m_detailWidget, &DriverDetailWidget::useRequested,    this, &MainWindow::onUseDriverRequested);

    // 3 – quick measurement (input only, routes to datasheet entry on Calculate)
    m_quickEntry = new QuickEntryWidget(m_db);
    m_stack->addWidget(m_quickEntry);
    connect(m_quickEntry, &QuickEntryWidget::recordCalculated,
            this, &MainWindow::onRecordReady);

    // 4 – datasheet entry (manual edit + save; receives results from quick/wizard)
    m_datasheetEntry = new DatasheetEntryWidget(m_db);
    m_stack->addWidget(m_datasheetEntry);
    connect(m_datasheetEntry, &DatasheetEntryWidget::driverSaved, this, &MainWindow::onDriverSaved);

    // Start on enclosure model
    m_stack->setCurrentIndex(0);
}

void MainWindow::buildSidebar()
{
    m_sidebar = new QWidget;
    m_sidebar->setObjectName("sidebar");
    m_sidebar->setFixedWidth(kSidebarExpanded);

    auto *vb = new QVBoxLayout(m_sidebar);
    vb->setContentsMargins(0, 0, 0, 0);
    vb->setSpacing(0);

    // ── Title acts as collapse toggle ─────────────────────────────
    m_appTitle = new QPushButton;
    m_appTitle->setFlat(true);
    m_appTitle->setCursor(Qt::PointingHandCursor);
    // Rich text on QPushButton requires a QLabel trick — use a child label
    {
        auto *inner = new QLabel(m_appTitle);
        inner->setObjectName("appTitleInner");
        inner->setTextFormat(Qt::RichText);
        inner->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto *titleLayout = new QHBoxLayout(m_appTitle);
        titleLayout->setContentsMargins(14, 20, 4, 6);
        titleLayout->addWidget(inner);
    }
    connect(m_appTitle, &QPushButton::clicked, this, &MainWindow::toggleSidebar);
    vb->addWidget(m_appTitle);

    m_appSub = new QLabel("  T/S Parameter Tool");
    m_appSub->setObjectName("appSub");
    vb->addWidget(m_appSub);

    auto *sep1 = new QFrame;
    sep1->setObjectName("sidebarSep");
    sep1->setFrameShape(QFrame::HLine);
    vb->addWidget(sep1);

    // ── Nav button helpers ────────────────────────────────────────
    auto makeNavBtn = [&](const QString &iconKind, const QString &label) -> QPushButton * {
        auto *b = new QPushButton(label);
        b->setObjectName("sidebarNav");
        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        vb->addWidget(b);
        m_navEntries.push_back({b, iconKind, label});
        return b;
    };

    auto *btnEnclosure = makeNavBtn("box", "Enclosure Model");
    auto *btnDb        = makeNavBtn("db",  "Driver Database");
    btnEnclosure->setToolTip("Enclosure Model  (Ctrl+1)");
    btnDb->setToolTip("Driver Database  (Ctrl+2)");

    // ── New Entry dropdown ────────────────────────────────────────
    auto *btnNewEntry = new QPushButton("New Entry  ▾");
    btnNewEntry->setObjectName("sidebarNav");
    btnNewEntry->setFlat(true);
    btnNewEntry->setCursor(Qt::PointingHandCursor);
    vb->addWidget(btnNewEntry);
    m_navEntries.push_back({btnNewEntry, "pencil", "New Entry"});

    // The current page is shown as a checked nav button; checking is
    // driven exclusively by updateActiveNav(), never by the click itself.
    for (auto *b : {btnEnclosure, btnDb, btnNewEntry}) {
        b->setCheckable(true);
        b->setAutoExclusive(false);
    }

    auto *newEntryMenu = new QMenu(this);
    newEntryMenu->setObjectName("sidebarMenu");
    // Shortcut hints after \t are display-only; the QShortcuts in
    // buildShortcuts() do the actual triggering.
    auto *actQuick  = newEntryMenu->addAction("⚡  Quick Measurement\tCtrl+3");
    auto *actWizard = newEntryMenu->addAction("🔬  Guided Wizard\tCtrl+5");
    newEntryMenu->addSeparator();
    auto *actManual = newEntryMenu->addAction("📋  Manual / Datasheet\tCtrl+4");
    connect(btnNewEntry, &QPushButton::clicked, this, [=]() {
        newEntryMenu->exec(btnNewEntry->mapToGlobal(QPoint(0, btnNewEntry->height())));
        updateActiveNav();   // undo the click's toggle if the menu was dismissed
    });
    connect(actQuick,  &QAction::triggered, this, &MainWindow::showQuickMeasurement);
    connect(actWizard, &QAction::triggered, this, &MainWindow::launchWizard);
    connect(actManual, &QAction::triggered, this, &MainWindow::showDatasheetEntry);

    auto *sep2 = new QFrame;
    sep2->setObjectName("sidebarSep");
    sep2->setFrameShape(QFrame::HLine);
    vb->addWidget(sep2);

    auto *btnAdvanced = makeNavBtn("gear", "Advanced Settings");
    auto *btnAbout    = makeNavBtn("info", "About / Method");
    btnAdvanced->setToolTip("Advanced Settings  (Ctrl+,)");
    btnAbout->setToolTip("About / Method  (F1)");

    vb->addStretch();

    m_dbStatusLbl = new QLabel;
    m_dbStatusLbl->setObjectName("appSub");
    m_dbStatusLbl->setWordWrap(true);
    vb->addWidget(m_dbStatusLbl);

    connect(btnEnclosure, &QPushButton::clicked, this, &MainWindow::showEnclosureModel);
    connect(btnDb,        &QPushButton::clicked, this, &MainWindow::showDatabase);
    connect(btnAdvanced,  &QPushButton::clicked, this, &MainWindow::showAdvancedSettings);
    connect(btnAbout,     &QPushButton::clicked, this, &MainWindow::showAbout);
    // Re-assert the checked state — a click on the already-current button
    // would otherwise just toggle it off (the stack index doesn't change).
    connect(btnEnclosure, &QPushButton::clicked, this, &MainWindow::updateActiveNav);
    connect(btnDb,        &QPushButton::clicked, this, &MainWindow::updateActiveNav);

    // Apply theme-aware styles
    restyleSidebar();

    // Main splitter
    m_splitter = new QSplitter(Qt::Horizontal);
    m_splitter->setHandleWidth(1);
    m_splitter->addWidget(m_sidebar);
    m_splitter->addWidget(m_stack);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setCollapsible(0, false);
    m_splitter->setCollapsible(1, false);

    setCentralWidget(m_splitter);

    // Highlight the nav button for whichever page the stack shows.
    connect(m_stack, &QStackedWidget::currentChanged,
            this, [this](int) { updateActiveNav(); });
    updateActiveNav();

    // Live theme switching: re-style sidebar widgets so they match.
    connect(&Theme::instance(), &Theme::themeChanged, this, &MainWindow::restyleSidebar);
}

void MainWindow::restyleSidebar()
{
    if (!m_sidebar) return;
    const auto &t = Theme::instance();

    m_sidebar->setStyleSheet(
        QString("#sidebar { background-color: %1; }").arg(hex(t.sidebarBg())));

    if (auto *inner = m_appTitle ? m_appTitle->findChild<QLabel*>("appTitleInner") : nullptr) {
        inner->setStyleSheet(
            QString("background:transparent; font-size:20pt; font-weight:bold; letter-spacing:-1px;"));
    }

    if (m_appSub) {
        m_appSub->setStyleSheet(
            QString("color:%1; font-size:8pt; padding:0 14px 18px 14px;")
                .arg(hex(t.textMuted())));
    }
    if (m_dbStatusLbl) {
        m_dbStatusLbl->setStyleSheet(
            QString("color:%1; font-size:8pt; padding:10px 14px;")
                .arg(hex(t.textMuted())));
    }

    // Separator hairlines — keep crisp against the sidebar bg
    for (auto *sep : m_sidebar->findChildren<QFrame*>("sidebarSep"))
        sep->setStyleSheet(
            QString("color:%1; background:%1; max-height:1px;").arg(hex(t.border())));

    // Re-apply per-state nav button styling (handles expanded/collapsed too)
    applySidebarState();
}

void MainWindow::toggleSidebar()
{
    const bool expanding = !m_sidebarExpanded;
    m_sidebarExpanded = expanding;

    // On collapse: update labels immediately so they vanish as sidebar shrinks
    if (!expanding) {
        applySidebarState();
    }

    const int fromW = m_sidebar->width();
    const int toW   = expanding ? kSidebarExpanded : kSidebarCollapsed;

    // Lock sidebar min/max so the splitter handle can't be dragged during animation
    m_sidebar->setFixedWidth(toW);

    auto *anim = new QVariantAnimation(this);
    anim->setStartValue(fromW);
    anim->setEndValue(toW);
    anim->setDuration(180);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &v) {
        const int sw = v.toInt();
        const int total = m_splitter->width() - m_splitter->handleWidth();
        m_splitter->setSizes({sw, total - sw});
    });
    connect(anim, &QVariantAnimation::finished, this, [this, expanding]() {
        if (expanding) applySidebarState();
    });
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::applySidebarState()
{
    const bool exp = m_sidebarExpanded;
    const auto &t = Theme::instance();

    // Logo wordmark — bold amber accent + softened cream/ink for body
    const QString accentHex = hex(t.accent());
    const QString softHex   = hex(t.textSecondary());
    const QString hoverHex  = hex(t.mode() == Theme::Mode::Light ? t.sunkenBg() : t.panelBg());

    if (m_appTitle) {
        auto *inner = m_appTitle->findChild<QLabel*>("appTitleInner");
        if (inner) {
            if (exp)
                inner->setText(
                    QString("<span style='color:%1;'>TS</span>"
                            "<span style='color:%2;'>Boss</span>")
                        .arg(accentHex, softHex));
            else
                inner->setText(
                    QString("<center><span style='color:%1;'>TS</span></center>")
                        .arg(accentHex));
        }
        m_appTitle->setStyleSheet(exp
            ? QString(
              "QPushButton { background:transparent; border:none; text-align:left;"
              "  font-size:20pt; font-weight:bold;"
              "  padding:20px 4px 6px 14px; letter-spacing:-1px; }"
              "QPushButton:hover { background:%1; }").arg(hoverHex)
            : QString(
              "QPushButton { background:transparent; border:none;"
              "  padding:14px 0 6px 0; }"
              "QPushButton:hover { background:%1; }").arg(hoverHex));
    }
    if (m_appSub)      m_appSub->setVisible(exp);
    if (m_dbStatusLbl) m_dbStatusLbl->setVisible(exp);

    // Nav button styling — amber-on-hover, accent-on-press, and a left
    // accent bar + bold text marking the current page (checked state).
    const QString fgHex      = hex(t.textPrimary());
    const QString accentBg   = hex(t.accent());
    const QString textOnAccentHex = hex(t.textOnAccent());

    const QString expStyle = QString(
        "QPushButton { background:transparent; color:%1; border:none;"
        "  border-left:3px solid transparent;"
        "  text-align:left; padding:13px 17px; font-size:10pt; }"
        "QPushButton:hover { background:%2; color:%3; }"
        "QPushButton:pressed { background:%4; color:%5; }"
        "QPushButton:checked { background:%2; color:%3;"
        "  border-left:3px solid %3; font-weight:bold; }")
        .arg(fgHex, hoverHex, accentHex, accentBg, textOnAccentHex);
    const QString colStyle = QString(
        "QPushButton { background:transparent; color:%1; border:none;"
        "  border-left:3px solid transparent;"
        "  text-align:center; padding:12px 1px; font-size:14pt; }"
        "QPushButton:hover { background:%2; color:%3; }"
        "QPushButton:pressed { background:%4; color:%5; }"
        "QPushButton:checked { background:%2;"
        "  border-left:3px solid %3; }")
        .arg(fgHex, hoverHex, accentHex, accentBg, textOnAccentHex);

    const QColor iconClr = t.textPrimary();
    const QColor iconOn  = t.accent();
    for (auto &e : m_navEntries) {
        e.btn->setStyleSheet(exp ? expStyle : colStyle);
        e.btn->setIcon(navIcon(e.icon, iconClr, iconOn));
        e.btn->setIconSize(QSize(18, 18));
        if (e.label == "New Entry")
            e.btn->setText(exp ? e.label + "  ▾" : QString());
        else
            e.btn->setText(exp ? e.label : QString());
    }
}

void MainWindow::updateActiveNav()
{
    if (m_navEntries.size() < 3 || !m_stack) return;
    const int page = m_stack->currentIndex();
    // 0 = enclosure; 1,2 = database (list + detail); 3,4 = entry forms
    m_navEntries[0].btn->setChecked(page == 0);
    m_navEntries[1].btn->setChecked(page == 1 || page == 2);
    m_navEntries[2].btn->setChecked(page == 3 || page == 4);
}

void MainWindow::updateStatusBar()
{
    const int n = m_db->isOpen() ? m_db->driverCount() : 0;
    if (m_dbStatusLbl)
        m_dbStatusLbl->setText(
            QString("Database: %1 driver%2\n%3")
            .arg(n).arg(n==1?"":"s")
            .arg(m_db->path().section('/', -1)));
    statusBar()->showMessage(
        m_db->isOpen()
        ? QString("Database open: %1  (%2 drivers)").arg(m_db->path()).arg(n)
        : "Database not open");
}

// ─────────────────────────────────────────────────────────────────
//  Navigation slots
// ─────────────────────────────────────────────────────────────────

void MainWindow::showEnclosureModel()
{
    m_enclosure->refreshDriverList();
    m_stack->setCurrentIndex(0);
    m_enclosure->setFocus();      // arm the page's Ctrl+S / Ctrl+O shortcuts
}

void MainWindow::showDatabase()
{
    m_listWidget->refresh();
    m_stack->setCurrentIndex(1);
    m_listWidget->setFocus();     // arm the page's Ctrl+F shortcut
}

void MainWindow::showDetail(int id)
{
    if (!m_db->isOpen()) return;
    const auto r = m_db->loadDriver(id);
    if (!r.isValid()) return;
    m_detailWidget->loadDriver(r);
    m_stack->setCurrentIndex(2);
}

void MainWindow::showQuickMeasurement()
{
    m_quickEntry->clear();
    m_stack->setCurrentIndex(3);
}

void MainWindow::showDatasheetEntry()
{
    m_datasheetEntry->clear();
    m_stack->setCurrentIndex(4);
}

void MainWindow::launchWizard()
{
    auto *wiz = new TSWizard(this);
    connect(wiz, &TSWizard::recordReady, this, &MainWindow::onRecordReady);
    wiz->exec();
    wiz->deleteLater();
}

// ─────────────────────────────────────────────────────────────────
//  Record routing: quick or wizard result → DatasheetEntry
// ─────────────────────────────────────────────────────────────────

void MainWindow::onRecordReady(DriverRecord record)
{
    // Mark as new (unsaved)
    record.id = -1;
    m_datasheetEntry->loadRecord(record);
    m_stack->setCurrentIndex(4);
}

// ─────────────────────────────────────────────────────────────────
//  Post-save / edit / delete
// ─────────────────────────────────────────────────────────────────

void MainWindow::onDriverSaved(int id)
{
    updateStatusBar();
    m_enclosure->refreshDriverList();
    if (id >= 0) showDetail(id);
}

void MainWindow::onEditRequested(int id)
{
    const auto r = m_db->loadDriver(id);
    if (!r.isValid()) return;
    m_datasheetEntry->loadRecord(r);
    m_stack->setCurrentIndex(4);
}

void MainWindow::onDeleteRequested(int id)
{
    const auto r = m_db->loadDriver(id);
    const auto ans = QMessageBox::question(this, "Delete driver",
        QString("Permanently delete  %1 %2?").arg(r.make, r.model));
    if (ans == QMessageBox::Yes) {
        m_db->deleteDriver(id);
        m_detailWidget->clear();
        m_listWidget->refresh();
        m_stack->setCurrentIndex(1);
        updateStatusBar();
    }
}

void MainWindow::onUseDriverRequested(int id)
{
    m_enclosure->refreshDriverList();
    m_enclosure->addDriverModel(id);
    m_stack->setCurrentIndex(0);
}

// ── Speed of sound from environmental conditions (ISO 9613-1 / Cramer) ──────
static double calcSpeedOfSound(double T_C, double RH_pct, double P_hPa)
{
    const double P_Pa = P_hPa * 100.0;
    // Magnus saturation vapor pressure
    const double Psat = 611.657 * std::exp(17.502 * T_C / (240.97 + T_C));
    // Mole fraction of water vapor in air
    const double xw   = (RH_pct / 100.0) * Psat / P_Pa;
    // Speed of sound (temperature + humidity; pressure has negligible effect below ±10%)
    return 331.3 * std::sqrt((273.15 + T_C) / 273.15) * std::sqrt(1.0 + 0.0320 * xw);
}

// Re-apply persisted air conditions and plot ranges at startup, so the
// Advanced Settings survive a restart instead of silently resetting.
void MainWindow::applyStoredEnvironment()
{
    QSettings s;
    const double T  = s.value("env/temperatureC", 20.0).toDouble();
    const double RH = s.value("env/humidityPct",  50.0).toDouble();
    const double P  = s.value("env/pressureHPa",  1013.25).toDouble();
    m_enclosure->setSpeedOfSound(calcSpeedOfSound(T, RH, P));

    PlotRangeSettings r;
    auto load = [&s](const char *kMin, const char *kMax,
                     std::optional<double> &mn, std::optional<double> &mx) {
        if (s.contains(kMin) && s.contains(kMax)) {
            mn = s.value(kMin).toDouble();
            mx = s.value(kMax).toDouble();
        }
    };
    load("plot/splMin",  "plot/splMax",  r.splMin,  r.splMax);
    load("plot/gdMin",   "plot/gdMax",   r.gdMin,   r.gdMax);
    load("plot/voltMin", "plot/voltMax", r.voltMin, r.voltMax);
    load("plot/excMin",  "plot/excMax",  r.excMin,  r.excMax);
    m_enclosure->setPlotRanges(r);
}

void MainWindow::showAdvancedSettings()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("Advanced Settings");
    dlg->setMinimumWidth(420);

    auto *vb = new QVBoxLayout(dlg);

    // ── Appearance / Theme ────────────────────────────────────────
    auto *grpTheme = new QGroupBox("Appearance");
    auto *themeForm = new QFormLayout(grpTheme);
    themeForm->setSpacing(8);

    auto *themeRow   = new QWidget;
    auto *themeHb    = new QHBoxLayout(themeRow);
    themeHb->setContentsMargins(0,0,0,0); themeHb->setSpacing(16);
    auto *rbDark     = new QRadioButton("Dark");
    auto *rbLight    = new QRadioButton("Light");
    auto *themeGroup = new QButtonGroup(dlg);
    themeGroup->addButton(rbDark,  0);
    themeGroup->addButton(rbLight, 1);
    (Theme::instance().mode() == Theme::Mode::Light ? rbLight : rbDark)->setChecked(true);
    themeHb->addWidget(rbDark);
    themeHb->addWidget(rbLight);
    themeHb->addStretch();
    themeForm->addRow("Theme:", themeRow);

    auto *themeNote = new QLabel(
        "The theme switches live across all pages.");
    themeNote->setObjectName("FormCaption");
    themeNote->setWordWrap(true);
    themeForm->addRow("", themeNote);

    // Apply theme live as the radio changes — gives instant preview
    connect(themeGroup, &QButtonGroup::idClicked, this, [](int id) {
        Theme::instance().setMode(id == 1 ? Theme::Mode::Light : Theme::Mode::Dark);
    });
    vb->addWidget(grpTheme);

    // ── Environmental conditions group ────────────────────────────
    auto *grp = new QGroupBox("Air Conditions");
    auto *form = new QFormLayout(grp);
    form->setSpacing(8);

    auto mkSpin = [](double lo, double hi, double step, double def,
                     int dec, const QString &sfx) {
        auto *s = new QDoubleSpinBox;
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setDecimals(dec);
        s->setValue(def);
        s->setSuffix(sfx);
        s->setFixedWidth(130);
        return s;
    };

    // Initialise from the persisted values, not hard-coded defaults —
    // otherwise every OK silently reverts a previously chosen climate.
    QSettings settings;
    auto *spTemp = mkSpin(-40.0, 60.0, 0.5,
                          settings.value("env/temperatureC", 20.0).toDouble(), 1, " °C");
    auto *spHum  = mkSpin(  0.0,100.0, 1.0,
                          settings.value("env/humidityPct", 50.0).toDouble(), 0, " %");
    auto *spPres = mkSpin(800.0,1100.0,1.0,
                          settings.value("env/pressureHPa", 1013.25).toDouble(), 2, " hPa");

    form->addRow("Temperature:",        spTemp);
    form->addRow("Relative humidity:",  spHum);
    form->addRow("Air pressure:",       spPres);

    // ── Computed speed of sound (read-only) ───────────────────────
    auto *cLbl = new QLabel;
    cLbl->setObjectName("ResultValue");
    form->addRow("Speed of sound:", cLbl);

    auto updateC = [=]{
        const double c = calcSpeedOfSound(spTemp->value(), spHum->value(), spPres->value());
        cLbl->setText(QString("%1 m/s").arg(c, 0, 'f', 2));
    };
    updateC();

    connect(spTemp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double){ updateC(); });
    connect(spHum,  QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double){ updateC(); });
    connect(spPres, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [=](double){ updateC(); });

    vb->addWidget(grp);

    // ── Note ──────────────────────────────────────────────────────
    auto *note = new QLabel(
        "Affects all vented-box enclosure calculations.\n"
        "Formula: ISO 9613-1 / Cramer (1993).");
    note->setObjectName("FormCaption");
    note->setWordWrap(true);
    vb->addWidget(note);

    // ── Plot Y-axis ranges ────────────────────────────────────────
    auto *grpPlot = new QGroupBox("Plot Y-axis Ranges");
    auto *plotForm = new QFormLayout(grpPlot);
    plotForm->setSpacing(6);

    const PlotRangeSettings cur = m_enclosure->plotRanges();

    // Helper: one row = [checkbox "Fixed"] [min spin] [–] [max spin] [unit]
    struct RangeRow {
        QCheckBox      *chk;
        QDoubleSpinBox *spMin, *spMax;
    };

    auto mkRangeRow = [&](const QString &label, const QString &unit,
                           double loMin, double hiMax, int dec,
                           std::optional<double> curMin,
                           std::optional<double> curMax) -> RangeRow
    {
        auto *chk   = new QCheckBox("Fixed");
        auto *spMin = new QDoubleSpinBox;
        auto *spMax = new QDoubleSpinBox;
        for (auto *s : {spMin, spMax}) {
            s->setRange(loMin, hiMax);
            s->setDecimals(dec);
            s->setFixedWidth(80);
        }
        spMin->setSuffix(" " + unit);
        spMax->setSuffix(" " + unit);

        const bool fixed = curMin.has_value() && curMax.has_value();
        chk->setChecked(fixed);
        spMin->setValue(fixed ? *curMin : loMin);
        spMax->setValue(fixed ? *curMax : hiMax);
        spMin->setEnabled(fixed);
        spMax->setEnabled(fixed);

        QObject::connect(chk, &QCheckBox::toggled, spMin, &QWidget::setEnabled);
        QObject::connect(chk, &QCheckBox::toggled, spMax, &QWidget::setEnabled);

        auto *row = new QWidget;
        auto *h   = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0); h->setSpacing(4);
        h->addWidget(chk);
        h->addStretch();
        h->addWidget(new QLabel("Min:")); h->addWidget(spMin);
        h->addWidget(new QLabel("Max:")); h->addWidget(spMax);
        plotForm->addRow(label, row);
        return {chk, spMin, spMax};
    };

    auto rowSpl  = mkRangeRow("SPL Response:", "dB", -40.0, 140.0, 0, cur.splMin,  cur.splMax);
    auto rowGd   = mkRangeRow("Group Delay:",  "ms",   0.0, 500.0, 0, cur.gdMin,   cur.gdMax);
    auto rowVolt = mkRangeRow("Voltage:",        "V",   0.0, 500.0, 0, cur.voltMin, cur.voltMax);
    auto rowExc  = mkRangeRow("Excursion:",     "mm",   0.0, 200.0, 1, cur.excMin,  cur.excMax);

    vb->addWidget(grpPlot);

    // ── Buttons ───────────────────────────────────────────────────
    auto *btnReset = new QPushButton("Reset to defaults");
    connect(btnReset, &QPushButton::clicked, this, [=]{
        spTemp->setValue(20.0);
        spHum->setValue(50.0);
        spPres->setValue(1013.25);
        for (auto *row : {&rowSpl, &rowGd, &rowVolt, &rowExc})
            row->chk->setChecked(false);
    });

    auto *bbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bbox->addButton(btnReset, QDialogButtonBox::ResetRole);
    connect(bbox, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    vb->addWidget(bbox);

    // Remember the theme on entry so we can roll back on Cancel.
    const Theme::Mode initialThemeMode = Theme::instance().mode();

    if (dlg->exec() == QDialog::Accepted) {
        // Theme already applied live; ensure it is persisted to settings.
        Theme::instance().saveToSettings();
        settings.setValue("env/temperatureC", spTemp->value());
        settings.setValue("env/humidityPct",  spHum->value());
        settings.setValue("env/pressureHPa",  spPres->value());
        const double c = calcSpeedOfSound(spTemp->value(), spHum->value(), spPres->value());
        m_enclosure->setSpeedOfSound(c);
        statusBar()->showMessage(
            QString("Speed of sound updated to %1 m/s  (T=%2°C, RH=%3%, P=%4 hPa)")
            .arg(c, 0, 'f', 2)
            .arg(spTemp->value(), 0, 'f', 1)
            .arg(spHum->value(), 0, 'f', 0)
            .arg(spPres->value(), 0, 'f', 1),
            8000);

        PlotRangeSettings r;
        auto applyRow = [&settings](const RangeRow &row,
                           const char *kMin, const char *kMax,
                           std::optional<double> &mn, std::optional<double> &mx) {
            if (row.chk->isChecked()) {
                mn = row.spMin->value();
                mx = row.spMax->value();
                settings.setValue(kMin, *mn);
                settings.setValue(kMax, *mx);
            } else {
                settings.remove(kMin);
                settings.remove(kMax);
            }
        };
        applyRow(rowSpl,  "plot/splMin",  "plot/splMax",  r.splMin,  r.splMax);
        applyRow(rowGd,   "plot/gdMin",   "plot/gdMax",   r.gdMin,   r.gdMax);
        applyRow(rowVolt, "plot/voltMin", "plot/voltMax", r.voltMin, r.voltMax);
        applyRow(rowExc,  "plot/excMin",  "plot/excMax",  r.excMin,  r.excMax);
        m_enclosure->setPlotRanges(r);
    } else {
        // Cancel — undo any live theme preview.
        if (Theme::instance().mode() != initialThemeMode)
            Theme::instance().setMode(initialThemeMode);
    }
    dlg->deleteLater();
}

void MainWindow::showAbout()
{
    auto *dlg = new QDialog(this);
    dlg->setWindowTitle("TSBoss – About");
    dlg->resize(580, 540);
    auto *vb   = new QVBoxLayout(dlg);
    auto *tabs = new QTabWidget;

    auto mkTab = [&](const QString &title, const QString &html) {
        auto *tb = new QTextBrowser;
        tb->setOpenExternalLinks(true);
        tb->setFrameShape(QFrame::NoFrame);
        tb->setHtml(html);
        tabs->addTab(tb, title);
    };

    mkTab("About",
        "<h3>TSBoss</h3>"
        "<p>Loudspeaker Thiele/Small parameter management and enclosure-modelling tool.</p>"
        "<p><b>What it does:</b></p>"
        "<ul>"
        "<li><b>Driver database</b> &mdash; capture raw impedance and added-mass "
        "measurements, compute the full Thiele/Small parameter set "
        "(small- and large-signal), and store drivers in a local SQLite database "
        "with CSV import/export and side-by-side compare views.</li>"
        "<li><b>Measurement entry</b> &mdash; full datasheet form, a minimal "
        "quick-entry form, and a step-by-step wizard that walks through "
        "impedance sweep, &minus;3&nbsp;dB bandwidth extraction, and the "
        "delta-mass procedure.</li>"
        "<li><b>Enclosure modeller</b> &mdash; compare sealed, vented, "
        "infinite-baffle and bandpass designs simultaneously, with SPL, group "
        "delay, power, excursion, port-velocity, impedance and max-SPL plots, "
        "plus a port calculator for round and rectangular ports.</li>"
        "</ul>"
        "<hr>"
        "<p><b>Support the project:</b><br>"
        "TSBoss is a free, one-person side project. If it has been useful and "
        "you'd like to help keep it actively developed, a small donation of any "
        "amount is genuinely appreciated &mdash; there's no minimum.<br>"
        "<a href=\"https://www.paypal.com/ncp/payment/88KEBEEQCC6CQ\">Donate via PayPal</a></p>"
        "<p><b>Bugs &amp; feedback:</b><br>"
        "Found a bug, or have a feature request? Email "
        "<a href=\"mailto:wessellemmer@gmail.com\">wessellemmer@gmail.com</a>.</p>");

    mkTab("Method",
        "<p><b>Measurement method:</b><br>"
        "Delta-mass / constant-voltage impedance technique. Resonance and quality "
        "factors are extracted from the &minus;3&nbsp;dB bandwidth around f<sub>s</sub>; "
        "m<sub>ms</sub> is recovered from the resonance shift after adding a known "
        "mass to the cone; BL, C<sub>ms</sub>, V<sub>as</sub>, S<sub>d</sub> and "
        "L<sub>e</sub> then follow analytically.</p>"
        "<p><b>Formulae implemented:</b><br>"
        "Z₁₂ = √(Rₑ·Zmax)<br>"
        "R₀ = Zmax/Rₑ<br>"
        "Qms = fₛ/(f₂−f₁)·√R₀<br>"
        "Qes = Qms/(R₀−1)<br>"
        "Qts = Qms·Qes/(Qms+Qes)<br>"
        "mms = Δm/((fₛ/f₀)²−1)<br>"
        "Rms = 2π·fₛ·mms/Qms<br>"
        "BL  = √((Zmax−Rₑ)·Rms)<br>"
        "Cms = 1/((2π·fₛ)²·mms)<br>"
        "Sᵈ  = π·(Dᵈ/2)²<br>"
        "Vas = Cms·ρ·c²·Sᵈ²  (ρ=1.2 kg/m³, c=344 m/s)<br>"
        "Lₑ  = [(Rₑ·20000/(2π·f₃))+0.5]·10⁻³/20</p>"
        "<p><b>Vented-box acoustics:</b><br>"
        "Cone and port volume velocities are taken from the coupled "
        "mechanical/acoustical admittance circuit and summed coherently. "
        "f₃ is located by binary search on the resulting SPL curve; group "
        "delay is the numerical phase derivative of the total acoustic output.</p>"
        "<p><b>Database:</b> SQLite, stored in the application data directory; "
        "schema is migrated forward via additive ALTER&nbsp;TABLE steps.</p>");

    mkTab("Shortcuts",
        "<p><b>Global:</b></p>"
        "<p>Ctrl+1 &mdash; Enclosure model<br>"
        "Ctrl+2 &mdash; Driver database<br>"
        "Ctrl+3 &mdash; Quick measurement<br>"
        "Ctrl+4 &mdash; Datasheet entry<br>"
        "Ctrl+5 &mdash; Guided wizard<br>"
        "Ctrl+, &mdash; Advanced settings<br>"
        "F1 &mdash; About</p>"
        "<p><b>Enclosure page:</b></p>"
        "<p>Ctrl+S &mdash; save project<br>"
        "Ctrl+O &mdash; load project</p>"
        "<p><b>Database page:</b></p>"
        "<p>Ctrl+F &mdash; focus search<br>"
        "Ctrl+click &mdash; select multiple drivers (2&ndash;4) to compare</p>");

    vb->addWidget(tabs);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(bb, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    vb->addWidget(bb);
    dlg->exec();
    dlg->deleteLater();
}
