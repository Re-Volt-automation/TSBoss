#include "driverdb.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QStandardPaths>
#include <QDir>

static const char *CREATE_TABLE = R"(
CREATE TABLE IF NOT EXISTS drivers (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    make          TEXT    NOT NULL,
    model         TEXT    NOT NULL,
    date_measured TEXT,
    v_meas        REAL    DEFAULT 1.0,
    r_series      REAL    DEFAULT 50.0,
    voltage_mode  INTEGER DEFAULT 0,
    measured_by   TEXT,
    notes         TEXT,
    Re            REAL, fs     REAL, Zmax  REAL,
    f1            REAL, f2     REAL, delta_m REAL,
    fo            REAL, Dd     REAL, Zmin  REAL,
    f3            REAL,
    Z12           REAL, fs_verify REAL, R0   REAL,
    Qms           REAL, Qes   REAL,  Qts  REAL,
    mms           REAL, Rms   REAL,  BL   REAL,
    Cms           REAL, Sd    REAL,  Vas  REAL,
    Le            REAL,
    Znom          REAL DEFAULT 0,
    fLe           REAL DEFAULT 0,
    KLe           REAL DEFAULT 0,
    Xmax          REAL DEFAULT 0,
    Xlim          REAL DEFAULT 0,
    Pe            REAL DEFAULT 0,
    Hg            REAL DEFAULT 0,
    Vd            REAL DEFAULT 0,
    is_dvc        INTEGER DEFAULT 0,
    dvc_wiring    INTEGER DEFAULT 0,
    field_origins        INTEGER DEFAULT 0,
    user_entered_fields  INTEGER DEFAULT 0,
    spl                  REAL    DEFAULT 0
)
)";

DriverDatabase::DriverDatabase(const QString &dbPath)
{
    if (dbPath.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(
                                QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        m_path = dir + "/tsboss_drivers.db";
    } else {
        m_path = dbPath;
    }
}

DriverDatabase::~DriverDatabase() { close(); }

bool DriverDatabase::open()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE", "tsboss_conn");
    m_db.setDatabaseName(m_path);
    if (!m_db.open()) {
        m_lastError = m_db.lastError().text();
        return false;
    }
    QSqlQuery pragma(m_db);
    pragma.exec("PRAGMA journal_mode=WAL");
    pragma.exec("PRAGMA foreign_keys=ON");
    return createSchema();
}

void DriverDatabase::close()
{
    if (m_db.isOpen()) m_db.close();
    const QString connName = m_db.connectionName();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(connName);
}

bool DriverDatabase::isOpen() const  { return m_db.isOpen();       }
QString DriverDatabase::lastError() const { return m_lastError;    }

bool DriverDatabase::createSchema()
{
    QSqlQuery q(m_db);
    if (!q.exec(CREATE_TABLE)) {
        m_lastError = q.lastError().text();
        return false;
    }

    // Migrate older databases that are missing columns added after initial release.
    // ALTER TABLE ADD COLUMN fails silently if the column already exists (we ignore that error).
    static const struct { const char *col; const char *def; } kMigrations[] = {
        { "v_meas",       "REAL    DEFAULT 1.0"  },
        { "r_series",     "REAL    DEFAULT 50.0" },
        { "voltage_mode", "INTEGER DEFAULT 0"    },
        { "Znom",         "REAL    DEFAULT 0"    },
        { "fLe",          "REAL    DEFAULT 0"    },
        { "KLe",          "REAL    DEFAULT 0"    },
        { "Xmax",         "REAL    DEFAULT 0"    },
        { "Xlim",         "REAL    DEFAULT 0"    },
        { "Pe",           "REAL    DEFAULT 0"    },
        { "Hg",           "REAL    DEFAULT 0"    },
        { "Vd",           "REAL    DEFAULT 0"    },
        { "is_dvc",        "INTEGER DEFAULT 0"   },
        { "dvc_wiring",    "INTEGER DEFAULT 0"   },
        { "field_origins",        "INTEGER DEFAULT 0" },
        { "user_entered_fields",  "INTEGER DEFAULT 0" },
        { "spl",                  "REAL    DEFAULT 0" },
    };
    for (const auto &m : kMigrations) {
        QSqlQuery mq(m_db);
        mq.exec(QString("ALTER TABLE drivers ADD COLUMN %1 %2").arg(m.col, m.def));
        // ignore return value — "duplicate column name" means it already exists
    }

    return true;
}

