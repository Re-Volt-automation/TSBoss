#pragma once
#include "boxmodel.h"
#include <QJsonObject>
#include <QList>
#include <QString>

// ─────────────────────────────────────────────────────────────────
//  boxmodelio – JSON persistence for BoxModel (.tsbox / .tsproj).
//  Qt Core/Gui only (QColor), no widgets — unit-testable.
// ─────────────────────────────────────────────────────────────────
namespace boxmodelio {

/// Format version written into .tsbox / .tsproj envelopes.
/// 1 = legacy unversioned files; 2 = first versioned release.
inline constexpr int kFormatVersion = 2;

/// Serialize one model. Includes legacy bools (isVented/isInfiniteBaffle)
/// so older builds can still read the file.
QJsonObject modelToJson(const BoxModel &m);

/// Deserialize one model. Unknown enum values are clamped; missing keys take
/// the historical defaults. A missing/empty color leaves m.color invalid —
/// callers assign a palette color.
BoxModel modelFromJson(const QJsonObject &obj);

/// Build the .tsbox / .tsproj root objects: {type, version, model|models}.
QJsonObject tsboxDocument(const BoxModel &m);
QJsonObject tsprojDocument(const QList<BoxModel> &models);

struct LoadResult {
    bool            ok = false;
    QString         error;        ///< set when !ok
    int             version = 1;  ///< 1 when the file predates versioning
    QList<BoxModel> models;       ///< exactly one entry for .tsbox
};

LoadResult loadTsbox (const QByteArray &json);
LoadResult loadTsproj(const QByteArray &json);

} // namespace boxmodelio
