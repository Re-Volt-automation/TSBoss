# Save File Auto-Naming Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the static/display-name-derived default filenames in the Save dialogs with clean, filesystem-safe names derived from driver model number + enclosure type + volume (`.tsbox`) and model count + brands (`.tsproj`).

**Architecture:** Add one `sanitizeFilename()` static helper in `enclosurewidget.cpp`. Use it in `onSaveModel()` to sanitize `model.name`, and in `onSaveProject()` to build a brand-aggregated stem from DB lookups. No struct changes, no JSON format changes.

**Tech Stack:** Qt6, C++17, `QRegularExpression`, `QFileDialog`.

---

## Files

- Modify: `src/enclosurewidget.cpp` — add `sanitizeFilename`, update `onSaveModel` and `onSaveProject`

---

### Task 1: Add `sanitizeFilename` helper and update `onSaveModel`

**Files:**
- Modify: `src/enclosurewidget.cpp` — around line 4900 (just before `onSaveModel`)

The current `onSaveModel` default filename (around line 4910):
```cpp
QDir::homePath() + "/" + model.name + ".tsbox",
```
This produces filenames like `"/home/wessel/SB17MFC-4 / 35 L / vented.tsbox"` — spaces and slashes make poor filenames.

- [ ] **Step 1: Add `sanitizeFilename` static helper just before `onSaveModel`**

Find the line `void EnclosureWidget::onSaveModel()` in `src/enclosurewidget.cpp` (around line 4900). Insert this function immediately before it:

```cpp
static QString sanitizeFilename(const QString &s)
{
    QString out = s;
    out.replace(" / ", "_");
    out.replace(' ', '_');
    out.remove(QRegularExpression(R"([^\w\-+.])"));
    out.replace(QRegularExpression("_+"), "_");
    out = out.trimmed();
    return out.isEmpty() ? "enclosure" : out;
}
```

Note: `\w` in Qt regex matches `[A-Za-z0-9_]`. This strips slashes, parentheses, and any other non-safe chars. Multiple underscores collapse to one.

- [ ] **Step 2: Update `onSaveModel` to use `sanitizeFilename`**

Find (inside `onSaveModel`):
```cpp
        QDir::homePath() + "/" + model.name + ".tsbox",
```

Replace with:
```cpp
        QDir::homePath() + "/" + sanitizeFilename(model.name) + ".tsbox",
```

- [ ] **Step 3: Add `QRegularExpression` include if not already present**

Check the top of `enclosurewidget.cpp` for `#include <QRegularExpression>`. If missing, add it alongside the other Qt includes.

- [ ] **Step 4: Build**

```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -10
```

Expected: clean build, no errors.

---

### Task 2: Update `onSaveProject` with brand aggregation

**Files:**
- Modify: `src/enclosurewidget.cpp` — `onSaveProject` function (around line 4971)

The current default project filename:
```cpp
QDir::homePath() + "/enclosure.tsproj",
```

It must become e.g. `3x_Dayton_Audio-SB_Acoustics.tsproj`.

- [ ] **Step 1: Replace the static default path in `onSaveProject`**

Find `onSaveProject` (around line 4971). The function currently starts like:

```cpp
void EnclosureWidget::onSaveProject()
{
    if (m_models.isEmpty()) {
        QMessageBox::information(this, "No models",
            "Add at least one model before saving a project.");
        return;
    }
    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project", QDir::homePath() + "/enclosure.tsproj",
        "TSBoss Project (*.tsproj)");
```

Replace the block from `const QString path = QFileDialog...` with:

```cpp
    // Build default filename: Nx_Brand1-Brand2
    QStringList brands;
    for (const auto &m : m_models) {
        if (m.driverId >= 0 && m_db && m_db->isOpen()) {
            const auto r = m_db->loadDriver(m.driverId);
            if (!r.make.isEmpty()) {
                const QString b = sanitizeFilename(r.make);
                if (!brands.contains(b))
                    brands.append(b);
            }
        }
    }
    brands.sort(Qt::CaseInsensitive);
    const QString stem = brands.isEmpty()
        ? "enclosure"
        : QString("%1x_%2").arg(m_models.size()).arg(brands.join('-'));

    const QString path = QFileDialog::getSaveFileName(
        this, "Save Project", QDir::homePath() + "/" + stem + ".tsproj",
        "TSBoss Project (*.tsproj)");
```

- [ ] **Step 2: Build**

```bash
cd /home/wessel/projects/TSBoss/build && cmake --build . --parallel 2>&1 | tail -10
```

Expected: clean build, no errors.

- [ ] **Step 3: Manual verification — `.tsbox`**

Run:
```bash
cd /home/wessel/projects/TSBoss/build && ./TSBoss
```

1. Open Enclosure Modeller, select a driver (e.g. `SB17MFC-4`), set vented 35 L.
2. Click "Save Model · .tsbox".
3. Verify the pre-filled filename in the dialog is `SB17MFC-4_35_L_vented.tsbox` (no spaces, no slashes).
4. Cancel without saving.

- [ ] **Step 4: Manual verification — `.tsproj`**

1. Add 2–3 models from different brands (e.g. SB Acoustics + Dayton Audio).
2. Click "Save Project · .tsproj".
3. Verify the pre-filled filename is e.g. `3x_Dayton_Audio-SB_Acoustics.tsproj` (N=model count, brands sorted alphabetically, spaces replaced with `_`).
4. Cancel without saving.

- [ ] **Step 5: Edge case — no driver linked**

1. Add a model without selecting a driver from the DB (manual T/S entry).
2. For `.tsbox`: verify filename is e.g. `Driver_35_L_vented.tsbox` (the auto-name uses "Driver" as fallback).
3. For `.tsproj` with all-manual models: verify filename falls back to `enclosure.tsproj`.

- [ ] **Step 6: Commit**

```bash
cd /home/wessel/projects/TSBoss
git add src/enclosurewidget.cpp docs/superpowers/specs/2026-05-07-save-file-auto-naming-design.md docs/superpowers/plans/2026-05-07-save-file-auto-naming.md
git commit -m "feat: auto-name save dialogs from driver model, enclosure type, and brands"
```
