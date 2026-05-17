#include "theme.h"
#include <QApplication>
#include <QSettings>

Theme &Theme::instance() {
    static Theme t;
    return t;
}

Theme::Theme() { loadFromSettings(); }

void Theme::loadFromSettings() {
    QSettings s;
    const QString v = s.value("ui/themeMode", "dark").toString();
    m_mode = (v.compare("light", Qt::CaseInsensitive) == 0) ? Mode::Light : Mode::Dark;
}

void Theme::saveToSettings() const {
    QSettings s;
    s.setValue("ui/themeMode", m_mode == Mode::Light ? "light" : "dark");
}

void Theme::setMode(Mode m) {
    if (m == m_mode) return;
    m_mode = m;
    saveToSettings();
    emit themeChanged();
    if (auto *app = qobject_cast<QApplication *>(QApplication::instance()))
        app->setStyleSheet(globalStylesheet());
}

// ─── Dark palette ────────────────────────────────────────────────────
// Tektronix scope × RME interface — warm near-black, cream text.
static QColor d_pageBg()        { return QColor("#0E1116"); }
static QColor d_panelBg()       { return QColor("#161A21"); }
static QColor d_sunkenBg()      { return QColor("#14181F"); }
static QColor d_inputBg()       { return QColor("#1A1F28"); }
static QColor d_inputBgFocus()  { return QColor("#1E2530"); }
static QColor d_statusBarBg()   { return QColor("#0A0C10"); }
static QColor d_plotBg()        { return QColor("#14181F"); }
static QColor d_sidebarBg()     { return QColor("#0A0C10"); }
static QColor d_border()        { return QColor("#262C36"); }
static QColor d_borderStrong()  { return QColor("#3A4150"); }
static QColor d_gridLine()      { return QColor("#262C36"); }
static QColor d_textPrimary()   { return QColor("#E8E1D3"); }
static QColor d_textSecondary() { return QColor("#BAB3A4"); }
static QColor d_textMuted()     { return QColor("#8A8579"); }
static QColor d_textDisabled()  { return QColor("#5A5A5A"); }
static QColor d_textUnit()      { return QColor("#6E6960"); }
static QColor d_accent()        { return QColor("#D97706"); }
static QColor d_accentHover()   { return QColor("#F59E0B"); }
static QColor d_accentLight()   { return QColor("#F5C887"); }
static QColor d_accentPressed() { return QColor("#B45309"); }
static QColor d_plotDim()       { return QColor("#4A4F58"); }
static QColor d_cursorBg()      { return QColor("#1A1F28"); }
static QColor d_statusError()   { return QColor("#FCA5A5"); }
static QColor d_statusWarn()    { return QColor("#FDBA74"); }

// ─── Light palette ───────────────────────────────────────────────────
// Lab notebook on warm paper — deep ink, same amber accent.
// Background and panel surfaces tinted slightly warm so amber doesn't
// look out of place; borders and grid lines use beige-grey so the chart
// doesn't strobe.
static QColor l_pageBg()        { return QColor("#F4EFE3"); }  // warm paper
static QColor l_panelBg()       { return QColor("#FBF8EE"); }  // brighter card
static QColor l_sunkenBg()      { return QColor("#ECE5D2"); }  // inset
static QColor l_inputBg()       { return QColor("#FFFDF6"); }
static QColor l_inputBgFocus()  { return QColor("#FFFFFF"); }
static QColor l_statusBarBg()   { return QColor("#E8E0CB"); }
static QColor l_plotBg()        { return QColor("#FFFDF6"); }
static QColor l_sidebarBg()     { return QColor("#E8E0CB"); }
static QColor l_border()        { return QColor("#CFC6AE"); }
static QColor l_borderStrong()  { return QColor("#A89E83"); }
static QColor l_gridLine()      { return QColor("#E1D9C2"); }
static QColor l_textPrimary()   { return QColor("#1F1B14"); }  // deep warm ink
static QColor l_textSecondary() { return QColor("#5C5448"); }
static QColor l_textMuted()     { return QColor("#7C7460"); }
static QColor l_textDisabled()  { return QColor("#B5AC95"); }
static QColor l_textUnit()      { return QColor("#8C8270"); }
static QColor l_accent()        { return QColor("#B45309"); }  // deeper for contrast on paper
static QColor l_accentHover()   { return QColor("#D97706"); }
static QColor l_accentLight()   { return QColor("#8C3F06"); }
static QColor l_accentPressed() { return QColor("#7C3D08"); }
static QColor l_plotDim()       { return QColor("#A89E83"); }
static QColor l_cursorBg()      { return QColor("#FBF8EE"); }
static QColor l_statusError()   { return QColor("#B91C1C"); }
static QColor l_statusWarn()    { return QColor("#B45309"); }

