# PDF Report Export Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a PDF report export button to TSBoss's EnclosureWidget that produces a portable A4 PDF containing five combined overlay plots followed by per-model parameter breakdowns.

**Architecture:** New `PdfReport` class builds an HTML document (tables + `<img>` tags), rasterises the five existing plot widgets into pixmaps via `QWidget::render()`, embeds them as `QTextDocument` resources, and prints to a `QPdfWriter`. An "Export PDF…" button in EnclosureWidget's left panel opens a small scope dialog, then a file save dialog. The auto-naming logic from `onSaveProject` is extracted to a reusable static helper.

**Tech Stack:** Qt6 Widgets + PrintSupport (already linked), `QPdfWriter`, `QTextDocument`, `QPixmap`, `QFileDialog`, `QMessageBox`. No new dependencies.

**Reference spec:** `docs/superpowers/specs/2026-05-18-pdf-report-export-design.md`

**Testing note:** TSBoss has no automated test harness. Verification is build-clean + manual smoke checks; each task ends with a build step and (for UI-visible tasks) a brief manual verification list.

---

## File Map

| File | Action | Responsibility |
|------|--------|----------------|
| `src/pdfreport.h` | Create | `PdfReportOptions` struct, `PdfReport::exportToFile` declaration |
| `src/pdfreport.cpp` | Create | HTML builder, plot rasteriser, PDF writer, scope dialog |
| `src/enclosurewidget.h` | Modify | Add `m_btnExportPdf` member, `onExportPdf()` slot, `deriveProjectFileBaseName()` static helper |
| `src/enclosurewidget.cpp` | Modify | Wire button into left panel, implement slot, refactor `onSaveProject` to use the new helper |
| `CMakeLists.txt` | Modify | Add `src/pdfreport.h` and `src/pdfreport.cpp` to `PROJECT_SOURCES` |

---

## Task 1: Extract the auto-naming helper from `onSaveProject`

**Files:**
- Modify: `src/enclosurewidget.h` (add declaration)
- Modify: `src/enclosurewidget.cpp` (around lines 5203–5252)

This refactor lets both `onSaveProject` and the new `onExportPdf` reuse the same brand-aggregation filename logic.

- [ ] **Step 1: Add the static helper declaration**

In `src/enclosurewidget.h`, under the `private:` section of `EnclosureWidget` (find the block starting with `void    buildUi();` around line 341), add the following declaration just below `void deserializeModels(const QString &json);`:

```cpp
    /// Build a default filename stem from the loaded models:
    /// "Nx_Brand1-Brand2" with hyphens in brand names escaped to underscores.
    /// Brands are deduplicated case-insensitively and sorted; driver IDs are
    /// deduplicated so the same driver isn't queried twice. Falls back to
    /// "enclosure" when no brands resolve.
    static QString deriveProjectFileBaseName(const QList<BoxModel> &models,
                                             DriverDatabase *db);
```

- [ ] **Step 2: Implement the helper in the .cpp**

In `src/enclosurewidget.cpp`, find `void EnclosureWidget::onSaveProject()` (line ~5203). Just above that function, add the new helper implementation. (Place it above onSaveProject so its definition is visible from both call sites.)

```cpp
QString EnclosureWidget::deriveProjectFileBaseName(const QList<BoxModel> &models,
                                                   DriverDatabase *db)
{
    QStringList brands;
    QSet<int> seenIds;
    for (const auto &m : models) {
        if (m.driverId >= 0 && !seenIds.contains(m.driverId) &&
            db && db->isOpen()) {
            seenIds.insert(m.driverId);
            const auto r = db->loadDriver(m.driverId);
            if (!r.make.isEmpty()) {
                QString b = sanitizeFilename(r.make);
                b.replace('-', '_');
                if (!brands.contains(b, Qt::CaseInsensitive))
                    brands.append(b);
            }
        }
    }
    brands.sort(Qt::CaseInsensitive);
    return brands.isEmpty()
        ? QStringLiteral("enclosure")
        : QString("%1x_%2").arg(models.size()).arg(brands.join('-'));
}
```

- [ ] **Step 3: Replace the inline logic in `onSaveProject`**

In `src/enclosurewidget.cpp`, replace the body of `onSaveProject` from the `if (m_models.isEmpty())` check through the `const QString stem = ...` line. The new top of the function should read:

```cpp
void EnclosureWidget::onSaveProject()
{
    if (m_models.isEmpty()) {
        QMessageBox::information(this, "No models",
            "Add at least one model before saving a project.");
        return;
    }
    const QString stem = deriveProjectFileBaseName(m_models, m_db);

    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project", QDir::homePath() + "/" + stem + ".tsproj",
        "TSBoss Project (*.tsproj)");
    if (path.isEmpty()) return;
    // ... rest of function unchanged (QJsonObject root through final setText)
```

Delete the now-redundant `QStringList brands;`, `QSet<int> seenIds;`, the `for` loop, the `brands.sort` call, and the inline `stem` ternary that the helper now replaces. Keep everything from `const QString path = QFileDialog::getSaveFileName(...)` onward.

