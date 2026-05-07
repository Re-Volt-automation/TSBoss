# Save File Auto-Naming — Design Spec
_2026-05-07_

## Summary

Replace the static / display-name-derived default filenames in the Save dialogs with meaningful, filesystem-safe names built from the model's driver model number, enclosure type, and volume (`.tsbox`), or model count and driver brands (`.tsproj`).

---

## Sanitization Helper

A single static helper in `enclosurewidget.cpp`:

```cpp
static QString sanitizeFilename(const QString &s)
{
    QString out = s;
    out.replace(" / ", "_");
    out.replace(' ', '_');
    // strip characters not safe in filenames cross-platform
    out.remove(QRegularExpression(R"([^\w\-+.])"));
    // collapse runs of underscores
    out.replace(QRegularExpression("_+"), "_");
    out = out.trimmed();
    return out.isEmpty() ? "enclosure" : out;
}
```

`\w` in Qt's regex covers `[A-Za-z0-9_]`.

---

## `.tsbox` Default Filename

**Source:** `model.name` — the auto-generated display name produced by `refreshAutoName()`.

Format of `model.name` for auto-named models:
```
"{driverComboText} / {vol} L / {type}"   e.g. "SB17MFC-4 / 35 L / vented"
"{driverComboText} / IB"                 for infinite baffle
"{driverComboText} / {front}+{rear} L / BP6"
```

**Filename stem:** `sanitizeFilename(model.name)`

Examples:
| `model.name` | Filename |
|---|---|
| `SB17MFC-4 / 35 L / vented` | `SB17MFC-4_35_L_vented.tsbox` |
| `RS225-8 / 20 L / sealed` | `RS225-8_20_L_sealed.tsbox` |
| `SB17MFC-4 / IB` | `SB17MFC-4_IB.tsbox` |
| `SB17MFC-4 / 12+25 L / BP6` | `SB17MFC-4_12+25_L_BP6.tsbox` |

If `model.autoName` is false (user-renamed), `model.name` is the user's chosen name — sanitize it the same way.

**Change in `onSaveModel()`:**

```cpp
// Before:
QDir::homePath() + "/" + model.name + ".tsbox"

// After:
QDir::homePath() + "/" + sanitizeFilename(model.name) + ".tsbox"
```

---

## `.tsproj` Default Filename

**Source:** all models in `m_models`.

**Algorithm:**
1. `N` = `m_models.size()`
2. Collect unique `DriverRecord.make` values: for each model where `driverId >= 0`, call `m_db->loadDriver(driverId)`, take `.make`. Deduplicate, sort alphabetically.
3. Sanitize each brand with `sanitizeFilename`, join with `-`.
4. Stem = `QString("%1x_%2").arg(N).arg(brandsStr)`. If no brands resolved, stem = `"enclosure"`.

Examples:
| Models | Brands | Filename |
|---|---|---|
| 3 | SB Acoustics, Dayton Audio | `3x_Dayton_Audio-SB_Acoustics.tsproj` |
| 1 | Peerless | `1x_Peerless.tsproj` |
| 2 | (none linked) | `enclosure.tsproj` (unchanged fallback) |
| 2 | same brand twice | `2x_SB_Acoustics.tsproj` (deduplicated) |

**Change in `onSaveProject()`:**

Replace:
```cpp
QDir::homePath() + "/enclosure.tsproj"
```

With the constructed stem above + `".tsproj"`.

---

## Constraints

- `sanitizeFilename` is the single sanitization function — no duplication.
- User still sees the file dialog and can rename before saving — auto-name is only the pre-filled suggestion.
- DB lookups in `onSaveProject` are read-only and happen at save time only (not on every model change).
- No changes to JSON format, `BoxModel` struct, or `model.name` logic.
