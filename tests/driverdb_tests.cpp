#include "driverdb.h"
#include "driverrecord.h"
#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cstdio>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// Build a database file with the original v1 schema (none of the columns added
// after initial release) plus one row, using a throwaway connection.
static bool makeOldSchemaDb(const QString &path)
{
    bool ok = false;
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "old_schema_conn");
        db.setDatabaseName(path);
        if (db.open()) {
            QSqlQuery q(db);
            ok = q.exec(
                "CREATE TABLE drivers ("
                " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                " make TEXT NOT NULL, model TEXT NOT NULL,"
                " date_measured TEXT, measured_by TEXT, notes TEXT,"
                " Re REAL, fs REAL, Zmax REAL, f1 REAL, f2 REAL, delta_m REAL,"
                " fo REAL, Dd REAL, Zmin REAL, f3 REAL,"
                " Z12 REAL, fs_verify REAL, R0 REAL,"
                " Qms REAL, Qes REAL, Qts REAL,"
                " mms REAL, Rms REAL, BL REAL, Cms REAL, Sd REAL, Vas REAL, Le REAL)")
              && q.exec("INSERT INTO drivers (make,model,fs,Qts,Vas) "
                        "VALUES ('Old','Driver',31.5,0.42,0.085)");
            db.close();
        }
    }
    QSqlDatabase::removeDatabase("old_schema_conn");
    return ok;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);   // required for Qt SQL plugins

    QTemporaryDir tmp;
    if (!tmp.isValid()) { std::printf("FAIL: no temp dir\n"); return 1; }

    // ── Save / load round-trip ────────────────────────────────────
    {
        DriverDatabase db(tmp.filePath("roundtrip.db"));
        check(db.open(), "open: fresh database");

        DriverRecord r;
        r.make  = "Acme";
        r.model = "W6 \"Special\"";
        r.notes = "line one\nline two";
        r.dateMeasured = QDate(2026, 6, 11);
        r.Re = 3.2; r.fs = 28.0; r.Qms = 5.5; r.Qes = 0.5; r.Qts = 0.458;
        r.Vas = 0.045; r.BL = 11.2; r.Xmax = 8.5;
        r.fieldOrigins      = FieldOrigin::Qts | FieldOrigin::Vas;
        r.userEnteredFields = FieldOrigin::BL;

        check(db.saveDriver(r),  "save: insert succeeds");
        check(r.id > 0,          "save: insert assigns id");

        const DriverRecord g = db.loadDriver(r.id);
        check(g.make == r.make && g.model == r.model, "load: identity fields");
        check(g.notes == r.notes,                     "load: multiline notes intact");
        check(g.dateMeasured == r.dateMeasured,       "load: date");
        check(g.Qts == r.Qts && g.Xmax == r.Xmax,     "load: numeric fields");
        check(g.fieldOrigins == r.fieldOrigins
              && g.userEnteredFields == r.userEnteredFields, "load: provenance masks");

        // Update path
        DriverRecord u = g;
        u.fs = 29.5;
        check(db.saveDriver(u), "save: update succeeds");
        check(db.loadDriver(u.id).fs == 29.5, "load: update visible");
        check(db.driverCount() == 1, "update did not duplicate");
        db.close();
    }

    // ── clearAllDrivers: works on fresh DB, resets ids, atomic ───
    {
        DriverDatabase db(tmp.filePath("clear.db"));
        check(db.open(), "open: clear-test database");

        // No row was ever inserted -> sqlite_sequence does not exist yet.
        check(db.clearAllDrivers(), "clear: succeeds on never-used database");

        DriverRecord a; a.make = "A"; a.model = "1"; a.fs = 30;
        DriverRecord b; b.make = "B"; b.model = "2"; b.fs = 40;
        db.saveDriver(a);
        db.saveDriver(b);
        check(db.driverCount() == 2, "clear: two rows inserted");

        check(db.clearAllDrivers(),  "clear: succeeds with rows present");
        check(db.driverCount() == 0, "clear: table empty");

        DriverRecord c; c.make = "C"; c.model = "3"; c.fs = 50;
        db.saveDriver(c);
        check(c.id == 1, "clear: autoincrement reset, next id is 1");
        db.close();
    }

    // ── Migration: old v1 schema gains new columns, data preserved ─
    {
        const QString path = tmp.filePath("old.db");
        check(makeOldSchemaDb(path), "migration: old-schema fixture built");

        DriverDatabase db(path);
        check(db.open(), "migration: old database opens");

        const auto all = db.allDrivers();
        check(all.size() == 1, "migration: existing row survives");
        if (all.size() == 1) {
            check(all[0].make == "Old" && all[0].fs == 31.5,
                  "migration: existing values intact");
            check(all[0].Xmax == 0.0 && all[0].fieldOrigins == 0,
                  "migration: new columns read back as defaults");

            // Saving through the new schema must work (touches new columns).
            DriverRecord u = all[0];
            u.Xmax = 12.0;
            check(db.saveDriver(u), "migration: update with new columns succeeds");
            check(db.loadDriver(u.id).Xmax == 12.0, "migration: new column persisted");
        }
        db.close();

        // Re-open: migrations must be idempotent.
        DriverDatabase db2(path);
        check(db2.open(), "migration: idempotent on second open");
        db2.close();
    }

    // ── Migration failure must be reported, not swallowed ─────────
    {
        const QString path = tmp.filePath("readonly.db");
        check(makeOldSchemaDb(path), "readonly: old-schema fixture built");
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::ReadUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);

        DriverDatabase db(path);
        const bool opened = db.open();
        check(!opened, "readonly: open() reports failure when migration cannot run");
        check(!db.lastError().isEmpty(), "readonly: lastError is set");
        db.close();

        // Restore write access so the temp dir can be removed.
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser  | QFileDevice::WriteUser);
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll driverdb tests passed.\n");
    return g_failures;
}