- [ ] **Step 4: Build to verify the refactor compiles cleanly**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build, no warnings about the new helper. If `sanitizeFilename` is a static free function in `enclosurewidget.cpp` (not a member), it's fine because both the helper and `onSaveProject` live in the same TU.

- [ ] **Step 5: Manual smoke check**

Launch `./TSBoss`, open the Enclosure tab, add a model from a known driver (e.g. one with make "Dayton"), click **Save Project…**. Verify the suggested filename in the dialog still reads `1x_Dayton.tsproj` (or equivalent). Cancel the dialog.

- [ ] **Step 6: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/enclosurewidget.h src/enclosurewidget.cpp && git commit -m "$(cat <<'EOF'
refactor: extract deriveProjectFileBaseName helper

Pulled the brand-aggregation filename logic out of onSaveProject into
a static helper so it can be reused by the upcoming PDF export feature.
No behaviour change.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: Create the `PdfReport` skeleton (header + empty implementation)

**Files:**
- Create: `src/pdfreport.h`
- Create: `src/pdfreport.cpp`
- Modify: `CMakeLists.txt`

Get the new TU compiling and linked before adding logic.

- [ ] **Step 1: Write `src/pdfreport.h`**

Create `src/pdfreport.h`:

```cpp
#pragma once
#include <QString>
#include <QList>

class QWidget;
class DriverDatabase;
struct BoxModel;

struct PdfReportOptions {
    QString    projectName;       ///< Used for cover page title and default filename stem.
    QList<int> selectedIndices;   ///< Indices into the models list to include.
    double     appliedPower = 1.0;///< Watts to pass to power-aware plots (SPL, Voltage, Excursion, Port Velocity).
    bool       perDriverMode = false; ///< Mirror of the on-screen Per-Driver toggle.
};

class PdfReport {
public:
    /// Show a save-file dialog and write a PDF report to the chosen path.
    /// Returns true on success, false on cancel or I/O error.
    /// Shows its own QMessageBox on failure.
    static bool exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db);
};
```

- [ ] **Step 2: Write `src/pdfreport.cpp` with a stub implementation**

Create `src/pdfreport.cpp`:

```cpp
#include "pdfreport.h"
#include "enclosurewidget.h"   // for BoxModel
#include "driverdb.h"          // for DriverDatabase
#include <QFileDialog>
#include <QMessageBox>
#include <QWidget>

bool PdfReport::exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db)
{
    Q_UNUSED(models);
    Q_UNUSED(opts);
    Q_UNUSED(db);
    QMessageBox::information(parent, "PDF Export",
        "PDF export is not yet implemented.");
    return false;
}
```

- [ ] **Step 3: Add the new files to `CMakeLists.txt`**

In `/home/wessel/projects/TSBoss/CMakeLists.txt`, find the `PROJECT_SOURCES` set() block (lines 12–35). Add two lines just after the `enclosurewidget.cpp` entry (currently line 32):

```cmake
    src/enclosurewidget.h
    src/enclosurewidget.cpp
    src/pdfreport.h
    src/pdfreport.cpp
    src/theme.h
```

(Insert `src/pdfreport.h` and `src/pdfreport.cpp` between the existing `enclosurewidget.cpp` and `src/theme.h` lines.)

- [ ] **Step 4: Build to verify the new TU links**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build. If CMake doesn't pick up the new sources, run `cmake ..` from the build directory first then rebuild.

- [ ] **Step 5: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/pdfreport.h src/pdfreport.cpp CMakeLists.txt && git commit -m "$(cat <<'EOF'
feat: add PdfReport skeleton

Empty class + options struct, wired into the build. Implementation
arrives in follow-up commits.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Wire the Export PDF button into EnclosureWidget

**Files:**
- Modify: `src/enclosurewidget.h`
- Modify: `src/enclosurewidget.cpp` (around line 2204 where btnSaveProject is created)

End state for this task: button visible, disabled when no models, opens stub dialog when clicked.

- [ ] **Step 1: Add the member and slot declarations**

In `src/enclosurewidget.h`, find the `private slots:` block (around line 315). Add `onExportPdf` to the existing save/load group:

```cpp
    void onSaveModel();
    void onLoadModel();
    void onSaveProject();
    void onLoadProject();
    void onExportPdf();
```

Then in the members area near `m_statusLbl` (around line 473), add the button pointer. Find this block and add the new line:

```cpp
    QLabel *m_statusLbl = nullptr;
    QPushButton *m_btnExportPdf = nullptr;
```

- [ ] **Step 2: Add the Export PDF button next to Save/Load Project**

In `src/enclosurewidget.cpp` around lines 2188–2206, the Save/Load buttons are built via a local `mkBtn` lambda and added to the `leftVb` QVBoxLayout. The block ends at the closing brace at line 2206.

Find this exact existing line (line 2200):

```cpp
        leftVb->addWidget(btnLoadProject);
```

…and insert immediately after it (still inside the same `{ ... }` scope, before the four `connect(...)` calls at lines 2202–2205):

```cpp

        m_btnExportPdf = mkBtn("Export PDF  ·  .pdf");
        m_btnExportPdf->setToolTip("Export the current project as a PDF report");
        leftVb->addWidget(m_btnExportPdf);
```

