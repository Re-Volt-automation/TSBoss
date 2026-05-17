#pragma once
#include <QObject>
#include <QColor>
#include <QString>
#include <QList>

class QApplication;

// Centralised theme tokens for TSBoss.
//
// Two modes ship: Dark (Tektronix × RME warm near-black) and Light
// (lab-notebook paper warm-white). Both keep the amber accent and IBM
// Plex typography so the brand reads identically across modes.
//
// Widgets with custom paint or inline stylesheets should:
//   1. read colours via Theme::instance().<accessor>() at paint time, AND
//   2. connect to Theme::themeChanged() and call update() / re-apply
//      their stylesheet so the switch is live.
class Theme : public QObject {
    Q_OBJECT
public:
    enum class Mode { Dark, Light };

    static Theme &instance();

    Mode mode() const { return m_mode; }
    void setMode(Mode m);
    void loadFromSettings();
    void saveToSettings() const;

    // ── Surfaces ────────────────────────────────────────────────
    QColor pageBg() const;        ///< Outermost background
    QColor panelBg() const;       ///< GroupBox / tab pane background
    QColor sunkenBg() const;      ///< Inset surfaces (list, table)
    QColor inputBg() const;       ///< Input fields
    QColor inputBgFocus() const;  ///< Input field on focus
    QColor statusBarBg() const;   ///< Status bar darker variant
    QColor plotBg() const;        ///< Inside plot rectangle
    QColor sidebarBg() const;     ///< Left navigation rail

    // ── Borders ─────────────────────────────────────────────────
    QColor border() const;        ///< Hairline divider
    QColor borderStrong() const;  ///< Secondary border (input bottom)
    QColor gridLine() const;      ///< Plot grid

    // ── Text ────────────────────────────────────────────────────
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor textMuted() const;
    QColor textDisabled() const;
    QColor textUnit() const;      ///< Smallest hint text
    QColor textOnAccent() const;  ///< Text drawn on accent fill

    // ── Accent ──────────────────────────────────────────────────
    QColor accent() const;        ///< Primary amber
    QColor accentHover() const;
    QColor accentLight() const;   ///< Highlight tint on text
    QColor accentPressed() const;

    // ── Plot helpers ────────────────────────────────────────────
    QColor plotAxis() const;      ///< Axis lines and prominent labels
    QColor plotTickLabel() const; ///< Tick numbers, secondary text
    QColor plotDimLine() const;   ///< Dashed reference lines
    QColor plotCursorBg() const;
    QColor plotCursorBorder() const;

    // ── Curve palette (cycles for multi-model plots) ────────────
    QList<QColor> curvePalette() const;

    // ── Status colours (errors, warnings) ───────────────────────
    QColor statusError() const;
    QColor statusWarn() const;

    // ── Global stylesheet (regenerated on theme change) ─────────
    QString globalStylesheet() const;
    void    applyToApplication(QApplication *app);

signals:
    void themeChanged();

private:
    Theme();
    Mode m_mode = Mode::Dark;
};

// Convenience — return CSS hex string for a colour.
inline QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

// Substitute %tokens% in a QSS string with current theme colours.
// Tokens: %page%, %panel%, %sunken%, %input%, %inputFocus%, %statusbar%,
//         %border%, %border2%, %grid%, %text%, %text2%, %muted%,
//         %disabled%, %unit%, %onAccent%, %accent%, %accentHov%,
//         %accentLt%, %accentPr%, %error%, %warn%.
QString themed(const QString &qss);
