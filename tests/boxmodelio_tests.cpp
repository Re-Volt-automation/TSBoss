#include "boxmodelio.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <cstdio>
#include <cmath>

static int g_failures = 0;
static void check(bool cond, const char *msg) {
    if (!cond) { std::printf("FAIL: %s\n", msg); ++g_failures; }
    else       { std::printf("ok:   %s\n", msg); }
}

// A model with a non-default value in every serialized field.
static BoxModel makeFullModel()
{
    BoxModel m;
    m.name = "Test BP6";  m.autoName = true;  m.driverId = 42;
    m.fs = 27.5; m.Vas_L = 88.2; m.Qts = 0.41; m.Qes = 0.46;
    m.Qms = 4.8; m.Re = 3.4; m.mms_g = 155.0; m.BL = 14.2; m.Sd_cm2 = 510.0;
    m.encType = BoxModel::EncType::Bandpass6;
    m.volumeL = 55.0; m.fb = 31.0; m.QL = 8.5;
    m.volumeFront_L = 28.0; m.fbFront = 72.0; m.QLFront = 6.0;
    m.portFrontShape = 1; m.portFrontWidth_mm = 120.0; m.portFrontHeight_mm = 40.0;
    m.portFrontWalls = 2; m.numPortsFront = 2;
    m.portFrontWallThick_mm = 6.0; m.portFrontInsertDepth_mm = 25.0;
    m.portFrontExtraSurfArea_cm2 = 12.0; m.portFrontFlare = 1;
    m.portShape = 1; m.portWidth_mm = 100.0; m.portHeight_mm = 60.0;
    m.portWalls = 3; m.numPorts = 4;
    m.portWallThick_mm = 4.5; m.portInsertDepth_mm = 10.0;
    m.portExtraSurfArea_cm2 = 8.0; m.portFlare = 2;
    m.numDrivers = 2; m.wiringMode = BoxModel::WiringMode::Parallel;
    m.isDVC = true; m.dvcWiring = 1;
    m.addedMass_g = 15.0; m.visible = false;
    m.hpfOrder = 4; m.hpfFreq = 22.5;
    m.color = QColor("#d97706");
    return m;
}