Then add a fifth `connect` line at the end of the existing connect block (after line 2205):

```cpp
        connect(m_btnExportPdf,  &QPushButton::clicked, this, &EnclosureWidget::onExportPdf);
```

Use `mkBtn` (not bare `new QPushButton`) so the new button picks up the themed stylesheet defined by the lambda just above.

- [ ] **Step 3: Implement the slot as a thin wrapper around the stub**

In `src/enclosurewidget.cpp`, add this implementation immediately after the existing `onLoadProject` function (line ~5289):

```cpp
void EnclosureWidget::onExportPdf()
{
    if (m_models.isEmpty()) {
        QMessageBox::information(this, "No models",
            "Add at least one model before exporting a PDF report.");
        return;
    }

    // Default: all models selected. Selection dialog arrives in Task 4.
    PdfReportOptions opts;
    opts.projectName    = deriveProjectFileBaseName(m_models, m_db);
    opts.selectedIndices.reserve(m_models.size());
    for (int i = 0; i < m_models.size(); ++i)
        opts.selectedIndices.append(i);
    opts.appliedPower   = m_splPower ? m_splPower->value() : 1.0;
    opts.perDriverMode  = m_perDriverPower && m_perDriverPower->isChecked();

    PdfReport::exportToFile(this, m_models, opts, m_db);
}
```

- [ ] **Step 4: Add the include**

Near the other `#include` lines at the top of `src/enclosurewidget.cpp`, add:

```cpp
#include "pdfreport.h"
```

- [ ] **Step 5: Build**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 6: Manual smoke check**

Launch `./TSBoss`, go to the Enclosure tab. Verify:
- An **"Export PDF  ·  .pdf"** button is visible below **Load Project**.
- Clicking it with zero models shows the "No models" info dialog.
- Adding a model and clicking it again shows the stub "PDF export is not yet implemented." dialog from Task 2.

- [ ] **Step 7: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/enclosurewidget.h src/enclosurewidget.cpp && git commit -m "$(cat <<'EOF'
feat: add Export PDF button to EnclosureWidget

Button wired into the left panel next to Save/Load Project. Invokes
the (still stubbed) PdfReport::exportToFile with all models selected
by default.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Build the scope-selection dialog

**Files:**
- Modify: `src/pdfreport.cpp`

Add a private helper that runs a small modal dialog letting the user pick "All models" or a custom subset, then return the chosen indices.

- [ ] **Step 1: Add includes and helper declaration**

At the top of `src/pdfreport.cpp`, add these includes after the existing ones:

```cpp
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QRadioButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLabel>
```

- [ ] **Step 2: Add the dialog helper as an anonymous-namespace function**

Above `PdfReport::exportToFile`, add:

```cpp
namespace {

/// Show a modal dialog that lets the user pick which models to include.
/// `defaultIndices` pre-selects rows when "Selected models" is chosen.
/// Returns the chosen indices on accept, or an empty list on cancel.
QList<int> runScopeDialog(QWidget* parent,
                          const QList<BoxModel>& models,
                          const QList<int>& defaultIndices)
{
    QDialog dlg(parent);
    dlg.setWindowTitle("Export PDF Report");

    auto* root = new QVBoxLayout(&dlg);
    root->addWidget(new QLabel("Which models should be included in the report?"));

    auto* rbAll = new QRadioButton("All models");
    auto* rbSel = new QRadioButton("Selected models");
    rbAll->setChecked(true);
    root->addWidget(rbAll);
    root->addWidget(rbSel);

    auto* list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (int i = 0; i < models.size(); ++i) {
        auto* it = new QListWidgetItem(models[i].name.isEmpty()
                                       ? QString("Model %1").arg(i + 1)
                                       : models[i].name);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(defaultIndices.contains(i)
                          ? Qt::Checked : Qt::Unchecked);
        list->addItem(it);
    }
    list->setEnabled(false);
    root->addWidget(list);

    QObject::connect(rbSel, &QRadioButton::toggled,
                     list,  &QListWidget::setEnabled);

    auto* btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    root->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted)
        return {};

    QList<int> out;
    if (rbAll->isChecked()) {
        for (int i = 0; i < models.size(); ++i) out.append(i);
    } else {
        for (int i = 0; i < models.size(); ++i)
            if (list->item(i)->checkState() == Qt::Checked) out.append(i);
    }
    return out;
}

} // namespace
```

- [ ] **Step 3: Call the dialog from `exportToFile`**

Replace the body of `PdfReport::exportToFile` with:

```cpp
bool PdfReport::exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db)
{
    Q_UNUSED(db);

    if (models.isEmpty()) {
        QMessageBox::information(parent, "PDF Export",
            "Add at least one model before exporting.");
        return false;
    }

    const QList<int> chosen = runScopeDialog(parent, models, opts.selectedIndices);
    if (chosen.isEmpty()) return false;  // cancel OR zero checked

    QMessageBox::information(parent, "PDF Export",
        QString("Stub: would export %1 model(s).").arg(chosen.size()));
    return true;
}
```

- [ ] **Step 4: Build**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 5: Manual smoke check**

