#include "boxmodelio.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <algorithm>

namespace boxmodelio {

QJsonObject modelToJson(const BoxModel &m)
{
    QJsonObject obj;
    obj["name"]      = m.name;
    obj["driverId"]  = m.driverId;
    obj["fs"]        = m.fs;
    obj["Vas_L"]     = m.Vas_L;
    obj["Qts"]       = m.Qts;
    obj["Qes"]       = m.Qes;
    obj["Qms"]       = m.Qms;
    obj["Re"]        = m.Re;
    obj["mms_g"]     = m.mms_g;
    obj["BL"]        = m.BL;
    obj["Sd_cm2"]    = m.Sd_cm2;
    obj["volumeL"]   = m.volumeL;
    {
        const char *s = "sealed";
        switch (m.encType) {
        case BoxModel::EncType::Sealed:    s = "sealed";    break;
        case BoxModel::EncType::Vented:    s = "vented";    break;
        case BoxModel::EncType::IB:        s = "ib";        break;
        case BoxModel::EncType::Bandpass4: s = "bandpass4"; break;
        case BoxModel::EncType::Bandpass6: s = "bandpass6"; break;
        }
        obj["encType"] = s;
    }
    // Legacy bools so builds that predate encType can still read the file
    obj["isVented"] = m.encType == BoxModel::EncType::Vented
                   || m.encType == BoxModel::EncType::Bandpass4
                   || m.encType == BoxModel::EncType::Bandpass6;
    obj["isInfiniteBaffle"] = m.encType == BoxModel::EncType::IB;
    obj["fb"]             = m.fb;
    obj["QL"]             = m.QL;
    // Bandpass front-chamber persistence
    obj["volumeFront_L"]    = m.volumeFront_L;
    obj["fbFront"]          = m.fbFront;
    obj["QLFront"]          = m.QLFront;
    obj["portFrontShape"]      = m.portFrontShape;
    obj["portFrontWidth_mm"]   = m.portFrontWidth_mm;
    obj["portFrontHeight_mm"]  = m.portFrontHeight_mm;
    obj["portFrontWalls"]      = m.portFrontWalls;
    obj["numPortsFront"]       = m.numPortsFront;
    obj["portFrontWallThick_mm"]      = m.portFrontWallThick_mm;
    obj["portFrontInsertDepth_mm"]    = m.portFrontInsertDepth_mm;
    obj["portFrontExtraSurfArea_cm2"] = m.portFrontExtraSurfArea_cm2;
    obj["portFrontFlare"]             = m.portFrontFlare;
    obj["portShape"]           = m.portShape;
    obj["portWidth_mm"]        = m.portWidth_mm;
    obj["portHeight_mm"]       = m.portHeight_mm;
    obj["portWalls"]           = m.portWalls;
    obj["numPorts"]            = m.numPorts;
    obj["numDrivers"]          = m.numDrivers;
    obj["wiringMode"]          = static_cast<int>(m.wiringMode);
    obj["portWallThick_mm"]       = m.portWallThick_mm;
    obj["portInsertDepth_mm"]     = m.portInsertDepth_mm;
    obj["portExtraSurfArea_cm2"]  = m.portExtraSurfArea_cm2;
    obj["portFlare"]              = m.portFlare;
    obj["autoName"]            = m.autoName;
    obj["color"]               = m.color.name();
    obj["isDVC"]               = m.isDVC;
    obj["dvcWiring"]           = m.dvcWiring;
    obj["addedMass_g"]         = m.addedMass_g;
    obj["visible"]             = m.visible;
    return obj;
}

BoxModel modelFromJson(const QJsonObject &obj)
{
    BoxModel m;
    m.name      = obj["name"].toString();
    m.driverId  = obj["driverId"].toInt(-1);
    m.fs        = obj["fs"].toDouble();
    m.Vas_L     = obj["Vas_L"].toDouble();
    m.Qts       = obj["Qts"].toDouble();
    m.Qes       = obj["Qes"].toDouble();
    m.Qms       = obj["Qms"].toDouble();
    m.Re        = obj["Re"].toDouble();
    m.mms_g     = obj["mms_g"].toDouble();
    m.BL        = obj["BL"].toDouble();
    m.Sd_cm2    = obj["Sd_cm2"].toDouble();
    m.volumeL   = obj["volumeL"].toDouble(40.0);
    if (obj.contains("encType")) {
        const QString s = obj["encType"].toString("sealed");
        if      (s == "vented")    m.encType = BoxModel::EncType::Vented;
        else if (s == "ib")        m.encType = BoxModel::EncType::IB;
        else if (s == "bandpass4") m.encType = BoxModel::EncType::Bandpass4;
        else if (s == "bandpass6") m.encType = BoxModel::EncType::Bandpass6;
        else                       m.encType = BoxModel::EncType::Sealed;
    } else if (obj["isInfiniteBaffle"].toBool(false)) {
        m.encType = BoxModel::EncType::IB;
    } else if (obj["isVented"].toBool(false)) {
        m.encType = BoxModel::EncType::Vented;
    } else {
        m.encType = BoxModel::EncType::Sealed;
    }
    m.fb             = obj["fb"].toDouble(35.0);
    m.QL             = obj["QL"].toDouble(7.0);
    m.volumeFront_L      = obj["volumeFront_L"].toDouble(40.0);
    m.fbFront            = obj["fbFront"].toDouble(60.0);
    m.QLFront            = obj["QLFront"].toDouble(7.0);
    m.portFrontShape     = std::clamp(obj["portFrontShape"].toInt(0), 0, 1);
    m.portFrontWidth_mm  = obj["portFrontWidth_mm"].toDouble(75.0);
    m.portFrontHeight_mm = obj["portFrontHeight_mm"].toDouble(50.0);
    m.portFrontWalls     = std::clamp(obj["portFrontWalls"].toInt(0), 0, 3);
    m.numPortsFront      = std::max(1, obj["numPortsFront"].toInt(1));
    m.portFrontWallThick_mm   = obj["portFrontWallThick_mm"].toDouble(3.0);
    m.portFrontInsertDepth_mm = obj["portFrontInsertDepth_mm"].toDouble(0.0);
    m.portFrontExtraSurfArea_cm2 = obj["portFrontExtraSurfArea_cm2"].toDouble(0.0);
    m.portFrontFlare     = std::clamp(obj["portFrontFlare"].toInt(0), 0, 2);
    m.portShape     = obj["portShape"].toInt(0);
    m.portWidth_mm  = obj["portWidth_mm"].toDouble(75.0);
    m.portHeight_mm = obj["portHeight_mm"].toDouble(50.0);
    // portWalls: fall back to old portSharedWall bool for backward compat
    if (obj.contains("portWalls"))
        m.portWalls = std::clamp(obj["portWalls"].toInt(0), 0, 3);
    else
        m.portWalls = obj["portSharedWall"].toBool(false) ? 1 : 0;
    m.numPorts    = std::max(1, obj["numPorts"].toInt(1));
    m.numDrivers  = std::max(1, obj["numDrivers"].toInt(1));
    m.wiringMode  = static_cast<BoxModel::WiringMode>(
                        std::clamp(obj["wiringMode"].toInt(0), 0, 2));
    m.portWallThick_mm        = obj["portWallThick_mm"].toDouble(3.0);
    m.portInsertDepth_mm      = obj["portInsertDepth_mm"].toDouble(0.0);
    m.portExtraSurfArea_cm2   = obj["portExtraSurfArea_cm2"].toDouble(0.0);
    m.portFlare               = std::clamp(obj["portFlare"].toInt(0), 0, 2);
    // autoName: old files lack the key → treat as user-named to preserve saved names
    m.autoName  = obj["autoName"].toBool(false);
    m.isDVC     = obj["isDVC"].toBool(false);
    m.dvcWiring = obj["dvcWiring"].toInt(0);
    m.addedMass_g = obj["addedMass_g"].toDouble(0.0);
    m.visible     = obj["visible"].toBool(true);
    const QString cstr = obj["color"].toString();
    m.color = cstr.isEmpty() ? QColor() : QColor(cstr);
    return m;
}

QJsonObject tsboxDocument(const BoxModel &m)
{
    QJsonObject root;
    root["type"]    = "tsbox";
    root["version"] = kFormatVersion;
    root["model"]   = modelToJson(m);
    return root;
}

QJsonObject tsprojDocument(const QList<BoxModel> &models)
{
    QJsonObject root;
    root["type"]    = "tsproj";
    root["version"] = kFormatVersion;
    QJsonArray arr;
    for (const auto &m : models)
        arr.append(modelToJson(m));
    root["models"] = arr;
    return root;
}

// Shared envelope validation: parse, check type tag, read version.
static bool openEnvelope(const QByteArray &json, const char *type,
                         const char *payloadKey, LoadResult &res, QJsonObject &root)
{
    QJsonParseError perr;
    const auto doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError) {
        res.error = "Not valid JSON: " + perr.errorString();
        return false;
    }
    root = doc.object();
    if (root["type"].toString() != type || !root.contains(payloadKey)) {
        res.error = QString("Not a valid .%1 file.").arg(type);
        return false;
    }
    // Missing key predates versioning. Newer versions are tolerated — unknown
    // fields are ignored and missing ones default; callers may warn.
    res.version = root["version"].toInt(1);
    return true;
}

LoadResult loadTsbox(const QByteArray &json)
{
    LoadResult res;
    QJsonObject root;
    if (!openEnvelope(json, "tsbox", "model", res, root)) return res;
    res.models.append(modelFromJson(root["model"].toObject()));
    res.ok = true;
    return res;
}

LoadResult loadTsproj(const QByteArray &json)
{
    LoadResult res;
    QJsonObject root;
    if (!openEnvelope(json, "tsproj", "models", res, root)) return res;
    const auto arr = root["models"].toArray();
    for (const auto &v : arr)
        res.models.append(modelFromJson(v.toObject()));
    res.ok = true;
    return res;
}

} // namespace boxmodelio
