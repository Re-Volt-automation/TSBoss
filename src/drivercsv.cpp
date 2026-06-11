#include "drivercsv.h"
#include <QDate>
#include <QMap>

namespace drivercsv {

QList<QStringList> parseCsv(const QString &text)
{
    QList<QStringList> records;
    QStringList fields;
    QString cur;
    bool inQuote = false;

    auto endField  = [&] { fields << cur; cur.clear(); };
    auto endRecord = [&] {
        endField();
        // Drop blank records (empty lines parse as one empty field).
        const bool blank = fields.size() == 1 && fields[0].trimmed().isEmpty();
        if (!blank) records << fields;
        fields.clear();
    };

    for (int i = 0; i < text.size(); ++i) {
        const QChar c = text[i];
        if (inQuote) {
            if (c == '"') {
                // Peek: doubled quote is an escaped quote
                if (i + 1 < text.size() && text[i + 1] == '"') { cur += '"'; ++i; }
                else inQuote = false;
            } else { cur += c; }
        }
        else if (c == '"')  { inQuote = true; }
        else if (c == ',')  { endField(); }
        else if (c == '\n') { endRecord(); }
        else if (c == '\r') {                       // CRLF pair or lone CR
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            endRecord();
        }
        else { cur += c; }
    }
    if (!cur.isEmpty() || !fields.isEmpty()) endRecord();
    return records;
}

QString csvField(const QString &raw)
{
    if (!raw.contains(',') && !raw.contains('"')
        && !raw.contains('\n') && !raw.contains('\r'))
        return raw;
    QString s = raw;
    s.replace('"', "\"\"");
    return '"' + s + '"';
}

// 15 significant digits: lossless for any physically meaningful value.
static QString num(double v) { return QString::number(v, 'g', 15); }

QString exportCsv(const QList<DriverRecord> &records)
{
    QString out;
    out += "id,make,model,date,measured_by,"
           "Re,fs,Zmax,f1,f2,deltaM_kg,fo,Dd_m,Zmin,f3,"
           "Z12,R0,Qms,Qes,Qts,mms_kg,Rms,BL,Cms,Sd_m2,Vas_m3,Le,"
           "Znom,fLe,KLe,Xmax_mm,Xlim_mm,Pe,Hg_mm,Vd_cm3,notes\n";

    for (const auto &r : records) {
        QStringList f;
        f << QString::number(r.id)
          << csvField(r.make)
          << csvField(r.model)
          << r.dateMeasured.toString("yyyy-MM-dd")
          << csvField(r.measuredBy)
          << num(r.Re)   << num(r.fs)   << num(r.Zmax)
          << num(r.f1)   << num(r.f2)   << num(r.deltaM)
          << num(r.fo)   << num(r.Dd)   << num(r.Zmin) << num(r.f3)
          << num(r.Z12)  << num(r.R0)   << num(r.Qms)
          << num(r.Qes)  << num(r.Qts)  << num(r.mms)
          << num(r.Rms)  << num(r.BL)   << num(r.Cms)
          << num(r.Sd)   << num(r.Vas)  << num(r.Le)
          << num(r.Znom) << num(r.fLe)  << num(r.KLe)
          << num(r.Xmax) << num(r.Xlim) << num(r.Pe)
          << num(r.Hg)   << num(r.Vd)
          << csvField(r.notes);
        out += f.join(',') + '\n';
    }
    return out;
}

ImportResult importCsv(const QString &text)
{
    ImportResult res;
    const auto rows = parseCsv(text);
    if (rows.isEmpty()) { res.error = "File is empty."; return res; }

    // Column-name → index map (case-insensitive)
    QMap<QString, int> colIdx;
    const QStringList &header = rows.first();
    for (int i = 0; i < header.size(); ++i)
        colIdx[header[i].trimmed().toLower()] = i;

    const QStringList required = {"make", "model", "re", "fs", "qts"};
    for (const auto &req : required) {
        if (!colIdx.contains(req)) {
            res.error = QString("Column '%1' not found in CSV header.\n"
                                "Expected format matches TSBoss Export CSV.").arg(req);
            return res;
        }
    }
    res.ok = true;

    for (int rowNo = 1; rowNo < rows.size(); ++rowNo) {
        const QStringList &f = rows[rowNo];

        auto raw = [&](const QString &col) -> QString {
            const int i = colIdx.value(col, -1);
            return (i >= 0 && i < f.size()) ? f[i] : QString{};
        };
        auto str = [&](const QString &col) { return raw(col).trimmed(); };
        auto dbl = [&](const QString &col) -> double {
            const QString s = str(col);
            if (s.isEmpty()) return 0.0;
            bool ok = false;
            const double v = s.toDouble(&ok);
            if (!ok) { res.issues.append({rowNo, col, s}); return 0.0; }
            return v;
        };

        DriverRecord r;
        r.id    = -1;                    // always insert as new
        r.make  = str("make");
        r.model = str("model");
        if (r.make.isEmpty() && r.model.isEmpty()) continue;  // skip blank rows

        r.measuredBy = str("measured_by");
        r.notes      = raw("notes");     // untrimmed: preserve exact text

        const QString ds = str("date");
        r.dateMeasured = QDate::fromString(ds, "yyyy-MM-dd");
        if (!r.dateMeasured.isValid()) {
            if (!ds.isEmpty()) res.issues.append({rowNo, "date", ds});
            r.dateMeasured = QDate::currentDate();
        }

        // Raw measurements
        r.Re     = dbl("re");
        r.fs     = dbl("fs");
        r.Zmax   = dbl("zmax");
        r.f1     = dbl("f1");
        r.f2     = dbl("f2");
        r.deltaM = dbl("deltam_kg");
        r.fo     = dbl("fo");
        r.Dd     = dbl("dd_m");
        r.Zmin   = dbl("zmin");
        r.f3     = dbl("f3");
        // Computed results
        r.Z12    = dbl("z12");
        r.R0     = dbl("r0");
        r.Qms    = dbl("qms");
        r.Qes    = dbl("qes");
        r.Qts    = dbl("qts");
        r.mms    = dbl("mms_kg");
        r.Rms    = dbl("rms");
        r.BL     = dbl("bl");
        r.Cms    = dbl("cms");
        r.Sd     = dbl("sd_m2");
        r.Vas    = dbl("vas_m3");
        r.Le     = dbl("le");
        // Additional linear parameters
        r.Znom   = dbl("znom");
        r.fLe    = dbl("fle");
        r.KLe    = dbl("kle");
        // Large signal parameters
        r.Xmax   = dbl("xmax_mm");
        r.Xlim   = dbl("xlim_mm");
        r.Pe     = dbl("pe");
        r.Hg     = dbl("hg_mm");
        r.Vd     = dbl("vd_cm3");

        res.records.append(r);
    }
    return res;
}

} // namespace drivercsv
