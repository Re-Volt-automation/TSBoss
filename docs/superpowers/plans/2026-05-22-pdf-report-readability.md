# PDF Report Readability Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the exported PDF digestible — two large plots per page with one shared legend, and a compact side-by-side comparative table (capped at 10 models) replacing the per-model pages.

**Architecture:** All changes live in `src/pdfreport.cpp`. The `buildHtml` function is restructured: a single shared legend (color swatch + `M#` + full name), plots packed two-per-page via explicit page breaks on even indices, and three grouped comparative tables (parameters as rows, `M1…M10` as columns). `buildModelSection` and the old `buildLegendHtml` are removed. A 10-model cap is enforced in `exportToFile`.

**Tech Stack:** Qt6 / C++17, `QTextDocument` HTML4/CSS subset, `QPdfWriter` (A4 portrait, 150 dpi). No test framework — verification is build success + regenerating `7x_Hertz.pdf` and inspecting it.

**Verification note:** There is no unit-test runner in this repo. Each task ends with a build and (where output changes) a regenerated-PDF inspection. Build command throughout:
```
cmake --build /home/wessel/projects/TSBoss/build --parallel
```
Inspection: run `/home/wessel/projects/TSBoss/build/TSBoss`, load the project that produced `7x_Hertz.pdf`, export, and open the result (or `pdftotext -layout out.pdf - | head` for a quick text check if `pdftotext` is installed).

---

### Task 1: Add helpers and the shared `M#` legend

**Files:**
- Modify: `src/pdfreport.cpp` (anonymous namespace, near `portShapeLabel` ~line 46; replace `buildLegendHtml` ~lines 178-190)

- [ ] **Step 1: Add `<functional>` include**

At the top includes (after `#include <cmath>`, line 4), add:

```cpp
#include <functional>
```

- [ ] **Step 2: Add `isBandpass` and `naCell` helpers**

Immediately after `portShapeLabel` (line 46), add:

```cpp
bool isBandpass(BoxModel::EncType t) {
    return t == BoxModel::EncType::Bandpass4
        || t == BoxModel::EncType::Bandpass6;
}
QString naCell() { return QStringLiteral("&mdash;"); }
QString modelLabel(int i) { return QStringLiteral("M%1").arg(i + 1); }
```

- [ ] **Step 3: Replace `buildLegendHtml` with `buildSharedLegend`**

Replace the entire `buildLegendHtml` function (lines 178-190) with:

```cpp
// One legend, referenced by both the plots and the comparison tables:
// colored swatch + short M# label + full model name.
QString buildSharedLegend(const QList<BoxModel>& filtered) {
    QString h = "<table cellpadding='2' cellspacing='0' "
                "style='font-size:8pt; margin-bottom:8pt;'>";
    for (int i = 0; i < filtered.size(); ++i) {
        const auto& m = filtered[i];
        const QString name = m.name.isEmpty()
            ? QStringLiteral("(unnamed)") : m.name.toHtmlEscaped();
        h += QStringLiteral(
            "<tr>"
            "<td><span style='display:inline-block; width:10pt; height:10pt; "
            "background:%1;'></span></td>"
            "<td><b>%2</b></td>"
            "<td>%3</td>"
            "</tr>")
            .arg(m.color.name(), modelLabel(i), name);
    }
    h += "</table>";
    return h;
}
```

- [ ] **Step 4: Build**

Run: `cmake --build /home/wessel/projects/TSBoss/build --parallel`
Expected: FAILS to compile — `buildLegendHtml` is still called in `buildHtml` (line 228) and `buildModelSection` is now unused but still defined (unused-function warning only, not an error). The hard error is the missing `buildLegendHtml` call. This is expected; Task 2 fixes the call site.

> Note: if the build blocks on the unused `buildModelSection`, it is only a warning unless `-Werror` is set (it is not in this project's `CMakeLists.txt`). The blocking error is the `buildLegendHtml` reference, resolved next task.

- [ ] **Step 5: Commit**

```bash
git add src/pdfreport.cpp
git commit -m "feat(pdf): shared M# legend + comparative-table helpers

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 2: Rework plot layout — two per page, larger, single legend

**Files:**
- Modify: `src/pdfreport.cpp` — `buildHtml` plot loop (lines 220-233), CSS block (lines 205-213), render sizes in `exportToFile` (line 320)

- [ ] **Step 1: Update the CSS block**

Replace the `<style>…</style>` literal (lines 206-213) with:

```cpp
        "<html><head><style>"
        "body { font-family: sans-serif; color: #111; }"
        "h1 { font-size: 22pt; margin-bottom: 4pt; }"
        "h2 { font-size: 16pt; margin-top: 6pt; margin-bottom: 4pt; }"
        "h3 { font-size: 12pt; margin-top: 12pt; margin-bottom: 3pt; }"
        ".meta { color: #555; font-size: 10pt; margin-bottom: 14pt; }"
        ".plot { margin-bottom: 10pt; }"
        ".pagebreak { page-break-before: always; }"
        "</style></head><body>");
