#pragma once
#include "driverrecord.h"
#include <QList>
#include <QSqlDatabase>
#include <QString>

// ─────────────────────────────────────────────────────────────────
//  DriverDatabase – SQLite-backed persistence for DriverRecords.
//  Stores raw measurements, all T/S results, metadata, and notes.
// ─────────────────────────────────────────────────────────────────

class DriverDatabase
{
public:
    explicit DriverDatabase(const QString &dbPath = {});
    ~DriverDatabase();

    bool   open();
    void   close();
    bool   isOpen()     const;
    QString lastError() const;
    QString path()      const { return m_path; }

    /// Insert (id < 0) or update (id ≥ 0).  Sets record.id on insert.
    bool saveDriver     (DriverRecord &record);
    bool deleteDriver   (int id);
    bool clearAllDrivers();   ///< Delete every record (for overwrite import)

    DriverRecord        loadDriver  (int id)              const;
    QList<DriverRecord> allDrivers  ()                    const;
    QList<DriverRecord> searchDrivers(const QString &q)   const;
    int                 driverCount ()                    const;

private:
    bool createSchema();
    DriverRecord fromQuery(class QSqlQuery &q) const;
    void         bindRecord(class QSqlQuery &q, const DriverRecord &r) const;

    QString      m_path;
    QSqlDatabase m_db;
    QString      m_lastError;
};
