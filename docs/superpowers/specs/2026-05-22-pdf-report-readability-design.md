# PDF Report Readability Overhaul — Design

**Date:** 2026-05-22
**Scope:** `src/pdfreport.cpp` only (functions `buildHtml`, `buildModelSection`/replacement, `buildLegendHtml`, `exportToFile`, render helpers).
**Goal:** Make the exported PDF digestible — stop wasting pages on tiny plots, and replace the one-page-per-model vertical tables with a compact side-by-side comparative table.

## Constraint

The PDF is rendered by Qt's `QTextDocument`, which supports only a limited HTML4 / CSS subset:
- Layout via `<table>` only — **no flexbox, no grid**.
- Supported: `page-break-before`, `page-break-inside: avoid`, basic font/padding/border/background CSS, inline styles.
- Plot images embedded as `plot://` `QTextDocument::ImageResource` resources (unchanged mechanism).

Page setup stays as today: A4 **portrait**, 20 mm margins, 150 dpi (`exportToFile`).

## Problems being fixed

1. Each plot forces a page break, renders small, and leaves ~60% of the page blank.
2. The inline `buildLegendHtml` legend strip wraps across the full width and spills *below* the plot.
3. Redundant legends — the plot pixmaps already draw their own internal legend.
4. 7 models = 7 pages of repeated single-column key/value tables; no easy cross-model comparison.

## Section 1 — Plots

- **Drop** the per-plot inline `buildLegendHtml` strip.
- Render **one shared legend** as a small table at the top of the plots section. Each row: `[color swatch] M<n> — <full model name>`. One model per row; never wraps mid-name. This legend is the single mapping of number → color → name, referenced by both the plots and the comparative table.
- Suppress / ignore the plot widgets' own internal legend if practical; otherwise leave it (the shared legend is authoritative). (Implementation note: the standalone shared legend is the requirement; in-plot legend cleanup is best-effort.)
- **Two plots per page**, each at full content width. Increase render pixmap size (target ~2000×1150) and scale to content width so plots stay crisp. 5 plots → ~3 pages.
- Wrap each plot block (`<h2>` title + image) so it does not split across a page edge — `page-break-inside: avoid`. First plot section starts on a fresh page; subsequent plots flow two-per-page.

## Section 2 — Comparative data table

Replaces all per-model detail pages.

- **Orientation:** parameters as **rows**, models as **columns**. (No transpose.)
- **Three grouped tables**, each its own `<h3>` + table:
  - **Driver T/S:** fs (Hz), Vas (L), Qts, Qes, Qms, Re (Ω), mms (g), BL (Tm), Sd (cm²), # drivers
  - **Enclosure & Port:** Type, Volume (L), fb (Hz), QL, Front volume (L), Front fb (Hz) [bandpass]; alpha, Fc (Hz), Qtc [sealed]; Port shape, Diameter / Width+Height (mm), Shared walls, # ports, 2nd pipe harmonic (Hz)
  - **Results & Output:** f3 (Hz), η₀ (%), Reference SPL (1W/1m, dB), Applied power (W), Predicted passband SPL (dB), Driver Xmax (mm), Driver Xlim (mm)
- **Column headers:** short labels **M1 … M10** (tied to the shared legend). Full names are NOT repeated per column.
- **Heterogeneous models:** every row that applies to *any* included model is present; cells for models that lack that parameter (e.g. `alpha` on a vented box, front fb on a non-bandpass) show **"—"** rather than the row being omitted.
- **Fit:** reduce table font to ~7–8 pt and tighten cell padding so up to 10 model columns fit portrait A4 content width (~1000 px at 150 dpi: ~140 px label column + ~86 px per model column).

## Section 3 — 10-model cap

- The report supports **at most 10 models**.
- Enforced in `exportToFile` after scope selection: if more than 10 models are chosen, show a `QMessageBox` asking the user to select ≤ 10, and abort the export (return false). **No silent truncation** — dropping models from a comparison report would be misleading.

## Out of scope

- Cover page content/layout beyond what exists.
- Acoustic/model math — values come straight from `BoxModel` as today.
- Landscape orientation (rejected in favor of font reduction + 10-model cap).
- Per-model detail pages (removed).

## Verification

- Build (`cmake --build build --parallel`) succeeds.
- Re-export the 7-model `7x_Hertz` project: plots are large and two-per-page, a single shared M#/color/name legend appears, and one set of three grouped comparative tables replaces the seven per-model pages.
- Export an 11-model project: blocked with the ≤10 message.
- Export a mixed sealed/vented/bandpass set: type-specific rows show "—" where inapplicable, no rows missing.