Launch `./TSBoss`, add 3 models. Click **Export PDF…**. Verify:
- Dialog appears with two radio buttons; "All models" is selected; list of 3 model names is disabled.
- Clicking "Selected models" enables the list. All three start checked (matches default).
- Unchecking one and clicking OK shows "Stub: would export 2 model(s)."
- Clicking Cancel returns silently (no further dialog).
- Clicking OK with "Selected models" and zero checks returns silently too.

- [ ] **Step 6: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/pdfreport.cpp && git commit -m "$(cat <<'EOF'
feat: scope-selection dialog for PDF export

User can choose All models or hand-pick a subset before the file
dialog opens. Defaults to all-checked. Still a stub past this point.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Render one plot widget to a QPixmap (proof-of-concept)

**Files:**
- Modify: `src/pdfreport.cpp`

Before assembling the full HTML, get a single plot rendered to a pixmap and saved to a temp file we can eyeball. This isolates the off-screen rendering question early.

- [ ] **Step 1: Add includes**

In `src/pdfreport.cpp`, add to the existing include block:

```cpp
#include <QPixmap>
#include <QPainter>
#include <QStandardPaths>
#include <QDir>
```

- [ ] **Step 2: Add a private helper to render the SPL plot**

Inside the anonymous namespace, above `runScopeDialog`, add:

```cpp
/// Build a filtered model list containing only the chosen indices.
QList<BoxModel> filterModels(const QList<BoxModel>& models,
                             const QList<int>& chosen)
{
    QList<BoxModel> out;
    out.reserve(chosen.size());
    for (int idx : chosen)
        if (idx >= 0 && idx < models.size())
            out.append(models[idx]);
    return out;
}

/// Render the SPL plot for the given filtered models into a QPixmap.
QPixmap renderSplPlot(const QList<BoxModel>& filtered,
                      double appliedPower,
                      bool perDriverMode,
                      QSize sizePx)
{
    ResponsePlot plot;
    plot.setAttribute(Qt::WA_DontShowOnScreen);
    plot.resize(sizePx);
    plot.setPower(appliedPower);
    plot.setPerDriverMode(perDriverMode);
    plot.setModels(filtered, -1);  // no active highlight in the report
    plot.ensurePolished();

    QPixmap px(sizePx);
    px.fill(Qt::white);
    plot.render(&px);
    return px;
}
```

- [ ] **Step 3: Include the enclosure header so ResponsePlot is known**

The existing `#include "enclosurewidget.h"` already pulls in `ResponsePlot`. No new include needed.

- [ ] **Step 4: Use the helper in `exportToFile` as a temporary smoke check**

In `PdfReport::exportToFile`, replace the final stub `QMessageBox::information(parent, "PDF Export", QString("Stub: would export %1 model(s).").arg(chosen.size())); return true;` with:

```cpp
    const auto filtered = filterModels(models, chosen);
    const QPixmap px = renderSplPlot(filtered, opts.appliedPower,
                                     opts.perDriverMode, QSize(1600, 900));

    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                        + "/tsboss_spl_test.png";
    if (!px.save(tmp, "PNG")) {
        QMessageBox::warning(parent, "PDF Export",
            "Could not write test pixmap to " + tmp);
        return false;
    }
    QMessageBox::information(parent, "PDF Export",
        "SPL plot rendered to " + tmp);
    return true;
```

- [ ] **Step 5: Build**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 6: Manual verification**

Launch `./TSBoss`, add 2 sealed models. Click **Export PDF…**, accept defaults, OK. Open the temp PNG file (path is shown in the success dialog). Verify:
- The plot shows both model curves with their assigned colours.
- Axes and labels are legible.
- Background is white (not the dark theme background).
- Image is 1600×900 pixels.

If the plot background is black, that's a sign the dark theme leaked through; if the curves look identical to the on-screen plot, the off-screen render works.

- [ ] **Step 7: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/pdfreport.cpp && git commit -m "$(cat <<'EOF'
feat: off-screen render of SPL plot to QPixmap

Validates that ResponsePlot renders correctly off-screen. The export
flow now writes a temp PNG so the rasterisation can be eyeballed; this
will be replaced by a QTextDocument resource in the next task.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Render all five plots and build the combined-plots section

**Files:**
- Modify: `src/pdfreport.cpp`

Extend the rasteriser to handle all five plot types, then write a minimal QTextDocument with just the combined-plots pages so we can print our first PDF.

- [ ] **Step 1: Add includes**

In `src/pdfreport.cpp`, add to the include block:

```cpp
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QTextDocument>
#include <QUrl>
#include <QFileInfo>
#include <QDateTime>
```

- [ ] **Step 2: Generalise the renderer for every plot type**

Replace `renderSplPlot` (added in Task 5) with five plot-specific helpers — power-aware plots take applied power, GroupDelayPlot does not. Inside the anonymous namespace, add:

