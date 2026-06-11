#include "drivercsv.h"
#include <QDate>
#include <cstdio>
#include <cmath>
#include <algorithm>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}
static bool near(double a, double b, double rel = 1e-12) {
    return std::abs(a - b) <= rel * std::max({std::abs(a), std::abs(b), 1.0});
}

int main() {
    using namespace drivercsv;

    // ── parseCsv: structure ───────────────────────────────────────
    {
        const auto rows = parseCsv("a,b,c\n1,2,3\n");
        check(rows.size() == 2, "parseCsv: two records");
        check(rows.size() == 2 && rows[0] == QStringList({"a","b","c"}), "parseCsv: header fields");
        check(rows.size() == 2 && rows[1] == QStringList({"1","2","3"}), "parseCsv: data fields");
    }
    {
        const auto rows = parseCsv("\"a,b\",c");
        check(rows.size() == 1 && rows[0].size() == 2 && rows[0][0] == "a,b",
              "parseCsv: quoted comma stays one field");
    }
    {
        const auto rows = parseCsv("\"He said \"\"hi\"\"\",x");
        check(rows.size() == 1 && rows[0][0] == "He said \"hi\"",
              "parseCsv: doubled quotes unescaped");
    }
    {
        const auto rows = parseCsv("\"line1\nline2\",x\nnext,y");
        check(rows.size() == 2, "parseCsv: quoted newline keeps one record");
        check(rows.size() == 2 && rows[0][0] == "line1\nline2",
              "parseCsv: newline preserved inside field");
        check(rows.size() == 2 && rows[1][0] == "next",
              "parseCsv: record after quoted newline intact");
    }
    {
        const auto rows = parseCsv("a,b\r\nc,d\r\n");
        check(rows.size() == 2 && rows[1] == QStringList({"c","d"}),
              "parseCsv: CRLF line endings");
    }
    {
        const auto rows = parseCsv("a,b\n\n\nc,d\n");
        check(rows.size() == 2, "parseCsv: blank lines skipped");
    }

    // ── csvField: escaping ────────────────────────────────────────
    check(csvField("abc") == "abc",                       "csvField: plain text unquoted");
    check(csvField("a,b") == "\"a,b\"",                   "csvField: comma forces quotes");
    check(csvField("say \"hi\"") == "\"say \"\"hi\"\"\"", "csvField: quotes doubled");
    check(csvField("two\nlines") == "\"two\nlines\"",     "csvField: newline forces quotes");

    // ── export → import round-trip ────────────────────────────────
    {
        DriverRecord r;
        r.make         = "Dayton, Audio";                    // comma
        r.model        = "UM18-22 \"Ultimax\"";              // quotes
        r.notes        = "It's \"quoted\"\nsecond line";     // apostrophe + quote + newline
        r.measuredBy   = "Wessel";
        r.dateMeasured = QDate(2026, 6, 11);
        r.Re = 3.1; r.fs = 22.5; r.Qts = 0.53; r.Qms = 5.1; r.Qes = 0.59;
        r.Cms = 1.23456789012e-4;                            // precision check
        r.Vas = 0.231; r.BL = 18.7; r.mms = 0.265;
        r.Xmax = 22.0;

        const QString text = exportCsv({r});
        const ImportResult res = importCsv(text);
        check(res.ok,                  "roundtrip: import ok");
        check(res.issues.isEmpty(),    "roundtrip: no parse issues");
        check(res.records.size() == 1, "roundtrip: one record");
        if (res.records.size() == 1) {
            const DriverRecord &g = res.records[0];
            check(g.make  == r.make,   "roundtrip: make with comma survives");
            check(g.model == r.model,  "roundtrip: model with quotes survives");
            check(g.notes == r.notes,  "roundtrip: notes with quote+newline survive");
            check(g.dateMeasured == r.dateMeasured, "roundtrip: date survives");
            check(near(g.Cms, r.Cms),  "roundtrip: Cms full precision");
            check(near(g.fs,  r.fs),   "roundtrip: fs");
            check(near(g.Vas, r.Vas),  "roundtrip: Vas");
            check(near(g.Xmax, r.Xmax),"roundtrip: Xmax");
        }
    }

    // ── import: required column missing ───────────────────────────
    {
        const ImportResult res = importCsv("make,model,re,fs\nA,B,4,30\n");
        check(!res.ok, "import: missing qts column rejected");
        check(res.error.contains("qts", Qt::CaseInsensitive),
              "import: error names the missing column");
    }

    // ── import: unparseable numbers reported, not silently zeroed ─
    {
        const QString text =
            "make,model,re,fs,qts\n"
            "Acme,W1,4.0,\"28,5\",0.4\n"     // European decimal comma in fs
            "Acme,W2,oops,30,0.38\n";        // garbage Re
        const ImportResult res = importCsv(text);
        check(res.ok,                  "import: bad cells are not fatal");
        check(res.records.size() == 2, "import: both rows imported");
        check(res.issues.size() == 2,  "import: two issues reported");
        if (res.issues.size() == 2) {
            check(res.issues[0].row == 1 && res.issues[0].column == "fs"
                  && res.issues[0].value == "28,5", "import: issue 1 row/col/value");
            check(res.issues[1].row == 2 && res.issues[1].column == "re"
                  && res.issues[1].value == "oops", "import: issue 2 row/col/value");
        }
        if (res.records.size() == 2)
            check(res.records[0].fs == 0.0, "import: bad cell imported as 0");
    }

    // ── import: empty optional cells are fine ─────────────────────
    {
        const ImportResult res =
            importCsv("make,model,re,fs,qts,bl\nAcme,W3,4,30,0.4,\n");
        check(res.ok && res.issues.isEmpty(), "import: empty cell raises no issue");
        check(res.records.size() == 1 && res.records[0].BL == 0.0,
              "import: empty cell -> 0");
    }

    // ── import: case-insensitive header, blank rows, id forced ────
    {
        const ImportResult res =
            importCsv("ID,MAKE,Model,RE,FS,QTS\n7,Acme,W4,4,30,0.4\n,,,,,\n");
        check(res.ok && res.records.size() == 1,
              "import: case-insensitive header, blank row skipped");
        if (res.records.size() == 1)
            check(res.records[0].id == -1, "import: id forced to -1");
    }

    // ── import: bad date reported, falls back to a valid date ─────
    {
        const ImportResult res = importCsv(
            "make,model,re,fs,qts,date\nAcme,W5,4,30,0.4,31-12-2024\n");
        check(res.records.size() == 1, "import: bad-date row still imported");
        check(res.issues.size() == 1 && res.issues[0].column == "date",
              "import: bad date reported");
        if (res.records.size() == 1)
            check(res.records[0].dateMeasured.isValid(),
                  "import: bad date falls back to valid date");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll drivercsv tests passed.\n");
    return g_failures;
}