void DriverDatabase::bindRecord(QSqlQuery &q, const DriverRecord &r) const
{
    q.bindValue(":make",  r.make);
    q.bindValue(":model", r.model);
    q.bindValue(":date",  r.dateMeasured.toString(Qt::ISODate));
    q.bindValue(":vmeas", r.V_meas);
    q.bindValue(":rs",    r.R_s);
    q.bindValue(":vmode", r.voltageMode);
    q.bindValue(":by",     r.measuredBy);
    q.bindValue(":notes",  r.notes);
    q.bindValue(":Re",     r.Re);
    q.bindValue(":fs",     r.fs);
    q.bindValue(":Zmax",   r.Zmax);
    q.bindValue(":f1",     r.f1);
    q.bindValue(":f2",     r.f2);
    q.bindValue(":dm",     r.deltaM);
    q.bindValue(":fo",     r.fo);
    q.bindValue(":Dd",     r.Dd);
    q.bindValue(":Zmin",   r.Zmin);
    q.bindValue(":f3",     r.f3);
    q.bindValue(":Z12",    r.Z12);
    q.bindValue(":fsv",    r.fsVerify);
    q.bindValue(":R0",     r.R0);
    q.bindValue(":Qms",    r.Qms);
    q.bindValue(":Qes",    r.Qes);
    q.bindValue(":Qts",    r.Qts);
    q.bindValue(":mms",    r.mms);
    q.bindValue(":Rms",    r.Rms);
    q.bindValue(":BL",     r.BL);
    q.bindValue(":Cms",    r.Cms);
    q.bindValue(":Sd",     r.Sd);
    q.bindValue(":Vas",    r.Vas);
    q.bindValue(":Le",     r.Le);
    q.bindValue(":Znom",   r.Znom);
    q.bindValue(":fLe",    r.fLe);
    q.bindValue(":KLe",    r.KLe);
    q.bindValue(":Xmax",   r.Xmax);
    q.bindValue(":Xlim",   r.Xlim);
    q.bindValue(":Pe",     r.Pe);
    q.bindValue(":Hg",           r.Hg);
    q.bindValue(":Vd",           r.Vd);
    q.bindValue(":is_dvc",              r.isDVC ? 1 : 0);
    q.bindValue(":dvc_wiring",          r.dvcWiring);
    q.bindValue(":field_origins",       static_cast<qint64>(r.fieldOrigins));
    q.bindValue(":user_entered_fields", static_cast<qint64>(r.userEnteredFields));
    q.bindValue(":spl",                 r.Spl);
}

bool DriverDatabase::saveDriver(DriverRecord &r)
{
    QSqlQuery q(m_db);
    if (r.id < 0) {
        q.prepare(R"(INSERT INTO drivers
            (make,model,date_measured,measured_by,notes,
             v_meas,r_series,voltage_mode,
             Re,fs,Zmax,f1,f2,delta_m,fo,Dd,Zmin,f3,
             Z12,fs_verify,R0,Qms,Qes,Qts,mms,Rms,BL,Cms,Sd,Vas,Le,
             Znom,fLe,KLe,Xmax,Xlim,Pe,Hg,Vd,
             is_dvc,dvc_wiring,field_origins,user_entered_fields,spl)
            VALUES
            (:make,:model,:date,:by,:notes,
             :vmeas,:rs,:vmode,
             :Re,:fs,:Zmax,:f1,:f2,:dm,:fo,:Dd,:Zmin,:f3,
             :Z12,:fsv,:R0,:Qms,:Qes,:Qts,:mms,:Rms,:BL,:Cms,:Sd,:Vas,:Le,
             :Znom,:fLe,:KLe,:Xmax,:Xlim,:Pe,:Hg,:Vd,
             :is_dvc,:dvc_wiring,:field_origins,:user_entered_fields,:spl))");
    } else {
        q.prepare(R"(UPDATE drivers SET
            make=:make, model=:model,
            date_measured=:date, measured_by=:by, notes=:notes,
            v_meas=:vmeas, r_series=:rs, voltage_mode=:vmode,
            Re=:Re, fs=:fs, Zmax=:Zmax, f1=:f1, f2=:f2,
            delta_m=:dm, fo=:fo, Dd=:Dd, Zmin=:Zmin, f3=:f3,
            Z12=:Z12, fs_verify=:fsv, R0=:R0, Qms=:Qms, Qes=:Qes,
            Qts=:Qts, mms=:mms, Rms=:Rms, BL=:BL, Cms=:Cms,
            Sd=:Sd, Vas=:Vas, Le=:Le,
            Znom=:Znom, fLe=:fLe, KLe=:KLe,
            Xmax=:Xmax, Xlim=:Xlim, Pe=:Pe, Hg=:Hg, Vd=:Vd,
            is_dvc=:is_dvc, dvc_wiring=:dvc_wiring, field_origins=:field_origins,
            user_entered_fields=:user_entered_fields,
            spl=:spl
            WHERE id=:id)");
        q.bindValue(":id", r.id);
    }
    bindRecord(q, r);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    if (r.id < 0) r.id = q.lastInsertId().toInt();
    return true;
}