int main()
{
    using namespace boxmodelio;

    // ── Full round-trip ───────────────────────────────────────────
    {
        const BoxModel a = makeFullModel();
        const BoxModel b = modelFromJson(modelToJson(a));
        check(b.name == a.name && b.autoName == a.autoName, "roundtrip: name/autoName");
        check(b.driverId == a.driverId,                     "roundtrip: driverId");
        check(b.fs == a.fs && b.Vas_L == a.Vas_L
              && b.Qts == a.Qts && b.Qes == a.Qes,          "roundtrip: primary T/S");
        check(b.Qms == a.Qms && b.Re == a.Re && b.mms_g == a.mms_g
              && b.BL == a.BL && b.Sd_cm2 == a.Sd_cm2,      "roundtrip: secondary T/S");
        check(b.encType == a.encType,                       "roundtrip: encType BP6");
        check(b.volumeL == a.volumeL && b.fb == a.fb && b.QL == a.QL,
                                                            "roundtrip: rear chamber");
        check(b.volumeFront_L == a.volumeFront_L && b.fbFront == a.fbFront
              && b.QLFront == a.QLFront,                    "roundtrip: front chamber");
        check(b.portShape == a.portShape && b.portWidth_mm == a.portWidth_mm
              && b.portHeight_mm == a.portHeight_mm && b.portWalls == a.portWalls
              && b.numPorts == a.numPorts,                  "roundtrip: rear port geometry");
        check(b.portWallThick_mm == a.portWallThick_mm
              && b.portInsertDepth_mm == a.portInsertDepth_mm
              && b.portExtraSurfArea_cm2 == a.portExtraSurfArea_cm2
              && b.portFlare == a.portFlare,                "roundtrip: rear port extras");
        check(b.portFrontShape == a.portFrontShape
              && b.portFrontWidth_mm == a.portFrontWidth_mm
              && b.portFrontHeight_mm == a.portFrontHeight_mm
              && b.portFrontWalls == a.portFrontWalls
              && b.numPortsFront == a.numPortsFront
              && b.portFrontWallThick_mm == a.portFrontWallThick_mm
              && b.portFrontInsertDepth_mm == a.portFrontInsertDepth_mm
              && b.portFrontExtraSurfArea_cm2 == a.portFrontExtraSurfArea_cm2
              && b.portFrontFlare == a.portFrontFlare,      "roundtrip: front port geometry");
        check(b.numDrivers == a.numDrivers && b.wiringMode == a.wiringMode,
                                                            "roundtrip: drivers/wiring");
        check(b.isDVC == a.isDVC && b.dvcWiring == a.dvcWiring, "roundtrip: DVC");
        check(b.addedMass_g == a.addedMass_g,               "roundtrip: added mass");
        check(b.hpfOrder == a.hpfOrder && b.hpfFreq == a.hpfFreq,
                                                            "roundtrip: subsonic filter");
        check(b.visible == a.visible,                       "roundtrip: visibility");
        check(b.color == a.color,                           "roundtrip: color");
    }

    // ── Envelopes carry type + version ────────────────────────────
    {
        const QJsonObject box = tsboxDocument(makeFullModel());
        check(box["type"].toString() == "tsbox",            "tsbox: type tag");
        check(box["version"].toInt() == kFormatVersion,     "tsbox: version written");
        check(box["model"].isObject(),                      "tsbox: model object present");

        const QJsonObject proj = tsprojDocument({makeFullModel(), BoxModel{}});
        check(proj["type"].toString() == "tsproj",          "tsproj: type tag");
        check(proj["version"].toInt() == kFormatVersion,    "tsproj: version written");
        check(proj["models"].toArray().size() == 2,         "tsproj: models array");
    }

    // ── Envelope load round-trip ──────────────────────────────────
    {
        const QByteArray json = QJsonDocument(tsboxDocument(makeFullModel())).toJson();
        const LoadResult res = loadTsbox(json);
        check(res.ok,                                       "loadTsbox: ok");
        check(res.version == kFormatVersion,                "loadTsbox: version read");
        check(res.models.size() == 1 && res.models[0].name == "Test BP6",
                                                            "loadTsbox: model loaded");
        BoxModel m1 = makeFullModel(), m2; m2.name = "Second";
        const QByteArray pjson = QJsonDocument(tsprojDocument({m1, m2})).toJson();
        const LoadResult pres = loadTsproj(pjson);
        check(pres.ok && pres.models.size() == 2
              && pres.models[1].name == "Second",           "loadTsproj: order preserved");
    }

    // ── Legacy (pre-version) files ────────────────────────────────
    {
        // Unversioned vented model the way old builds wrote it.
        const QByteArray legacy = QByteArray(R"({
            "type": "tsbox",
            "model": { "name": "Old vented", "isVented": true,
                       "portSharedWall": true, "fs": 30.0 }
        })");
        const LoadResult res = loadTsbox(legacy);
        check(res.ok,                                       "legacy: loads");
        check(res.version == 1,                             "legacy: missing version reads as 1");
        if (res.models.size() == 1) {
            const BoxModel &m = res.models[0];
            check(m.encType == BoxModel::EncType::Vented,   "legacy: isVented bool -> Vented");
            check(m.portWalls == 1,                         "legacy: portSharedWall -> 1 wall");
            check(m.autoName == false,                      "legacy: autoName defaults false");
            check(m.visible == true,                        "legacy: visible defaults true");
            check(!m.color.isValid(),                       "legacy: missing color stays invalid");
            check(m.volumeL == 40.0 && m.fb == 35.0 && m.QL == 7.0,
                                                            "legacy: chamber defaults");
            check(m.hpfOrder == 0 && m.hpfFreq == 20.0,     "legacy: filter defaults off");
        } else {
            check(false, "legacy: model count");
        }
    }
    {
        QJsonObject obj; obj["isInfiniteBaffle"] = true;
        check(modelFromJson(obj).encType == BoxModel::EncType::IB,
              "legacy: isInfiniteBaffle -> IB");
        check(modelFromJson(QJsonObject{}).encType == BoxModel::EncType::Sealed,
              "legacy: no type info -> Sealed");
    }

    // ── Robustness: clamps and floors on bad values ───────────────
    {
        QJsonObject obj;
        obj["portFlare"]  = 99;
        obj["wiringMode"] = 7;
        obj["numPorts"]   = 0;
        obj["portWalls"]  = -2;
        obj["hpfOrder"]   = 3;   // only 0/2/4 are meaningful
        const BoxModel m = modelFromJson(obj);
        check(m.portFlare == 2,                             "clamp: portFlare");
        check(m.wiringMode == BoxModel::WiringMode::Separate, "clamp: wiringMode to max");
        check(m.numPorts == 1,                              "clamp: numPorts floor");
        check(m.portWalls == 0,                             "clamp: portWalls floor");
        check(m.hpfOrder == 2,                              "clamp: odd hpfOrder snaps down");
    }

    // ── Bad input rejected with an error ──────────────────────────
    {
        check(!loadTsbox("not json at all").ok,             "reject: garbage tsbox");
        check(!loadTsbox(R"({"type":"tsproj","models":[]})").ok,
                                                            "reject: tsproj fed to loadTsbox");
        check(!loadTsproj(R"({"type":"tsbox","model":{}})").ok,
                                                            "reject: tsbox fed to loadTsproj");
        check(!loadTsbox(R"({"type":"tsbox","model":{}})").error.isEmpty() == false
              || loadTsbox(R"({"type":"tsbox","model":{}})").ok,
                                                            "valid minimal tsbox accepted");
    }

    // ── Forward compat: newer version still loads, version reported ─
    {
        const QByteArray future = QByteArray(R"({
            "type": "tsbox", "version": 99,
            "model": { "name": "From the future" }
        })");
        const LoadResult res = loadTsbox(future);
        check(res.ok && res.version == 99,                  "forward: newer version tolerated");
    }

    if (g_failures) std::printf("\n%d FAILURE(S)\n", g_failures);
    else            std::printf("\nAll boxmodelio tests passed.\n");
    return g_failures;
}