// ─── Mode dispatch ───────────────────────────────────────────────────
#define DISPATCH(NAME) \
    QColor Theme::NAME() const { \
        return m_mode == Mode::Light ? l_##NAME() : d_##NAME(); \
    }

DISPATCH(pageBg)
DISPATCH(panelBg)
DISPATCH(sunkenBg)
DISPATCH(inputBg)
DISPATCH(inputBgFocus)
DISPATCH(statusBarBg)
DISPATCH(plotBg)
DISPATCH(sidebarBg)
DISPATCH(border)
DISPATCH(borderStrong)
DISPATCH(gridLine)
DISPATCH(textPrimary)
DISPATCH(textSecondary)
DISPATCH(textMuted)
DISPATCH(textDisabled)
DISPATCH(textUnit)
DISPATCH(accent)
DISPATCH(accentHover)
DISPATCH(accentLight)
DISPATCH(accentPressed)
DISPATCH(statusError)
DISPATCH(statusWarn)
#undef DISPATCH

QColor Theme::textOnAccent() const {
    return m_mode == Mode::Light ? QColor("#FBF8EE") : QColor("#0E1116");
}

QColor Theme::plotAxis() const { return textPrimary(); }
QColor Theme::plotTickLabel() const { return textMuted(); }
QColor Theme::plotDimLine() const {
    return m_mode == Mode::Light ? l_plotDim() : d_plotDim();
}
QColor Theme::plotCursorBg() const {
    return m_mode == Mode::Light ? l_cursorBg() : d_cursorBg();
}
QColor Theme::plotCursorBorder() const { return accent(); }

QList<QColor> Theme::curvePalette() const {
    if (m_mode == Mode::Light) {
        // Saturated, separable on cream paper. Darker than dark-mode
        // siblings so they punch through a bright background.
        return {
            QColor("#B45309"),  // burnt amber (primary)
            QColor("#0F766E"),  // deep teal
            QColor("#BE185D"),  // magenta
            QColor("#4D7C0F"),  // olive green
            QColor("#0369A1"),  // ocean blue
            QColor("#B91C1C"),  // brick red
            QColor("#6D28D9"),  // imperial purple
            QColor("#C2410C"),  // rust
        };
    }
    return {
        QColor("#F59E0B"),  // amber
        QColor("#5EEAD4"),  // phosphor cyan
        QColor("#F472B6"),  // rose
        QColor("#A3E635"),  // lime
        QColor("#7DD3FC"),  // sky
        QColor("#FCA5A5"),  // peach
        QColor("#C4B5FD"),  // lavender
        QColor("#FDBA74"),  // burnt orange
    };
}