```cpp
QPixmap renderSpl(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    ResponsePlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderGd(const QList<BoxModel>& m, QSize sz) {
    GroupDelayPlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderVolt(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    VoltagePlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderExc(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    ExcursionPlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
QPixmap renderPv(const QList<BoxModel>& m, double power, bool perDriver, QSize sz) {
    PortVelocityPlot p; p.setAttribute(Qt::WA_DontShowOnScreen);
    p.resize(sz); p.setPower(power); p.setPerDriverMode(perDriver);
    p.setModels(m, -1); p.ensurePolished();
    QPixmap px(sz); px.fill(Qt::white); p.render(&px); return px;
}
```

- [ ] **Step 3: Add an HTML builder for the combined-plots section**

Inside the anonymous namespace, add:

```cpp
QString buildHtmlSkeleton(const PdfReportOptions& opts,
                          const QList<BoxModel>& filtered)
{
    QStringList modelNames;
    for (const auto& m : filtered)
        modelNames << (m.name.isEmpty() ? QStringLiteral("(unnamed)") : m.name);

    const QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    const QString title = opts.projectName.isEmpty()
                          ? QStringLiteral("TSBoss Report") : opts.projectName;

    QString html;
    html += QStringLiteral(
        "<html><head><style>"
        "body { font-family: sans-serif; color: #111; }"
        "h1 { font-size: 22pt; margin-bottom: 4pt; }"
        "h2 { font-size: 16pt; margin-top: 18pt; border-bottom: 1px solid #888; padding-bottom: 2pt; }"
        ".meta { color: #555; font-size: 10pt; margin-bottom: 14pt; }"
        ".plot { margin-bottom: 14pt; }"
        ".pagebreak { page-break-before: always; }"
        "</style></head><body>");

    // Cover
    html += QStringLiteral("<h1>%1</h1>").arg(title.toHtmlEscaped());
    html += QStringLiteral("<div class='meta'>Exported %1 &middot; %2 model(s): %3</div>")
            .arg(today, QString::number(filtered.size()),
                 modelNames.join(", ").toHtmlEscaped());

    // Combined plots — each on its own page after the cover
    static const char* keys[]  = { "spl", "gd", "volt", "exc", "pv" };
    static const char* titles[] = {
        "SPL Response", "Group Delay", "Voltage Demand",
        "Cone Excursion", "Port Velocity"
    };
    for (int i = 0; i < 5; ++i) {
        html += QStringLiteral("<div class='pagebreak'></div>");
        html += QStringLiteral("<h2>%1</h2>").arg(titles[i]);
        html += QStringLiteral("<img class='plot' src='plot://%1' width='720'>").arg(keys[i]);
    }

    html += QStringLiteral("</body></html>");
    return html;
}
```

- [ ] **Step 4: Replace the temp-PNG smoke check with the real PDF write**

Replace the body of `PdfReport::exportToFile` from the `const auto filtered = filterModels(...)` line through the final `return true;` with:

```cpp
    const auto filtered = filterModels(models, chosen);

    // Ask user where to save
    const QString defaultName = (opts.projectName.isEmpty()
                                 ? QStringLiteral("enclosure")
                                 : opts.projectName) + ".pdf";
    const QString path = QFileDialog::getSaveFileName(
        parent, "Export PDF", QDir::homePath() + "/" + defaultName,
        "PDF Document (*.pdf)");
    if (path.isEmpty()) return false;

    const QSize plotPx(1600, 900);
    const QPixmap pxSpl  = renderSpl (filtered, opts.appliedPower, opts.perDriverMode, plotPx);
    const QPixmap pxGd   = renderGd  (filtered, plotPx);
    const QPixmap pxVolt = renderVolt(filtered, opts.appliedPower, opts.perDriverMode, plotPx);
    const QPixmap pxExc  = renderExc (filtered, opts.appliedPower, opts.perDriverMode, plotPx);
    const QPixmap pxPv   = renderPv  (filtered, opts.appliedPower, opts.perDriverMode, plotPx);

    QTextDocument doc;
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://spl"),  pxSpl);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://gd"),   pxGd);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://volt"), pxVolt);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://exc"),  pxExc);
    doc.addResource(QTextDocument::ImageResource, QUrl("plot://pv"),   pxPv);
    doc.setHtml(buildHtmlSkeleton(opts, filtered));

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    writer.setResolution(150);
    doc.setPageSize(QSizeF(writer.width(), writer.height()));
    doc.print(&writer);

    QMessageBox::information(parent, "PDF Export",
        "Report exported to " + QFileInfo(path).fileName());
    return true;
```

- [ ] **Step 5: Build**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 6: Manual verification**

Launch `./TSBoss`, add one sealed and one vented model. Click **Export PDF…**, accept the scope dialog, save the PDF somewhere. Open the result. Verify:
- Cover page shows the title, date, and a comma-separated list of both model names.
- Five subsequent pages, each titled and showing one plot with both models overlaid.
- Plots render with white background and visible curves.
- No empty pages between plots (or at most one — QTextDocument page-break behaviour can produce trailing whitespace).

- [ ] **Step 7: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/pdfreport.cpp && git commit -m "$(cat <<'EOF'
feat: PDF export emits cover + five combined plots

QTextDocument with image resources for each plot rasterisation,
printed to QPdfWriter on A4 with 20mm margins. Per-model breakdowns
come in a follow-up commit.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Add per-model breakdown sections (Tables A–D)