bool DriverDatabase::deleteDriver(int id)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM drivers WHERE id=:id");
    q.bindValue(":id", id);
    if (!q.exec()) { m_lastError = q.lastError().text(); return false; }
    return true;
}

bool DriverDatabase::clearAllDrivers()
{
    QSqlQuery q(m_db);
    if (!q.exec("DELETE FROM drivers")) {
        m_lastError = q.lastError().text();
        return false;
    }
    // Reset the auto-increment counter so IDs start fresh
    q.exec("DELETE FROM sqlite_sequence WHERE name='drivers'");
    return true;
}

DriverRecord DriverDatabase::fromQuery(QSqlQuery &q) const
{
    DriverRecord r;
    r.id           = q.value("id").toInt();
    r.make         = q.value("make").toString();
    r.model        = q.value("model").toString();
    r.dateMeasured = QDate::fromString(q.value("date_measured").toString(), Qt::ISODate);
    r.V_meas       = q.value("v_meas").toDouble();
    r.R_s          = q.value("r_series").isNull() ? 50.0 : q.value("r_series").toDouble();
    r.voltageMode  = q.value("voltage_mode").toInt();
    r.measuredBy   = q.value("measured_by").toString();
    r.notes        = q.value("notes").toString();
    r.Re     = q.value("Re").toDouble();
    r.fs     = q.value("fs").toDouble();
    r.Zmax   = q.value("Zmax").toDouble();
    r.f1     = q.value("f1").toDouble();
    r.f2     = q.value("f2").toDouble();
    r.deltaM = q.value("delta_m").toDouble();
    r.fo     = q.value("fo").toDouble();
    r.Dd     = q.value("Dd").toDouble();
    r.Zmin   = q.value("Zmin").toDouble();
    r.f3     = q.value("f3").toDouble();
    r.Z12      = q.value("Z12").toDouble();
    r.fsVerify = q.value("fs_verify").toDouble();
    r.R0       = q.value("R0").toDouble();
    r.Qms      = q.value("Qms").toDouble();
    r.Qes      = q.value("Qes").toDouble();
    r.Qts      = q.value("Qts").toDouble();
    r.mms      = q.value("mms").toDouble();
    r.Rms      = q.value("Rms").toDouble();
    r.BL       = q.value("BL").toDouble();
    r.Cms      = q.value("Cms").toDouble();
    r.Sd       = q.value("Sd").toDouble();
    r.Vas      = q.value("Vas").toDouble();
    r.Le       = q.value("Le").toDouble();
    r.Znom     = q.value("Znom").toDouble();
    r.fLe      = q.value("fLe").toDouble();
    r.KLe      = q.value("KLe").toDouble();
    r.Xmax     = q.value("Xmax").toDouble();
    r.Xlim     = q.value("Xlim").toDouble();
    r.Pe       = q.value("Pe").toDouble();
    r.Hg           = q.value("Hg").toDouble();
    r.Vd           = q.value("Vd").toDouble();
    r.isDVC            = q.value("is_dvc").toInt() != 0;
    r.dvcWiring        = q.value("dvc_wiring").toInt();
    r.fieldOrigins     = static_cast<quint64>(q.value("field_origins").toLongLong());
    r.userEnteredFields = static_cast<quint64>(q.value("user_entered_fields").toLongLong());
    r.Spl              = q.value("spl").toDouble();
    return r;
}

DriverRecord DriverDatabase::loadDriver(int id) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM drivers WHERE id=:id");
    q.bindValue(":id", id);
    q.exec();
    if (q.next()) return fromQuery(q);
    return {};
}

QList<DriverRecord> DriverDatabase::allDrivers() const
{
    QList<DriverRecord> list;
    QSqlQuery q("SELECT * FROM drivers ORDER BY date_measured DESC, make, model", m_db);
    while (q.next()) list.append(fromQuery(q));
    return list;
}

QList<DriverRecord> DriverDatabase::searchDrivers(const QString &search) const
{
    QList<DriverRecord> list;
    const QString p = "%" + search + "%";
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM drivers WHERE make LIKE :p OR model LIKE :p "
              "OR measured_by LIKE :p "
              "ORDER BY date_measured DESC");
    q.bindValue(":p", p);
    q.exec();
    while (q.next()) list.append(fromQuery(q));
    return list;
}

int DriverDatabase::driverCount() const
{
    QSqlQuery q("SELECT COUNT(*) FROM drivers", m_db);
    if (q.next()) return q.value(0).toInt();
    return 0;
}

