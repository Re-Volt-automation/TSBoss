#pragma once
#include "plotbase.h"

// ─────────────────────────────────────────────────────────────────
//  The five model plots. All state and interaction live in PlotBase;
//  each subclass only paints.
// ─────────────────────────────────────────────────────────────────

/// Log-frequency / dB SPL curves with cone/port decomposition.
class ResponsePlot : public PlotBase
{
    Q_OBJECT
public:
    explicit ResponsePlot(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

/// Group delay [ms] vs frequency.
class GroupDelayPlot : public PlotBase
{
    Q_OBJECT
public:
    explicit GroupDelayPlot(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

/// Voltage (left axis) and current (right axis) demand for the applied power.
class VoltagePlot : public PlotBase
{
    Q_OBJECT
public:
    explicit VoltagePlot(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

/// Cone displacement [mm peak] vs frequency, with Xmax/Xlim reference lines.
class ExcursionPlot : public PlotBase
{
    Q_OBJECT
public:
    explicit ExcursionPlot(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

/// Peak port air velocity vs frequency, with chuffing-limit lines.
class PortVelocityPlot : public PlotBase
{
    Q_OBJECT
public:
    explicit PortVelocityPlot(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

/// Electrical input impedance |Z(f)| at the small-signal limit —
/// vented saddle, sealed single peak, bandpass humps.
class ImpedancePlot : public PlotBase
{
    Q_OBJECT
public:
    explicit ImpedancePlot(QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
};

// ─────────────────────────────────────────────────────────────────
//  PlotRangeSettings – optional Y-axis overrides for each plot.
// ─────────────────────────────────────────────────────────────────
struct PlotRangeSettings {
    std::optional<double> splMin, splMax;
    std::optional<double> gdMin,  gdMax;
    std::optional<double> voltMin, voltMax;
    std::optional<double> excMin,  excMax;
};