```

- [ ] **Step 2: Replace the plot loop**

Replace the plot loop (lines 225-230) — the `for (int i = 0; i < 5; ++i)` block that emits a pagebreak, `<h2>`, `buildLegendHtml`, and a `width='720'` image — with:

```cpp
    // Two plots per page: force a page break before plots 0, 2, 4.
    // The shared legend sits once at the top of the first plots page.
    for (int i = 0; i < 5; ++i) {
        if (i % 2 == 0)
            html += QStringLiteral("<div class='pagebreak'></div>");
        if (i == 0)
            html += buildSharedLegend(filtered);
        html += QStringLiteral("<h2>%1</h2>").arg(titles[i]);
        html += QStringLiteral("<img class='plot' src='plot://%1' width='1000'>")
                .arg(keys[i]);
    }
```

- [ ] **Step 3: Bump render resolution**

In `exportToFile`, change the plot pixmap size (line 320) from:

```cpp
    const QSize plotPx(1600, 900);
```

to:

```cpp
    const QSize plotPx(2000, 1150);   // ~1.74:1 → 1000px wide scales to ~575px tall
```

- [ ] **Step 4: Build**

Run: `cmake --build /home/wessel/projects/TSBoss/build --parallel`
Expected: still FAILS — `buildModelSection` is still referenced in `buildHtml` (line 232-233). That call is replaced in Task 3. (If you want a clean intermediate build, you may temporarily comment the model-section loop, but it is fine to proceed to Task 3.)

- [ ] **Step 5: Commit**

```bash
git add src/pdfreport.cpp
git commit -m "feat(pdf): two larger plots per page with one shared legend

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 3: Comparative tables replace per-model pages

**Files:**
- Modify: `src/pdfreport.cpp` — remove `buildModelSection` (lines 48-134), add `buildComparativeTables`, update `buildHtml` model loop (lines 232-233)

- [ ] **Step 1: Delete `buildModelSection`**

Remove the entire `buildModelSection` function (lines 48-134, from `QString buildModelSection(` through its closing `}`). It is replaced by `buildComparativeTables`.

- [ ] **Step 2: Add `buildComparativeTables`**

Add this function in the anonymous namespace, just before `filterModels` (which is at line 136 originally):