QString Theme::globalStylesheet() const {
    return QString(R"(
        /* ── Surfaces ───────────────────────────────────────────── */
        QWidget {
            font-family: "IBM Plex Sans", "Helvetica Neue", "Liberation Sans", sans-serif;
            font-size: 10pt;
            color: %TEXT_PRIMARY%;
            background: transparent;
        }
        QMainWindow, QDialog {
            background: %PAGE_BG%;
        }
        QStackedWidget > QWidget,
        QSplitter, QScrollArea, QScrollArea > QWidget > QWidget {
            background: %PAGE_BG%;
        }

        /* ── Group boxes — hairline panels with serif titles ────── */
        QGroupBox {
            font-family: "IBM Plex Serif", "Liberation Serif", serif;
            font-size: 10.5pt;
            font-weight: 500;
            color: %TEXT_PRIMARY%;
            border: 1px solid %BORDER%;
            border-radius: 2px;
            margin-top: 18px;
            padding: 14px 10px 10px 10px;
            background: %PANEL_BG%;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 8px;
            color: %ACCENT%;
            letter-spacing: 1px;
            text-transform: uppercase;
            font-size: 8.5pt;
            font-family: "IBM Plex Sans", sans-serif;
            font-weight: 600;
            background: %PAGE_BG%;
        }
        QGroupBox QLabel,
        QFormLayout QLabel {
            color: %TEXT_SECONDARY%;
            background: transparent;
        }

        /* ── Inputs — flat with amber underline on focus ─────────── */
        QDoubleSpinBox, QSpinBox, QLineEdit, QDateEdit, QTextEdit, QComboBox {
            border: 1px solid %BORDER%;
            border-bottom: 1px solid %BORDER_STRONG%;
            border-radius: 0px;
            padding: 4px 8px;
            background: %INPUT_BG%;
            color: %TEXT_PRIMARY%;
            font-family: "IBM Plex Mono", "JetBrains Mono", "Fira Mono", monospace;
            font-size: 9.5pt;
            selection-background-color: %ACCENT%;
            selection-color: %TEXT_ON_ACCENT%;
        }
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button,
        QSpinBox::up-button,       QSpinBox::down-button {
            width: 0;
            border: none;
        }
        QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus,
        QDateEdit:focus, QTextEdit:focus, QComboBox:focus {
            border: 1px solid %BORDER%;
            border-bottom: 2px solid %ACCENT%;
            background: %INPUT_BG_FOCUS%;
            outline: none;
        }
        QDoubleSpinBox:disabled, QSpinBox:disabled, QLineEdit:disabled,
        QComboBox:disabled {
            color: %TEXT_DISABLED%;
            background: %SUNKEN_BG%;
            border-bottom: 1px solid %BORDER%;
        }
        QDoubleSpinBox:read-only, QSpinBox:read-only, QLineEdit:read-only {
            color: %TEXT_MUTED%;
            background: %SUNKEN_BG%;
        }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid %TEXT_MUTED%;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background: %PANEL_BG%;
            color: %TEXT_PRIMARY%;
            border: 1px solid %BORDER_STRONG%;
            selection-background-color: %ACCENT%;
            selection-color: %TEXT_ON_ACCENT%;
            outline: none;
        }

        /* ── Buttons — outline default, filled amber for primary ── */
        QPushButton {
            background: transparent;
            color: %TEXT_PRIMARY%;
            border: 1px solid %BORDER_STRONG%;
            border-radius: 0px;
            padding: 6px 18px;
            font-family: "IBM Plex Sans", sans-serif;
            font-size: 9pt;
            font-weight: 500;
            letter-spacing: 0.5px;
        }
        QPushButton:hover   { border-color: %ACCENT%; color: %ACCENT_LIGHT%; }
        QPushButton:pressed { background: %ACCENT%; color: %TEXT_ON_ACCENT%; }
        QPushButton:disabled{ color: %TEXT_DISABLED%; border-color: %BORDER%; }
        QPushButton:default {
            background: %ACCENT%;
            color: %TEXT_ON_ACCENT%;
            border-color: %ACCENT%;
        }
        QPushButton:default:hover  { background: %ACCENT_HOVER%; border-color: %ACCENT_HOVER%; }
        QPushButton:default:pressed{ background: %ACCENT_PRESSED%; border-color: %ACCENT_PRESSED%; }

        /* ── Tabs ───────────────────────────────────────────────── */
        QTabWidget::pane {
            border: 1px solid %BORDER%;
            background: %PANEL_BG%;
            top: -1px;
        }
        QTabBar::tab {
            background: %PAGE_BG%;
            color: %TEXT_MUTED%;
            border: none;
            border-bottom: 1px solid %BORDER%;
            padding: 8px 18px;
            font-family: "IBM Plex Mono", monospace;
            font-size: 8.5pt;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            min-width: 70px;
        }
        QTabBar::tab:hover    { color: %TEXT_PRIMARY%; }
        QTabBar::tab:selected {
            color: %TEXT_PRIMARY%;
            background: %PANEL_BG%;
            border-bottom: 2px solid %ACCENT%;
        }

        /* ── Splitter handles ───────────────────────────────────── */
        QSplitter::handle {
            background: %PAGE_BG%;
            border: 0;
        }
        QSplitter::handle:horizontal { width: 2px; }
        QSplitter::handle:vertical   { height: 2px; }
        QSplitter::handle:hover { background: %ACCENT%; }

        /* ── List widgets ───────────────────────────────────────── */
        QListWidget {
            background: %SUNKEN_BG%;
            border: 1px solid %BORDER%;
            color: %TEXT_PRIMARY%;
            font-family: "IBM Plex Mono", monospace;
            font-size: 9pt;
            padding: 4px;
            outline: 0;
        }
        QListWidget::item { padding: 6px 8px; border: none; }
        QListWidget::item:hover { background: %INPUT_BG%; }
        QListWidget::item:selected {
            background: %INPUT_BG%;
            color: %ACCENT_LIGHT%;
            border-left: 2px solid %ACCENT%;
        }

        /* ── Scrollbars ─────────────────────────────────────────── */
        QScrollBar:vertical, QScrollBar:horizontal {
            background: %PAGE_BG%;
            border: none;
            width: 8px;
            height: 8px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: %BORDER_STRONG%;
            border-radius: 0;
            min-height: 28px;
            min-width: 28px;
        }
        QScrollBar::handle:vertical:hover,
        QScrollBar::handle:horizontal:hover { background: %ACCENT%; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        /* ── Tree / list views (file dialog, model trees, etc.) ── */
        QTreeView, QListView, QColumnView {
            background: %SUNKEN_BG%;
            alternate-background-color: %PANEL_BG%;
            color: %TEXT_PRIMARY%;
            border: 1px solid %BORDER%;
            outline: 0;
            selection-background-color: %ACCENT%;
            selection-color: %TEXT_ON_ACCENT%;
        }
        QTreeView::item, QListView::item, QColumnView::item {
            padding: 4px 6px;
            border: none;
        }
        QTreeView::item:hover, QListView::item:hover, QColumnView::item:hover {
            background: %INPUT_BG%;
        }
        QTreeView::item:selected, QListView::item:selected, QColumnView::item:selected {
            background: %ACCENT%;
            color: %TEXT_ON_ACCENT%;
        }
        QTreeView::branch {
            background: %SUNKEN_BG%;
        }
        QTreeView::branch:selected {
            background: %ACCENT%;
        }

        /* ── Tables ─────────────────────────────────────────────── */
        QTableWidget, QTableView {
            background: %SUNKEN_BG%;
            gridline-color: %BORDER%;
            alternate-background-color: %PANEL_BG%;
            border: 1px solid %BORDER%;
            color: %TEXT_PRIMARY%;
            selection-background-color: %ACCENT%;
            selection-color: %TEXT_ON_ACCENT%;
        }
        QHeaderView::section {
            background: %PANEL_BG%;
            border: none;
            border-bottom: 1px solid %BORDER%;
            padding: 6px 10px;
            color: %ACCENT%;
            font-family: "IBM Plex Mono", monospace;
            font-size: 8.5pt;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        /* ── Wizard / status bar / menus ────────────────────────── */
        QWizard QLabel { color: %TEXT_PRIMARY%; }
        QStatusBar {
            background: %STATUSBAR_BG%;
            color: %TEXT_MUTED%;
            font-family: "IBM Plex Mono", monospace;
            font-size: 8.5pt;
            border-top: 1px solid %BORDER%;
        }
        QMenuBar { background: %STATUSBAR_BG%; color: %TEXT_PRIMARY%; border-bottom: 1px solid %BORDER%; }
        QMenuBar::item { background: transparent; padding: 4px 12px; }
        QMenuBar::item:selected { background: %INPUT_BG%; color: %ACCENT%; }
        QMenu { background: %PANEL_BG%; color: %TEXT_PRIMARY%; border: 1px solid %BORDER_STRONG%; }
        QMenu::item:selected { background: %INPUT_BG%; color: %ACCENT%; }

        QToolTip {
            background: %INPUT_BG%;
            color: %TEXT_PRIMARY%;
            border: 1px solid %ACCENT%;
            padding: 4px 8px;
            font-family: "IBM Plex Mono", monospace;
            font-size: 9pt;
        }

        /* ── Named structural panels (live theme switching) ─────── */
        QWidget#encLeftPanel     { background: %PAGE_BG%; }
        QWidget#encChambersPanel { background: %PAGE_BG%; border-top: 1px solid %BORDER%; }
        QWidget#encParamPanel    { background: %PAGE_BG%; border-top: 1px solid %BORDER%; }
        QTabWidget#encPlotTabs::pane,
        QTabWidget#encParamTabs::pane {
            border: none;
            background: %PAGE_BG%;
        }
        QTableWidget#driverTable {
            border: 1px solid %BORDER%;
            border-radius: 4px;
            background: %PANEL_BG%;
        }
        QTableWidget#driverTable::item {
            padding: 4px 8px;
            color: %TEXT_PRIMARY%;
            background: %PANEL_BG%;
        }
        QTableWidget#driverTable::item:selected {
            background: %ACCENT_HOVER%;
            color: %TEXT_ON_ACCENT%;
        }

        /* ── Result-readout typographic display ─────────────────── */
        QLabel#ResultValue {
            color: %ACCENT_LIGHT%;
            font-family: "IBM Plex Mono", monospace;
            font-size: 16pt;
            font-weight: 300;
            letter-spacing: 0.5px;
            background: transparent;
        }
        QLabel#ResultLabel {
            color: %TEXT_MUTED%;
            font-family: "IBM Plex Sans", sans-serif;
            font-size: 8.5pt;
            font-weight: 500;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            background: transparent;
        }
        QLabel#ResultUnit {
            color: %TEXT_UNIT%;
            font-family: "IBM Plex Serif", serif;
            font-size: 9pt;
            font-style: italic;
            background: transparent;
        }
        QLabel#FormCaption {
            color: %TEXT_MUTED%;
            font-family: "IBM Plex Sans", sans-serif;
            font-size: 9pt;
            background: transparent;
        }
    )")
        .replace("%PAGE_BG%",        hex(pageBg()))
        .replace("%PANEL_BG%",       hex(panelBg()))
        .replace("%SUNKEN_BG%",      hex(sunkenBg()))
        .replace("%INPUT_BG_FOCUS%", hex(inputBgFocus()))
        .replace("%INPUT_BG%",       hex(inputBg()))
        .replace("%STATUSBAR_BG%",   hex(statusBarBg()))
        .replace("%BORDER_STRONG%",  hex(borderStrong()))
        .replace("%BORDER%",         hex(border()))
        .replace("%TEXT_PRIMARY%",   hex(textPrimary()))
        .replace("%TEXT_SECONDARY%", hex(textSecondary()))
        .replace("%TEXT_DISABLED%",  hex(textDisabled()))
        .replace("%TEXT_MUTED%",     hex(textMuted()))
        .replace("%TEXT_UNIT%",      hex(textUnit()))
        .replace("%TEXT_ON_ACCENT%", hex(textOnAccent()))
        .replace("%ACCENT_PRESSED%", hex(accentPressed()))
        .replace("%ACCENT_HOVER%",   hex(accentHover()))
        .replace("%ACCENT_LIGHT%",   hex(accentLight()))
        .replace("%ACCENT%",         hex(accent()));
}