**Files:**
- Modify: `src/pdfreport.cpp`

Append per-model sections to the HTML: one page-break per model, then four tables. Table C is omitted for sealed models. Table D values come from the BoxModel computed results, not new computation.

- [ ] **Step 1: Add small formatting helpers**

Inside the anonymous namespace, add:

```cpp
QString fmt(double v, int decimals = 2) {
    return QString::number(v, 'f', decimals);
}
QString encTypeLabel(BoxModel::EncType t) {
    switch (t) {
    case BoxModel::EncType::Sealed:    return "Sealed";
    case BoxModel::EncType::Vented:    return "Vented";
    case BoxModel::EncType::IB:        return "Infinite Baffle";
    case BoxModel::EncType::Bandpass4: return "Bandpass (4th order)";
    case BoxModel::EncType::Bandpass6: return "Bandpass (6th order)";
    }
    return "Unknown";
}
bool hasPort(BoxModel::EncType t) {
    return t == BoxModel::EncType::Vented
        || t == BoxModel::EncType::Bandpass4
        || t == BoxModel::EncType::Bandpass6;
}
QString portShapeLabel(int s) { return s == 0 ? "Round" : "Rectangular"; }
```

- [ ] **Step 2: Add the per-model HTML builder**

Inside the anonymous namespace, add:

```cpp
QString buildModelSection(const BoxModel& m,
                          DriverDatabase* db,
                          double appliedPower)
{
    // Resolve driver display name from DB, with safe fallback.
    QString driverDesc = "(no driver linked)";
    if (m.driverId >= 0 && db && db->isOpen()) {
        const auto r = db->loadDriver(m.driverId);
        if (!r.make.isEmpty() || !r.model.isEmpty())
            driverDesc = QString("%1 %2").arg(r.make, r.model).trimmed();
        else
            driverDesc = "(driver not found in database)";
    }

    QString h;
    h += "<div class='pagebreak'></div>";
    h += QStringLiteral("<h2>%1</h2>").arg(m.name.toHtmlEscaped());
    h += QStringLiteral("<div class='meta'>%1 &middot; %2</div>")
         .arg(encTypeLabel(m.encType).toHtmlEscaped(), driverDesc.toHtmlEscaped());

    // Table A — Driver T/S
    h += "<h3>Driver T/S</h3><table border='1' cellpadding='4' cellspacing='0'>";
    h += QString("<tr><td>fs</td><td>%1 Hz</td></tr>").arg(fmt(m.fs, 1));
    h += QString("<tr><td>Vas</td><td>%1 L</td></tr>").arg(fmt(m.Vas_L, 2));
    h += QString("<tr><td>Qts</td><td>%1</td></tr>").arg(fmt(m.Qts, 3));
    h += QString("<tr><td>Qes</td><td>%1</td></tr>").arg(fmt(m.Qes, 3));
    h += QString("<tr><td>Qms</td><td>%1</td></tr>").arg(fmt(m.Qms, 3));
    h += QString("<tr><td>Re</td><td>%1 &Omega;</td></tr>").arg(fmt(m.Re, 2));
    h += QString("<tr><td>mms</td><td>%1 g</td></tr>").arg(fmt(m.mms_g, 2));
    h += QString("<tr><td>BL</td><td>%1 Tm</td></tr>").arg(fmt(m.BL, 2));
    h += QString("<tr><td>Sd</td><td>%1 cm&sup2;</td></tr>").arg(fmt(m.Sd_cm2, 1));
    h += QString("<tr><td>Drivers</td><td>%1</td></tr>").arg(m.numDrivers);
    h += "</table>";

    // Table B — Enclosure
    h += "<h3>Enclosure</h3><table border='1' cellpadding='4' cellspacing='0'>";
    h += QString("<tr><td>Type</td><td>%1</td></tr>").arg(encTypeLabel(m.encType).toHtmlEscaped());
    h += QString("<tr><td>Volume</td><td>%1 L</td></tr>").arg(fmt(m.volumeL, 2));
    if (hasPort(m.encType)) {
        h += QString("<tr><td>fb</td><td>%1 Hz</td></tr>").arg(fmt(m.fb, 1));
        h += QString("<tr><td>QL</td><td>%1</td></tr>").arg(fmt(m.QL, 2));
    }
    if (m.encType == BoxModel::EncType::Bandpass4 ||
        m.encType == BoxModel::EncType::Bandpass6) {
        h += QString("<tr><td>Front volume</td><td>%1 L</td></tr>").arg(fmt(m.volumeFront_L, 2));
        h += QString("<tr><td>Front fb</td><td>%1 Hz</td></tr>").arg(fmt(m.fbFront, 1));
    }
    if (m.encType == BoxModel::EncType::Sealed) {
        h += QString("<tr><td>alpha</td><td>%1</td></tr>").arg(fmt(m.alpha, 3));
        h += QString("<tr><td>Fc</td><td>%1 Hz</td></tr>").arg(fmt(m.Fc, 1));
        h += QString("<tr><td>Qtc</td><td>%1</td></tr>").arg(fmt(m.Qtc, 3));
    }
    h += QString("<tr><td>f3</td><td>%1 Hz</td></tr>").arg(fmt(m.f3, 1));
    h += QString("<tr><td>&eta;&#8320;</td><td>%1 %</td></tr>").arg(fmt(m.eta, 3));
    h += QString("<tr><td>Reference SPL (1W/1m)</td><td>%1 dB</td></tr>").arg(fmt(m.spl, 1));
    h += "</table>";

    // Table C — Port geometry (vented and bandpass only)
    if (hasPort(m.encType)) {
        h += "<h3>Port Geometry (rear)</h3><table border='1' cellpadding='4' cellspacing='0'>";
        h += QString("<tr><td>Shape</td><td>%1</td></tr>").arg(portShapeLabel(m.portShape));
        if (m.portShape == 0) {
            h += QString("<tr><td>Diameter</td><td>%1 mm</td></tr>").arg(fmt(m.portWidth_mm, 1));
        } else {
            h += QString("<tr><td>Width</td><td>%1 mm</td></tr>").arg(fmt(m.portWidth_mm, 1));
            h += QString("<tr><td>Height</td><td>%1 mm</td></tr>").arg(fmt(m.portHeight_mm, 1));
            h += QString("<tr><td>Shared walls</td><td>%1</td></tr>").arg(m.portWalls);
        }
        h += QString("<tr><td>Number of ports</td><td>%1</td></tr>").arg(m.numPorts);
        if (m.portF2H > 0)
            h += QString("<tr><td>2nd pipe harmonic</td><td>%1 Hz</td></tr>").arg(fmt(m.portF2H, 1));
        h += "</table>";
    }

    // Table D — Applied power & predicted output
    h += "<h3>Applied Power &amp; Predicted Output</h3>";
    h += "<table border='1' cellpadding='4' cellspacing='0'>";
    h += QString("<tr><td>Applied power</td><td>%1 W</td></tr>").arg(fmt(appliedPower, 2));
    h += QString("<tr><td>Predicted passband SPL</td><td>%1 dB</td></tr>")
         .arg(fmt(m.spl + 10.0 * std::log10(std::max(appliedPower, 1e-6)), 1));
    if (m.xmax_mm > 0)
        h += QString("<tr><td>Driver Xmax</td><td>%1 mm</td></tr>").arg(fmt(m.xmax_mm, 2));
    if (m.xlim_mm > 0)
        h += QString("<tr><td>Driver Xlim</td><td>%1 mm</td></tr>").arg(fmt(m.xlim_mm, 2));
    h += "</table>";

    return h;
}
```

