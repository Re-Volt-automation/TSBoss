#pragma once
#include "boxmodel.h"
#include "theme.h"
#include <QColor>
#include <QString>

// ─────────────────────────────────────────────────────────────────
//  Shared helpers for the model plots (and the enclosure widget).
// ─────────────────────────────────────────────────────────────────

inline constexpr double F_MIN = 10.0;
inline constexpr double F_MAX = 1000.0;

// Enclosure-type tests — replace the old isVented / isInfiniteBaffle
// bool pair.  isPorted = "uses port acoustic math" (Vented or BP).
inline bool isVented   (const BoxModel &m) { return m.encType == BoxModel::EncType::Vented;    }
inline bool isIB       (const BoxModel &m) { return m.encType == BoxModel::EncType::IB;        }
inline bool isBP4      (const BoxModel &m) { return m.encType == BoxModel::EncType::Bandpass4; }
inline bool isBP6      (const BoxModel &m) { return m.encType == BoxModel::EncType::Bandpass6; }
inline bool isBandpass (const BoxModel &m) { return isBP4(m) || isBP6(m); }
inline bool isPorted   (const BoxModel &m) { return isVented(m) || isBandpass(m); }
inline bool isSealed   (const BoxModel &m) { return m.encType == BoxModel::EncType::Sealed;    }

// Plot colour accessors — read live from the global Theme so a theme
// switch immediately changes the canvas without restarting.
inline QColor CLR_PAGE_BG() { return Theme::instance().pageBg(); }
inline QColor CLR_PLOT_BG() { return Theme::instance().plotBg(); }
inline QColor CLR_ACCENT()  { return Theme::instance().accent(); }
inline QColor CLR_GREY_DK() { return Theme::instance().plotAxis(); }
inline QColor CLR_GREY()    { return Theme::instance().plotTickLabel(); }
inline QColor CLR_GREY_LT() { return Theme::instance().plotDimLine(); }
inline QColor CLR_GRID()    { return Theme::instance().gridLine(); }

// Curve palette delegates to Theme so light mode gets paper-friendly hues.
inline QColor paletteColor(int i) {
    const auto pal = Theme::instance().curvePalette();
    return pal.isEmpty() ? QColor("#D97706") : pal[i % pal.size()];
}
inline int paletteSize() {
    const int n = Theme::instance().curvePalette().size();
    return n > 0 ? n : 1;
}

// Plot-area margins shared by the cursor hit test and the paint code.
inline constexpr int PLT_ML = 58, PLT_MR = 16, PLT_MT = 16, PLT_MB = 42;

/// One row in the cursor annotation box.
struct CursorEntry { QColor color; bool active; QString name; double yPix; QString valStr; };