void Theme::applyToApplication(QApplication *app) {
    if (app) app->setStyleSheet(globalStylesheet());
}

QString themed(const QString &qss) {
    const Theme &t = Theme::instance();
    QString s = qss;
    s.replace("%page%",       hex(t.pageBg()))
     .replace("%panel%",      hex(t.panelBg()))
     .replace("%sunken%",     hex(t.sunkenBg()))
     .replace("%inputFocus%", hex(t.inputBgFocus()))
     .replace("%input%",      hex(t.inputBg()))
     .replace("%statusbar%",  hex(t.statusBarBg()))
     .replace("%border2%",    hex(t.borderStrong()))
     .replace("%border%",     hex(t.border()))
     .replace("%grid%",       hex(t.gridLine()))
     .replace("%text2%",      hex(t.textSecondary()))
     .replace("%text%",       hex(t.textPrimary()))
     .replace("%muted%",      hex(t.textMuted()))
     .replace("%disabled%",   hex(t.textDisabled()))
     .replace("%unit%",       hex(t.textUnit()))
     .replace("%onAccent%",   hex(t.textOnAccent()))
     .replace("%accentHov%",  hex(t.accentHover()))
     .replace("%accentLt%",   hex(t.accentLight()))
     .replace("%accentPr%",   hex(t.accentPressed()))
     .replace("%accent%",     hex(t.accent()))
     .replace("%error%",      hex(t.statusError()))
     .replace("%warn%",        hex(t.statusWarn()));
    return s;
}