- [ ] **Step 3: Add the `<cmath>` include**

At the top of `src/pdfreport.cpp`, near the other includes, add:

```cpp
#include <cmath>
```

- [ ] **Step 4: Extend `buildHtmlSkeleton` to take the DB and append per-model sections**

Change the signature of `buildHtmlSkeleton` to:

```cpp
QString buildHtml(const PdfReportOptions& opts,
                  const QList<BoxModel>& filtered,
                  DriverDatabase* db)
```

(Rename `buildHtmlSkeleton` → `buildHtml` everywhere it appears.) Just before the closing `</body></html>`, insert the per-model loop:

```cpp
    for (const auto& m : filtered)
        html += buildModelSection(m, db, opts.appliedPower);
```

- [ ] **Step 5: Update the caller**

In `PdfReport::exportToFile`, change the line `doc.setHtml(buildHtmlSkeleton(opts, filtered));` to:

```cpp
    doc.setHtml(buildHtml(opts, filtered, db));
```

And remove the `Q_UNUSED(db);` line near the top of the function — we use it now.

- [ ] **Step 6: Build**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 7: Manual verification**

Launch `./TSBoss`, build a project with:
- Model 1: sealed, known driver linked.
- Model 2: vented, known driver linked.
- Model 3: vented, with the driver row deleted from the DB after adding (to test the "driver not found" fallback) — *skip this step if it requires too much DB poking; just verify the happy path*.

Export to PDF. Open and verify:
- After the 5 combined-plot pages, model 1's section appears on its own page with three tables (A, B, D — no C).
- Model 2's section follows on a new page with all four tables; Port Geometry shows correct shape, diameter or W×H, and number-of-ports.
- Sealed model's Enclosure table includes alpha, Fc, Qtc; vented model's omits those and adds fb, QL.
- Driver names render correctly. If a driver lookup fails, "(driver not found in database)" appears.
- Applied-power row in Table D shows the value from the SPL tab.

- [ ] **Step 8: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/pdfreport.cpp && git commit -m "$(cat <<'EOF'
feat: per-model breakdown sections in PDF report

Each selected model gets its own page with Driver T/S, Enclosure,
Port (if applicable), and Applied Power tables. Driver name is
resolved from the DB with a graceful fallback when the row is gone.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 8: Polish — plot legends and small cleanups

**Files:**
- Modify: `src/pdfreport.cpp`

The plot widgets don't paint a legend (it lives outside the widget in the on-screen UI). Add a small HTML legend above each plot image so the model–colour mapping is preserved in print.

