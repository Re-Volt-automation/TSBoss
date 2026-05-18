# PDF Report Export — Design

**Date:** 2026-05-18
**Status:** Approved
**Scope:** Add a PDF export feature to TSBoss that produces a printable report of the currently open enclosure models, including all three plots (combined, overlaid) followed by per-model parameter breakdowns.

---

## 1. Goal

Allow the user to export a portable PDF containing:

1. The five combined plots (SPL Response, Group Delay, Voltage, Excursion, Port Velocity) overlaying every selected model — same visual content as the on-screen tabs.
2. A per-model breakdown section listing the driver T/S parameters, enclosure configuration, port geometry (if vented), and applied power / predicted output for each selected model.

The PDF should be suitable for review, archiving, and sharing alongside a `.tsbox` project file.

## 2. Non-goals

- No system print dialog. PDF file output only.
- No raw measurement traces beyond the existing model plots.
- No interactive PDF features (no bookmarks beyond what QTextDocument generates automatically).
- No batch export across multiple project files.

## 3. User flow

1. User has one or more models loaded in the EnclosureWidget.
2. User clicks an **"Export PDF…"** button in the EnclosureWidget left panel, beside the existing Save/Load Project buttons.
3. A dialog appears:
   - Radio buttons: **"All models"** (default) / **"Selected models"**
   - When "Selected models" is chosen, a checkbox list of model names becomes enabled, pre-checked to the currently selected indices in the list.
   - OK / Cancel.
4. On OK, a `QFileDialog::getSaveFileName` opens with `.pdf` filter; default filename is derived from the project name (reusing the same auto-naming heuristic used by Save Project, with `.pdf` extension).
5. The PDF is generated and written to disk. A status message ("Report exported to …") is shown via `QMessageBox::information` on success, `QMessageBox::warning` on failure.

## 4. Output structure

### Page 1 — Title / cover
- Project title (file basename, or "Untitled Project")
- Export date (ISO date)
- Model count
- One-line list of model names

### Pages 2–6 — Combined plots
- One plot per page (or as page breaks fall — QTextDocument decides), each overlaying the *selected* models:
  1. **SPL Response** — same content as the SPL tab, including the applied-power offset.
  2. **Group Delay** — same content as the Group Delay tab.
  3. **Voltage** — same content as the Voltage tab.
  4. **Excursion** — same content as the Excursion tab.
  5. **Port Velocity** — same content as the Port Velocity tab.
- Each plot has a heading and a small legend showing model name + colour swatch (matches `kPalette` cycling).

### Pages 7+ — Per-model breakdowns
For each selected model, in order:

- **Model header** — model name, enclosure type (Sealed / Vented), driver model+brand (from DB).
- **Table A — Driver T/S parameters:** `fs, Vas, Qts, Qes, Qms, Re, mms, BL, Sd, Le` (units shown).
- **Table B — Enclosure:** `Type, Volume (L), fb (Hz) [vented only], QL [vented only], alpha, Fc (Hz), Qtc, f3 (Hz), efficiency η₀ (%), reference SPL (dB)`.
- **Table C — Port geometry** *(vented models only, omitted for sealed):* `Shape (round/rect), Width/Diameter (mm), Height (mm) [rect only], Area (cm²), Computed length (cm), Shared wall (yes/no)`.
- **Table D — Applied power & predicted output:** `Applied power (W), Predicted SPL at fb (dB), Predicted SPL passband (dB), Predicted cone excursion at fb (mm)`. Excursion comes from the same calculation the Excursion plot uses.

Each per-model section starts on a new page (`page-break-before` style in the HTML).

## 5. Architecture

### New files

| File | Role |
|------|------|
| `src/pdfreport.h` | Declares `PdfReport` class and `PdfReportOptions` struct |
| `src/pdfreport.cpp` | Implementation: build HTML, render plots to pixmaps, print to QPdfWriter |

### New class

```cpp
struct PdfReportOptions {
    QString projectName;          // for cover page and default filename
    QList<int> selectedIndices;   // which models to include (indexes into models[])
};

class PdfReport {
public:
    // Returns true on success, false on cancel or I/O failure.
    // Shows its own QMessageBox on failure; caller doesn't need to.
    static bool exportToFile(QWidget* parent,
                             const QList<BoxModel>& models,
                             const PdfReportOptions& opts,
                             DriverDatabase* db);
};
```

`DriverDatabase*` is needed so the per-model breakdown can resolve the driver make/model and any Driver T/S fields not already mirrored into `BoxModel` (BoxModel has `fs, Vas, Qts, Qes, Qms, Re, mms, BL, Sd` — `Le` would need a lookup, as would the driver name). The class name is `DriverDatabase` (defined in `driverdb.h`), used as a pointer throughout the codebase.

### New dialog

A small modal dialog (declared inline inside `pdfreport.cpp` or as a nested helper) shown by a thin wrapper invoked from EnclosureWidget:

```cpp
// In EnclosureWidget:
void onExportPdf();   // shows scope dialog → file dialog → PdfReport::exportToFile
```

### Touched files