```cpp
// A single comparison row: a label and a per-model value producer.
struct CmpRow {
    QString label;
    std::function<QString(const BoxModel&)> value;
};

// Render one grouped table. Rows whose value is naCell() for EVERY model
// are dropped, so type-specific rows (alpha, front fb, ports) only appear
// when at least one included model has them. Models are columns (M1..Mn).
QString buildCmpTable(const QString& heading,
                      const QList<CmpRow>& rows,
                      const QList<BoxModel>& filtered)
{
    QString h = QStringLiteral("<h3>%1</h3>").arg(heading);
    h += "<table border='1' cellpadding='2' cellspacing='0' "
         "style='font-size:8pt;'>";

    // Header row: blank corner + M1..Mn
    h += "<tr><td style='background:#eee;'><b>Parameter</b></td>";
    for (int i = 0; i < filtered.size(); ++i)
        h += QStringLiteral("<td style='background:#eee;'><b>%1</b></td>")
             .arg(modelLabel(i));
    h += "</tr>";

    for (const auto& row : rows) {
        // Skip rows that are N/A for all models.
        bool anyReal = false;
        QStringList cells;
        for (const auto& m : filtered) {
            const QString v = row.value(m);
            if (v != naCell()) anyReal = true;
            cells << v;
        }
        if (!anyReal) continue;

        h += QStringLiteral("<tr><td>%1</td>").arg(row.label);
        for (const QString& c : cells)
            h += QStringLiteral("<td align='right'>%1</td>").arg(c);
        h += "</tr>";
    }
    h += "</table>";
    return h;
}

QString buildComparativeTables(const QList<BoxModel>& filtered,
                               DriverDatabase* db,
                               double appliedPower)
{
    Q_UNUSED(db);

    QString h = "<div class='pagebreak'></div>";
    h += "<h2>Model Comparison</h2>";
    h += buildSharedLegend(filtered);

    // --- Driver T/S ---
    const QList<CmpRow> driver = {
        {"fs (Hz)",   [](const BoxModel& m){ return fmt(m.fs, 1); }},
        {"Vas (L)",   [](const BoxModel& m){ return fmt(m.Vas_L, 2); }},
        {"Qts",       [](const BoxModel& m){ return fmt(m.Qts, 3); }},
        {"Qes",       [](const BoxModel& m){ return fmt(m.Qes, 3); }},
        {"Qms",       [](const BoxModel& m){ return fmt(m.Qms, 3); }},
        {"Re (Ohm)",  [](const BoxModel& m){ return fmt(m.Re, 2); }},
        {"mms (g)",   [](const BoxModel& m){ return fmt(m.mms_g, 2); }},
        {"BL (Tm)",   [](const BoxModel& m){ return fmt(m.BL, 2); }},
        {"Sd (cm2)",  [](const BoxModel& m){ return fmt(m.Sd_cm2, 1); }},
        {"# drivers", [](const BoxModel& m){ return QString::number(m.numDrivers); }},
    };
    h += buildCmpTable("Driver T/S", driver, filtered);

    // --- Enclosure & Port ---
    const QList<CmpRow> enclosure = {
        {"Type",            [](const BoxModel& m){ return encTypeLabel(m.encType).toHtmlEscaped(); }},
        {"Volume (L)",      [](const BoxModel& m){ return fmt(m.volumeL, 2); }},
        {"fb (Hz)",         [](const BoxModel& m){ return hasPort(m.encType) ? fmt(m.fb, 1) : naCell(); }},
        {"QL",              [](const BoxModel& m){ return hasPort(m.encType) ? fmt(m.QL, 2) : naCell(); }},
        {"Front vol (L)",   [](const BoxModel& m){ return isBandpass(m.encType) ? fmt(m.volumeFront_L, 2) : naCell(); }},
        {"Front fb (Hz)",   [](const BoxModel& m){ return isBandpass(m.encType) ? fmt(m.fbFront, 1) : naCell(); }},
        {"alpha",           [](const BoxModel& m){ return m.encType == BoxModel::EncType::Sealed ? fmt(m.alpha, 3) : naCell(); }},
        {"Fc (Hz)",         [](const BoxModel& m){ return m.encType == BoxModel::EncType::Sealed ? fmt(m.Fc, 1) : naCell(); }},
        {"Qtc",             [](const BoxModel& m){ return m.encType == BoxModel::EncType::Sealed ? fmt(m.Qtc, 3) : naCell(); }},
        {"Port shape",      [](const BoxModel& m){ return hasPort(m.encType) ? portShapeLabel(m.portShape) : naCell(); }},
        {"Port size (mm)",  [](const BoxModel& m){
            if (!hasPort(m.encType)) return naCell();
            return m.portShape == 0
                ? QStringLiteral("D %1").arg(fmt(m.portWidth_mm, 1))
                : QStringLiteral("%1 x %2").arg(fmt(m.portWidth_mm, 1), fmt(m.portHeight_mm, 1));
        }},
        {"Shared walls",    [](const BoxModel& m){ return (hasPort(m.encType) && m.portShape != 0) ? QString::number(m.portWalls) : naCell(); }},
        {"# ports",         [](const BoxModel& m){ return hasPort(m.encType) ? QString::number(m.numPorts) : naCell(); }},
        {"2nd harmonic (Hz)", [](const BoxModel& m){ return (hasPort(m.encType) && m.portF2H > 0) ? fmt(m.portF2H, 1) : naCell(); }},
    };
    h += buildCmpTable("Enclosure & Port", enclosure, filtered);

    // --- Results & Output ---
    const double power = appliedPower;
    const QList<CmpRow> results = {
        {"f3 (Hz)",            [](const BoxModel& m){ return fmt(m.f3, 1); }},
        {"eta0 (%)",           [](const BoxModel& m){ return fmt(m.eta, 3); }},
        {"Ref SPL 1W/1m (dB)", [](const BoxModel& m){ return fmt(m.spl, 1); }},
        {"Applied power (W)",  [power](const BoxModel&){ return fmt(power, 2); }},
        {"Passband SPL (dB)",  [power](const BoxModel& m){
            return fmt(m.spl + 10.0 * std::log10(std::max(power, 1e-6)), 1); }},
        {"Xmax (mm)",          [](const BoxModel& m){ return m.xmax_mm > 0 ? fmt(m.xmax_mm, 2) : naCell(); }},
        {"Xlim (mm)",          [](const BoxModel& m){ return m.xlim_mm > 0 ? fmt(m.xlim_mm, 2) : naCell(); }},
    };
    h += buildCmpTable("Results & Output", results, filtered);

    return h;
}
```

- [ ] **Step 3: Replace the model loop in `buildHtml`**

In `buildHtml`, replace these two lines (originally 232-233):

```cpp
    for (const auto& m : filtered)
        html += buildModelSection(m, db, opts.appliedPower);
```

with:

```cpp
    html += buildComparativeTables(filtered, db, opts.appliedPower);
```

- [ ] **Step 4: Build**

Run: `cmake --build /home/wessel/projects/TSBoss/build --parallel`
Expected: PASS (compiles cleanly, no remaining reference to `buildModelSection` or `buildLegendHtml`).

