#include "mainwindow.h"
#include <QApplication>
#include <QFont>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("TSBoss");
    app.setApplicationDisplayName("TSBoss – T/S Parameter Manager");
    app.setOrganizationName("TSBoss");
    app.setOrganizationDomain("tsboss.local");
    app.setApplicationVersion("1.0.0");

    // Clean base style
    app.setStyle(QStyleFactory::create("Fusion"));

    // ─── Dark instrumentation theme ──────────────────────────────────
    // Aesthetic: refined audio measurement panel — Tektronix scope ×
    // RME interface. Warm near-black with cream text, single amber
    // accent, and IBM Plex typography (Sans for UI, Mono for digits,
    // Serif for headings).
    app.setStyleSheet(R"(
        /* ── Surfaces ───────────────────────────────────────────── */
        QWidget {
            font-family: "IBM Plex Sans", "Helvetica Neue", "Liberation Sans", sans-serif;
            font-size: 10pt;
            color: #E8E1D3;
            background: transparent;
        }
        QMainWindow, QDialog {
            background: #0E1116;
        }
        QStackedWidget > QWidget,
        QSplitter, QScrollArea, QScrollArea > QWidget > QWidget {
            background: #0E1116;
        }

        /* ── Group boxes — hairline panels with serif titles ────── */
        QGroupBox {
            font-family: "IBM Plex Serif", "Liberation Serif", serif;
            font-size: 10.5pt;
            font-weight: 500;
            color: #E8E1D3;
            border: 1px solid #262C36;
            border-radius: 2px;
            margin-top: 18px;
            padding: 14px 10px 10px 10px;
            background: #161A21;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 8px;
            color: #D97706;
            letter-spacing: 1px;
            text-transform: uppercase;
            font-size: 8.5pt;
            font-family: "IBM Plex Sans", sans-serif;
            font-weight: 600;
            background: #0E1116;
        }
        QGroupBox QLabel,
        QFormLayout QLabel {
            color: #BAB3A4;
            background: transparent;
        }

        /* ── Inputs — flat with amber underline on focus ─────────── */
        QDoubleSpinBox, QSpinBox, QLineEdit, QDateEdit, QTextEdit, QComboBox {
            border: 1px solid #262C36;
            border-bottom: 1px solid #3A4150;
            border-radius: 0px;
            padding: 4px 8px;
            background: #1A1F28;
            color: #E8E1D3;
            font-family: "IBM Plex Mono", "JetBrains Mono", "Fira Mono", monospace;
            font-size: 9.5pt;
            selection-background-color: #D97706;
            selection-color: #0E1116;
        }
        QDoubleSpinBox::up-button, QDoubleSpinBox::down-button,
        QSpinBox::up-button,       QSpinBox::down-button {
            width: 0;
            border: none;
        }
        QDoubleSpinBox:focus, QSpinBox:focus, QLineEdit:focus,
        QDateEdit:focus, QTextEdit:focus, QComboBox:focus {
            border: 1px solid #262C36;
            border-bottom: 2px solid #D97706;
            background: #1E2530;
            outline: none;
        }
        QDoubleSpinBox:disabled, QSpinBox:disabled, QLineEdit:disabled,
        QComboBox:disabled {
            color: #5A5A5A;
            background: #14181F;
            border-bottom: 1px solid #262C36;
        }
        QDoubleSpinBox:read-only, QSpinBox:read-only, QLineEdit:read-only {
            color: #8A8579;
            background: #14181F;
        }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #8A8579;
            margin-right: 6px;
        }
        QComboBox QAbstractItemView {
            background: #161A21;
            color: #E8E1D3;
            border: 1px solid #3A4150;
            selection-background-color: #D97706;
            selection-color: #0E1116;
            outline: none;
        }

        /* ── Buttons — outline default, filled amber for primary ── */
        QPushButton {
            background: transparent;
            color: #E8E1D3;
            border: 1px solid #3A4150;
            border-radius: 0px;
            padding: 6px 18px;
            font-family: "IBM Plex Sans", sans-serif;
            font-size: 9pt;
            font-weight: 500;
            letter-spacing: 0.5px;
        }
        QPushButton:hover   { border-color: #D97706; color: #F5C887; }
        QPushButton:pressed { background: #D97706; color: #0E1116; }
        QPushButton:disabled{ color: #5A5A5A; border-color: #262C36; }
        QPushButton:default {
            background: #D97706;
            color: #0E1116;
            border-color: #D97706;
        }
        QPushButton:default:hover  { background: #F59E0B; border-color: #F59E0B; }
        QPushButton:default:pressed{ background: #B45309; border-color: #B45309; }

        /* ── Tabs — thin, mono labels with amber underline ──────── */
        QTabWidget::pane {
            border: 1px solid #262C36;
            background: #161A21;
            top: -1px;
        }
        QTabBar::tab {
            background: #0E1116;
            color: #8A8579;
            border: none;
            border-bottom: 1px solid #262C36;
            padding: 8px 18px;
            font-family: "IBM Plex Mono", monospace;
            font-size: 8.5pt;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            min-width: 70px;
        }
        QTabBar::tab:hover    { color: #E8E1D3; }
        QTabBar::tab:selected {
            color: #E8E1D3;
            background: #161A21;
            border-bottom: 2px solid #D97706;
        }

        /* ── Splitter handles ───────────────────────────────────── */
        QSplitter::handle {
            background: #0E1116;
            border: 0;
        }
        QSplitter::handle:horizontal { width: 2px; }
        QSplitter::handle:vertical   { height: 2px; }
        QSplitter::handle:hover { background: #D97706; }

        /* ── List widgets ───────────────────────────────────────── */
        QListWidget {
            background: #14181F;
            border: 1px solid #262C36;
            color: #E8E1D3;
            font-family: "IBM Plex Mono", monospace;
            font-size: 9pt;
            padding: 4px;
            outline: 0;
        }
        QListWidget::item { padding: 6px 8px; border: none; }
        QListWidget::item:hover { background: #1A1F28; }
        QListWidget::item:selected {
            background: #1A1F28;
            color: #F5C887;
            border-left: 2px solid #D97706;
        }

        /* ── Scrollbars ─────────────────────────────────────────── */
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #0E1116;
            border: none;
            width: 8px;
            height: 8px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #3A4150;
            border-radius: 0;
            min-height: 28px;
            min-width: 28px;
        }
        QScrollBar::handle:vertical:hover,
        QScrollBar::handle:horizontal:hover { background: #D97706; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

        /* ── Tables ─────────────────────────────────────────────── */
        QTableWidget, QTableView {
            background: #14181F;
            gridline-color: #262C36;
            alternate-background-color: #161A21;
            border: 1px solid #262C36;
            color: #E8E1D3;
            selection-background-color: #D97706;
            selection-color: #0E1116;
        }
        QHeaderView::section {
            background: #161A21;
            border: none;
            border-bottom: 1px solid #262C36;
            padding: 6px 10px;
            color: #D97706;
            font-family: "IBM Plex Mono", monospace;
            font-size: 8.5pt;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        /* ── Wizard / status bar / menus ────────────────────────── */
        QWizard QLabel { color: #E8E1D3; }
        QStatusBar {
            background: #0A0C10;
            color: #8A8579;
            font-family: "IBM Plex Mono", monospace;
            font-size: 8.5pt;
            border-top: 1px solid #262C36;
        }
        QMenuBar { background: #0A0C10; color: #E8E1D3; border-bottom: 1px solid #262C36; }
        QMenuBar::item { background: transparent; padding: 4px 12px; }
        QMenuBar::item:selected { background: #1A1F28; color: #D97706; }
        QMenu { background: #161A21; color: #E8E1D3; border: 1px solid #3A4150; }
        QMenu::item:selected { background: #1A1F28; color: #D97706; }

        QToolTip {
            background: #1A1F28;
            color: #E8E1D3;
            border: 1px solid #D97706;
            padding: 4px 8px;
            font-family: "IBM Plex Mono", monospace;
            font-size: 9pt;
        }

        /* ── Result-readout typographic display ─────────────────── */
        QLabel#ResultValue {
            color: #F5C887;
            font-family: "IBM Plex Mono", monospace;
            font-size: 16pt;
            font-weight: 300;
            letter-spacing: 0.5px;
            background: transparent;
        }
        QLabel#ResultLabel {
            color: #8A8579;
            font-family: "IBM Plex Sans", sans-serif;
            font-size: 8.5pt;
            font-weight: 500;
            letter-spacing: 1.5px;
            text-transform: uppercase;
            background: transparent;
        }
        QLabel#ResultUnit {
            color: #6E6960;
            font-family: "IBM Plex Serif", serif;
            font-size: 9pt;
            font-style: italic;
            background: transparent;
        }
        QLabel#FormCaption {
            color: #8A8579;
            font-family: "IBM Plex Sans", sans-serif;
            font-size: 9pt;
            background: transparent;
        }
    )");

    // Default font: IBM Plex Sans @ 10pt
    QFont font("IBM Plex Sans");
    font.setStyleHint(QFont::SansSerif);
    font.setPointSize(10);
    app.setFont(font);

    MainWindow w;
    w.show();

    return app.exec();
}