- [ ] **Step 1: Add a legend builder**

Inside the anonymous namespace, add:

```cpp
QString buildLegendHtml(const QList<BoxModel>& filtered) {
    QString h = "<div style='font-size: 9pt; margin-bottom: 6pt;'>";
    for (int i = 0; i < filtered.size(); ++i) {
        const auto& m = filtered[i];
        if (i > 0) h += "&nbsp;&nbsp;";
        h += QStringLiteral(
            "<span style='display:inline-block; width:10pt; height:10pt; "
            "background:%1; vertical-align:middle;'></span> %2")
            .arg(m.color.name(), m.name.toHtmlEscaped());
    }
    h += "</div>";
    return h;
}
```

- [ ] **Step 2: Insert the legend before each combined-plot image**

In `buildHtml` (formerly `buildHtmlSkeleton`), in the loop that emits the five plots, change:

```cpp
        html += QStringLiteral("<div class='pagebreak'></div>");
        html += QStringLiteral("<h2>%1</h2>").arg(titles[i]);
        html += QStringLiteral("<img class='plot' src='plot://%1' width='720'>").arg(keys[i]);
```

to:

```cpp
        html += QStringLiteral("<div class='pagebreak'></div>");
        html += QStringLiteral("<h2>%1</h2>").arg(titles[i]);
        html += buildLegendHtml(filtered);
        html += QStringLiteral("<img class='plot' src='plot://%1' width='720'>").arg(keys[i]);
```

- [ ] **Step 3: Build**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -20
```

Expected: clean build.

- [ ] **Step 4: Manual verification**

Re-export the test project from Task 7. Verify each combined-plot page now shows colour swatches and model names above the plot image, matching the on-screen colours.

- [ ] **Step 5: Commit**

```bash
cd /home/wessel/projects/TSBoss && git add src/pdfreport.cpp && git commit -m "$(cat <<'EOF'
feat: legend strip above each plot in PDF report

Each combined-plot page now shows a colour-swatch / model-name legend
matching the on-screen plot colours.

Co-Authored-By: Claude Opus 4.7 <noreply@anthropic.com>
EOF
)"
```

---

## Task 9: Final smoke test across all enclosure types

No code changes — purely a verification pass before declaring the feature done.

- [ ] **Step 1: Build a four-model test project**

Launch `./TSBoss`, create:
1. Model 1: Sealed, 1 driver.
2. Model 2: Vented, 1 driver, round port.
3. Model 3: Vented, 2 drivers (parallel), rectangular port with 1 shared wall.
4. Model 4: Bandpass4 (if the existing UI supports it without crashing).

- [ ] **Step 2: Export with "All models"**

Click **Export PDF…**, leave "All models" selected, OK, save the PDF. Open it. Verify:
- Cover page lists all 4 models.
- 5 plot pages, each with a 4-entry legend and all 4 curves visible (where applicable — bandpass curves may differ).
- 4 per-model pages in order, each with correct tables.
- File opens cleanly in Evince and a browser.

- [ ] **Step 3: Export with a 2-model subset**

Click **Export PDF…**, switch to "Selected models", uncheck models 1 and 3, OK, save. Open. Verify:
- Cover lists 2 models.
- Combined plots show only those 2 curves.
- Only 2 per-model pages.

- [ ] **Step 4: Edge case — zero checks**

Click **Export PDF…**, switch to "Selected models", uncheck all, OK. Verify the export silently returns without opening the file dialog (or with a sensible warning — current behaviour returns false silently, which is acceptable).

- [ ] **Step 5: Edge case — empty project**

Remove all models, click **Export PDF…**. Verify the "Add at least one model" info dialog appears and nothing further happens.

- [ ] **Step 6: No commit needed — verification only**

If any test fails, file a follow-up task; do not amend prior commits.

---

## Self-Review Notes

**Spec coverage:**
- §3 User flow → Tasks 3, 4, 6 (button, dialog, file save).
- §4 Output structure → Tasks 6 (cover + plots), 7 (breakdowns), 8 (legends).
- §5 Architecture → Tasks 1 (helper extract), 2 (new files), 3 (slot wiring).
- §6 Implementation approach → Tasks 5 (off-screen render), 6 (QPdfWriter pipeline), 7 (HTML tables).
- §7 Testing plan → Task 9 covers each scenario.
- §8 Risks — pixmap sizing is exercised in Task 5's manual check; font availability is covered by sticking to default Qt fonts (no custom font is referenced anywhere in the HTML); applied-power propagation resolved in Task 3 by reading from `m_splPower` directly.

**Note on spec deviation:** The spec was updated mid-plan to reflect that TSBoss has 5 plots (SPL, Group Delay, Voltage, Excursion, Port Velocity), not 3. This matches the user's original "all available plots" requirement. CLAUDE.md still mentions only 3 plot classes — that's a stale doc, separate from this work.

**Note on testing:** TSBoss has no automated test harness. Tasks 5, 6, 7, 8, 9 use manual smoke verification. Adding unit tests purely for PDF visual output is not worth the surface area; a regression in this feature is immediately visible.

**No outstanding placeholders.** Every step has the exact code or command needed.