- [ ] **Step 5: Regenerate and inspect the PDF**

Run `/home/wessel/projects/TSBoss/build/TSBoss`, reload the 7-model Hertz project, export to `/tmp/cmp.pdf`.
Expected: plots are large, two per page, with one shared M#/color/name legend; after the plots, a single "Model Comparison" page shows three grouped tables (Driver T/S, Enclosure & Port, Results & Output) with columns M1–M7. No per-model detail pages remain.
Quick text check (if `pdftotext` available):
```bash
pdftotext -layout /tmp/cmp.pdf - | grep -E "Model Comparison|Driver T/S|Enclosure & Port|Results & Output"
```
Expected: all four headings present.

- [ ] **Step 6: Commit**

```bash
git add src/pdfreport.cpp
git commit -m "feat(pdf): comparative tables replace per-model pages

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 4: Enforce the 10-model cap

**Files:**
- Modify: `src/pdfreport.cpp` — `exportToFile`, after `filtered` is built (after line 310)

- [ ] **Step 1: Add the cap check**

In `exportToFile`, immediately after:

```cpp
    const auto filtered = filterModels(models, chosen);
```

add:

```cpp
    if (filtered.size() > 10) {
        QMessageBox::information(parent, "PDF Export",
            QStringLiteral("This report compares up to 10 models. "
                           "You selected %1 — please choose 10 or fewer.")
                .arg(filtered.size()));
        return false;
    }
```

- [ ] **Step 2: Build**

Run: `cmake --build /home/wessel/projects/TSBoss/build --parallel`
Expected: PASS.

- [ ] **Step 3: Inspect behaviour**

Run `/home/wessel/projects/TSBoss/build/TSBoss`. With a project of 11+ models, choose "All models" and export.
Expected: a dialog "This report compares up to 10 models. You selected 11 — please choose 10 or fewer." and no file written. With ≤10 models, export proceeds normally.

- [ ] **Step 4: Commit**

```bash
git add src/pdfreport.cpp
git commit -m "feat(pdf): cap report at 10 models with clear message

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>"
```

---

### Task 5: Mixed-type verification pass

**Files:** none (verification only)

- [ ] **Step 1: Build (sanity)**

Run: `cmake --build /home/wessel/projects/TSBoss/build --parallel`
Expected: PASS.

- [ ] **Step 2: Export a mixed sealed/vented/bandpass project**

Run `/home/wessel/projects/TSBoss/build/TSBoss`. Build models of differing types (at least one sealed, one vented, one bandpass), export.
Expected in the "Model Comparison" tables:
- `alpha`, `Fc`, `Qtc` rows present, showing values for the sealed model and `—` for others.
- `fb`, `QL`, port rows show `—` for the sealed model, values for ported ones.
- `Front vol`, `Front fb` rows present only because a bandpass model exists; sealed/vented cells show `—`.
- No row is entirely `—` (e.g., if there is no sealed model at all, the alpha/Fc/Qtc rows are absent).

- [ ] **Step 3: Final confirmation (no commit — verification only)**

If all the above hold, the feature is complete. If any row is missing or wrongly dropped, revisit Task 3's `buildCmpTable` all-N/A skip logic.

---

## Self-Review

**Spec coverage:**
- Section 1 (plots): Task 2 — two per page, full width, larger render, single shared legend, old inline legend dropped. ✓ (`page-break-inside` was intentionally replaced by deterministic even-index page breaks, which `QTextDocument` supports reliably — a deliberate refinement over the spec's best-effort note.)
- Section 2 (comparative table): Task 3 — params as rows, M# columns, three groups, "—" for N/A, all-N/A rows dropped, 8pt font. ✓
- Section 3 (10-model cap): Task 4 — message + abort, no silent truncation. ✓
- Shared legend with M#/color/name: Task 1 + reused in Tasks 2 & 3. ✓

**Placeholder scan:** No TBD/TODO; every code step shows complete code. ✓

**Type consistency:** `buildSharedLegend(filtered)` signature consistent across Tasks 1/2/3. `naCell()`, `modelLabel(i)`, `isBandpass()`, `CmpRow`, `buildCmpTable`, `buildComparativeTables` all defined in Task 1/3 before use. `fmt`, `encTypeLabel`, `hasPort`, `portShapeLabel` are pre-existing helpers (lines 28-46). BoxModel fields referenced (`numDrivers`, `volumeFront_L`, `fbFront`, `portWalls`, `portF2H`, `numPorts`, `xmax_mm`, `xlim_mm`) all already used by the original `buildModelSection`, so they exist. ✓

**Note on intermediate builds:** Tasks 1 and 2 leave the tree non-compiling (known dangling references resolved in later tasks). This is called out in each task's build step. If strictly atomic green builds are required, Tasks 1–3 can be squashed into one commit.
