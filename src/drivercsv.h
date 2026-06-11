#pragma once
#include "driverrecord.h"
#include <QList>
#include <QString>
#include <QStringList>

// ─────────────────────────────────────────────────────────────────
//  drivercsv – RFC 4180 CSV import/export for DriverRecords.
//  Pure Qt Core (no UI) so it is unit-testable; the column set
//  matches the historical TSBoss "Export CSV" format.
// ─────────────────────────────────────────────────────────────────
namespace drivercsv {

/// One cell that could not be parsed during import (imported as 0 / today).
struct ParseIssue {
    int     row;      ///< 1-based data record number (header excluded)
    QString column;   ///< lower-case header name
    QString value;    ///< offending cell text
};

struct ImportResult {
    bool                ok = false;  ///< header was valid and rows were parsed
    QString             error;       ///< fatal problem when !ok
    QList<DriverRecord> records;
    QList<ParseIssue>   issues;      ///< non-fatal bad cells
};

/// Split CSV text into records of fields. Handles quoted fields containing
/// commas, doubled quotes ("") and newlines; accepts LF / CRLF / CR endings.
/// Blank records are dropped.
QList<QStringList> parseCsv(const QString &text);

/// Escape a single field for CSV output (quoted only when required).
QString csvField(const QString &raw);

/// Render records as CSV text, header included.
QString exportCsv(const QList<DriverRecord> &records);

/// Parse CSV text (TSBoss export format) into driver records.
ImportResult importCsv(const QString &text);

} // namespace drivercsv