| File | Change |
|------|--------|
| `src/enclosurewidget.h` | Add `m_btnExportPdf` member, `onExportPdf()` slot |
| `src/enclosurewidget.cpp` | Create button next to Save/Load, wire up slot, implement scope/file-pick flow |
| `src/CMakeLists.txt` | Add `pdfreport.cpp` / `pdfreport.h` to source list |

## 6. Implementation approach

### Rendering plots into the PDF

The three plot widgets (`ResponsePlot`, `GroupDelayPlot`, `VoltagePlot`) already paint correctly given a `QList<BoxModel>` plus their auxiliary state (selected indices, applied power). They're `QWidget` subclasses with `paintEvent` doing everything.

For the combined-plot pages we need to render a *filtered* view — only the user-selected models. Approach:

1. Construct a fresh instance of each plot widget off-screen (`new ResponsePlot; w->resize(targetSize); w->setAttribute(Qt::WA_DontShowOnScreen);`).
2. Hand it the filtered model list and any state it needs (applied power, selection set covering all members of the filtered list so curves are visible).
3. Call `widget->render(&pixmap)` into a `QPixmap` sized roughly `1600×900` (high DPI; QTextDocument scales to fit page width).
4. Embed the pixmap in `QTextDocument` via `doc.addResource(QTextDocument::ImageResource, QUrl("plot://spl"), QVariant(pixmap))` and reference with `<img src="plot://spl">`.
5. Delete the temporary widget.

This avoids any change to the existing plot classes — we reuse their `paintEvent` unchanged.

**Edge cases handled:**
- Empty selection → show a `QMessageBox::warning` ("Select at least one model") before opening the file dialog.
- Sealed model in the selection → Table C (port geometry) is omitted for that model only; combined plots still render.
- Driver T/S lookup miss (driver was deleted from DB after the model was loaded) → fall back to the values stored in `BoxModel`; show "(driver not found in database)" instead of the name.

### Building the HTML

Use `QTextDocument` + `setHtml`. Use a small, hand-written CSS string in a `<style>` block at the top for table styling, fonts, and `page-break-before: always` on per-model section headings.

Tables are HTML `<table>` with two columns (label, value+unit). One CSS class per section.

### Printing to PDF

```cpp
QPdfWriter writer(filePath);
writer.setPageSize(QPageSize(QPageSize::A4));
writer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
writer.setResolution(150);  // dpi for raster scaling
QTextDocument doc;
doc.setHtml(html);
doc.setPageSize(QSizeF(writer.width(), writer.height()));
doc.print(&writer);
```

(`QTextDocument::print` accepts any `QPagedPaintDevice`, including `QPdfWriter`.)

### Filename heuristic

Reuse the existing auto-naming logic from `onSaveProject` in `enclosurewidget.cpp` — same dedup-by-driver-id, brand aggregation, hyphen-escape. Swap the `.tsbox` extension for `.pdf`.

To avoid duplicating ~30 lines of code, **extract the auto-naming helper into a static method on `EnclosureWidget`** (`static QString deriveProjectFileBaseName(const QList<BoxModel>&, DriverDb&)`) and call it from both `onSaveProject` and `onExportPdf`.

## 7. Testing plan

Manual:
1. Single sealed model — export, verify only Tables A, B, D appear in breakdown; no Port section.
2. Single vented model — export, verify all four tables; port length matches what's shown in the Port tab.
3. Three models mixed sealed+vented, all selected — verify combined plots show three curves each, per-model sections in list order.
4. Three models, only two selected via dialog — verify combined plots show only the two selected, breakdown sections show only the two.
5. Empty model list — button should be disabled OR a warning dialog should appear (decide during implementation; disabled is preferred).
6. Driver deleted from DB after model load — verify "driver not found" fallback works.
7. Long model names / brand strings — verify they don't overflow the cover page or table cells.
8. Open resulting PDF in at least two viewers (Evince + a browser) to confirm rendering portability.

No automated tests are added — TSBoss currently has no test harness, and adding one solely for PDF visual output isn't worth the surface area.

## 8. Risks / open questions

- **Pixmap sizing on HiDPI screens:** `QWidget::render()` respects the widget's logical size, not device pixel ratio. Setting the offscreen widget to a large explicit size (e.g. 1600×900) and letting QTextDocument scale should produce acceptable print quality. If not, switch to `QPicture` (vector) for plots in a follow-up.
- **Font availability in PDF:** Stick to default Qt fonts; do not embed custom fonts.
- **Applied power propagation:** The applied-power value lives in the SPL tab today. The exporter needs read-only access to it. Add a getter on EnclosureWidget (`double currentAppliedPower() const`) and pass into `PdfReportOptions`, or read it directly from the SPL plot widget — pick the cleaner option during implementation.

## 9. Out of scope (explicit non-features)

- Custom paper sizes, landscape orientation, or user-configurable margins.
- Exporting individual plots (PNG/SVG) as separate files.
- Including the measurement-side raw data (impedance sweeps, etc.) — this is a *model* report, not a *driver* report.
- Localization / i18n of report text.
