#pragma once
#include "driverrecord.h"
#include "driverdb.h"
#include "driverdetailwidget.h"
#include <QWidget>
#include <QList>
#include <QColor>
#include <optional>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QPushButton;
class QRadioButton;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QLabel;
class QGroupBox;
class QListWidget;
class QTabWidget;

// ─────────────────────────────────────────────────────────────────
//  BoxModel – one driver+enclosure combination on the graph.
// ─────────────────────────────────────────────────────────────────
struct BoxModel
{
    QString name;
    bool    autoName = true;  ///< true = name is auto-generated; false = user-chosen
    int     driverId = -1;

    // Editable driver T/S params
    double  fs    = 0.0;
    double  Vas_L = 0.0;
    double  Qts   = 0.0;
    double  Qes   = 0.0;

    // Secondary driver params (locked spinboxes)
    double  Qms    = 0.0;
    double  Re     = 0.0;
    double  mms_g  = 0.0;
    double  BL     = 0.0;
    double  Sd_cm2 = 0.0;

    // Enclosure type
    enum class EncType { Sealed = 0, Vented = 1, IB = 2, Bandpass4 = 3, Bandpass6 = 4 };
    EncType encType = EncType::Sealed;

    // For bandpass: existing volumeL/fb/QL/port* fields refer to the
    // REAR chamber (sealed for BP4, vented for BP6).  Front-chamber
    // fields are below (volumeFront_L / fbFront / QLFront / portFront*).
    double  volumeL  = 40.0;
    double  fb       = 35.0;  ///< Port tuning frequency [Hz]  (rear for bandpass)
    double  QL       = 7.0;   ///< Box/port losses Q           (rear for bandpass)

    // Bandpass front chamber
    double  volumeFront_L      = 40.0;
    double  fbFront            = 60.0;
    double  QLFront            = 7.0;
    int     portFrontShape     = 0;
    double  portFrontWidth_mm  = 75.0;
    double  portFrontHeight_mm = 50.0;
    int     portFrontWalls     = 0;
    int     numPortsFront      = 1;
    double  portFrontWallThick_mm   = 3.0;
    double  portFrontInsertDepth_mm = 0.0;
    double  portFrontExtraSurfArea_cm2 = 0.0;
    int     portFrontFlare     = 0;

    // Port geometry (for length calculation only, not the acoustic simulation)
    int     portShape    = 0;     ///< 0 = round, 1 = rectangular
    double  portWidth_mm = 75.0;  ///< Round: inner diameter [mm]; Rect: width [mm]
    double  portHeight_mm= 50.0;  ///< Rectangular port height [mm]
    int     portWalls    = 0;     ///< Rect only — enclosure walls shared: 0–3
    int     numPorts     = 1;     ///< Number of identical parallel ports

    // Round-port extras
    double  portWallThick_mm   = 3.0;  ///< Round port tube wall thickness [mm]
    double  portInsertDepth_mm = 0.0;  ///< How much the port extends into the box [mm]

    // Extra surface area (e.g. bracing obstructing port airflow) [cm²]
    double  portExtraSurfArea_cm2 = 0.0;

    // Flare
    int     portFlare = 0;  ///< 0=straight, 1=one end flared, 2=both ends flared

    // Driver count
    int     numDrivers   = 1;     ///< Number of identical drivers in the same cabinet

    // Multi-driver wiring mode (affects electrical parameter scaling in acoustic sim)
    enum class WiringMode { Series = 0, Parallel = 1, Separate = 2 };
    WiringMode wiringMode = WiringMode::Series;

    // Voice coil type (informational only — from driver record)
    bool    isDVC      = false;
    int     dvcWiring  = 0;  ///< 0=series, 1=parallel

    // Large-signal reference (from driver record, not user-editable)
    double  xmax_mm  = 0.0;  ///< Peak linear excursion [mm] — 0 if unknown
    double  xlim_mm  = 0.0;  ///< Mechanical excursion limit [mm] — 0 if unknown

    // Optional added cone mass (e.g. felt rings, mass-loading test) [g]
    // Shifts fs, Qts, Qes, Qms while leaving Vas/Cms/Re/BL/Sd unchanged.
    double  addedMass_g = 0.0;

    // Computed results
    double  alpha   = 0.0;
    double  Fc      = 0.0;  ///< Sealed: system resonance; Vented: echoes fb
    double  Qtc     = 0.0;  ///< Sealed only (0 for vented)
    double  f3      = 0.0;
    double  eta     = 0.0;
    double  spl     = 0.0;
    double  portF2H = 0.0;  ///< Vented: 2nd pipe harmonic of port = C / Lp_eff [Hz]
    // Bandpass results
    double  f3Low            = 0.0;
    double  f3High           = 0.0;
    double  passbandRippleDb = 0.0;
    double  peakSpl          = 0.0;

    QColor  color;

    /// Plot visibility — toggled by left-clicking the model list.
    /// When false, the model is dimmed in the list and skipped on
    /// every plot.  Selection (active row) is independent of this.
    bool    visible = true;
};

// ─────────────────────────────────────────────────────────────────
//  ResponsePlot – log-frequency / dB SPL curves for sealed boxes.
// ─────────────────────────────────────────────────────────────────
class ResponsePlot : public QWidget
{
    Q_OBJECT
public:
    explicit ResponsePlot(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void clear();
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    void resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }
    void setPower(double watts);
    void setPerDriverMode(bool on) { m_perDriverMode = on; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

    double modelPower(const BoxModel &m) const
        { return m_power * (m_perDriverMode ? std::max(1, m.numDrivers) : 1); }

    QList<BoxModel>       m_models;
    int                   m_activeIdx    = -1;
    double                m_power        = 1.0;
    bool                  m_perDriverMode = false;
    double                m_cursorFreq   = -1.0;
    std::optional<double> m_yMin, m_yMax;
};

// ─────────────────────────────────────────────────────────────────
//  GroupDelayPlot – group delay curves for sealed-box models.
// ─────────────────────────────────────────────────────────────────
class GroupDelayPlot : public QWidget
{
    Q_OBJECT
public:
    explicit GroupDelayPlot(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void clear();
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    void resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

private:
    QList<BoxModel>       m_models;
    int                   m_activeIdx  = -1;
    double                m_cursorFreq = -1.0;
    std::optional<double> m_yMin, m_yMax;
};

// ─────────────────────────────────────────────────────────────────
//  VoltagePlot – voltage demand curves for a given applied power.
// ─────────────────────────────────────────────────────────────────
class VoltagePlot : public QWidget
{
    Q_OBJECT
public:
    explicit VoltagePlot(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void setPower(double watts);
    void clear();
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    void resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }
    void setPerDriverMode(bool on) { m_perDriverMode = on; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

    double modelPower(const BoxModel &m) const
        { return m_power * (m_perDriverMode ? std::max(1, m.numDrivers) : 1); }

    QList<BoxModel>       m_models;
    int                   m_activeIdx    = -1;
    double                m_power        = 1.0;
    bool                  m_perDriverMode = false;
    double                m_cursorFreq   = -1.0;
    std::optional<double> m_yMin, m_yMax;
};

// ─────────────────────────────────────────────────────────────────
//  ExcursionPlot – cone displacement vs frequency for a given power.
// ─────────────────────────────────────────────────────────────────
class ExcursionPlot : public QWidget
{
    Q_OBJECT
public:
    explicit ExcursionPlot(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void setPower(double watts);
    void clear();
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    void resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }
    void setPerDriverMode(bool on) { m_perDriverMode = on; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

    double modelPower(const BoxModel &m) const
        { return m_power * (m_perDriverMode ? std::max(1, m.numDrivers) : 1); }

    QList<BoxModel>       m_models;
    int                   m_activeIdx    = -1;
    double                m_power        = 1.0;
    bool                  m_perDriverMode = false;
    double                m_cursorFreq   = -1.0;
    std::optional<double> m_yMin, m_yMax;
};

// ─────────────────────────────────────────────────────────────────
//  PortVelocityPlot – peak port air velocity vs frequency.
// ─────────────────────────────────────────────────────────────────
class PortVelocityPlot : public QWidget
{
    Q_OBJECT
public:
    explicit PortVelocityPlot(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {640, 320}; }

    void setModels(const QList<BoxModel> &models, int activeIndex);
    void setPower(double watts);
    void clear();
    void setYRange(double min, double max) { m_yMin = min; m_yMax = max; update(); }
    void resetYRange()                     { m_yMin.reset(); m_yMax.reset(); update(); }
    void setPerDriverMode(bool on) { m_perDriverMode = on; update(); }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void leaveEvent(QEvent *) override;

    double modelPower(const BoxModel &m) const
        { return m_power * (m_perDriverMode ? std::max(1, m.numDrivers) : 1); }

    QList<BoxModel>       m_models;
    int                   m_activeIdx    = -1;
    double                m_power        = 100.0;
    bool                  m_perDriverMode = false;
    double                m_cursorFreq   = -1.0;
    std::optional<double> m_yMin, m_yMax;
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

// ─────────────────────────────────────────────────────────────────
//  EnclosureWidget – multi-model sealed-box comparison.
// ─────────────────────────────────────────────────────────────────
class EnclosureWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EnclosureWidget(DriverDatabase *db, QWidget *parent = nullptr);

    void refreshDriverList();
    void addDriverModel(int driverId);
    void setSpeedOfSound(double c);
    void setPlotRanges(const PlotRangeSettings &r);
    PlotRangeSettings plotRanges() const { return m_plotRanges; }

signals:
    void editDriverRequested(int id);

private slots:
    void onModelSelected(int row);
    void onAddModel();
    void onDuplicateModel();
    void onRemoveModel();
    void onRenameModel();
    void onDriverChanged(int index);
    void onResetToDriver();
    void onParamChanged();
    void onEncTypeChanged(int index);
    void onAlignBessel();
    void onAlignB2();
    void onAlignB4();

    void onPortShapeChanged(int index);
    void onViewDriverParams();

    void onSaveModel();
    void onLoadModel();
    void onSaveProject();
    void onLoadProject();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void    buildUi();
    void    applyPerDriverMode(bool on);
    void    refreshAutoName(int index);
    void    lockTsFields();
    void    unlockTsFields();
    void    recalculate(int index);
    void    recalculateAll();
    void    updateOptSealedHint();
    void    updatePlot();
    void    updateModelList();
    void    loadModelIntoFields(int index);
    void    clearFields();
    void    updatePortGroup();
    void    updatePortLength();
    QColor  nextColor() const;
    QString serializeModels() const;
    void    deserializeModels(const QString &json);

    DriverDatabase    *m_db;
    QList<BoxModel>    m_models;
    int                m_activeIdx = -1;
    bool               m_updating  = false;
    bool               m_tsLocked  = true;

    // Model list (left panel)
    QListWidget       *m_modelList = nullptr;

    // Driver selection
    QComboBox         *m_driverCombo     = nullptr;
    QSpinBox          *m_numDrivers      = nullptr;
    QPushButton       *m_btnViewDriver   = nullptr;
    QLabel            *m_vcTypeLbl       = nullptr;  ///< SVC / DVC wiring label

    // Editable parameters
    QDoubleSpinBox    *m_inFs   = nullptr;
    QDoubleSpinBox    *m_inVas  = nullptr;
    QDoubleSpinBox    *m_inQts  = nullptr;
    QDoubleSpinBox    *m_inQes  = nullptr;
    QDoubleSpinBox    *m_inAddedMass     = nullptr;
    QLabel            *m_addedMassEffLbl = nullptr;
    QDoubleSpinBox    *m_volume   = nullptr;
    QLabel            *m_volLabel = nullptr;   ///< "Box Vol:" label — hidden for IB
    QLabel            *m_ibInfinityLbl = nullptr; ///< "∞" big label, shown when IB selected
    // Bandpass front-chamber controls (visible only for BP4/BP6)
    QDoubleSpinBox    *m_volumeFront    = nullptr;
    QLabel            *m_volFrontLabel  = nullptr;
    QDoubleSpinBox    *m_inFbFront      = nullptr;
    QLabel            *m_fbFrontLabel   = nullptr;
    QDoubleSpinBox    *m_inQLFront      = nullptr;
    QLabel            *m_qlFrontLabel   = nullptr;

    // Enclosure type selector (in Model tab)
    QComboBox         *m_encType    = nullptr;

    // Port tab controls
    QTabWidget        *m_paramTabs      = nullptr;
    QWidget           *m_portTabContent = nullptr; ///< enabled only when vented
    QDoubleSpinBox    *m_inFb           = nullptr;
    QDoubleSpinBox    *m_inQL           = nullptr;
    QComboBox         *m_portShape      = nullptr;
    QWidget           *m_portDiamRow    = nullptr; ///< round-port section (shape=0)
    QWidget           *m_portRectRows   = nullptr; ///< rect-port section  (shape=1)
    QDoubleSpinBox    *m_inPortDiam     = nullptr;
    QDoubleSpinBox    *m_inPortWallThick= nullptr; ///< round port wall thickness
    QDoubleSpinBox    *m_inPortInsert        = nullptr; ///< round port insertion depth into box
    QSlider           *m_portInsertSlider   = nullptr; ///< 0–100% of computed port length
    QLabel            *m_portInsertPctLbl   = nullptr;
    QDoubleSpinBox    *m_inPortBraceSurf    = nullptr; ///< optional extra surface area (bracing) [cm²]
    QDoubleSpinBox    *m_inPortW        = nullptr;
    QDoubleSpinBox    *m_inPortH        = nullptr;
    QComboBox         *m_portWalls      = nullptr;  ///< rect only: shared walls (0–3)
    QSpinBox          *m_numPorts       = nullptr;
    QComboBox         *m_portFlare      = nullptr;  ///< 0=straight,1=one end,2=both
    QLabel            *m_portFlareNote  = nullptr;  ///< diagram + note, shown when flared
    QLabel            *m_portFbLabel    = nullptr;  ///< "fb:" row label in Port tab (hidden for BP)
    QLabel            *m_portQLLabel    = nullptr;  ///< "QL:" row label in Port tab (hidden for BP)
    QLabel            *m_portSectionHdr = nullptr;  ///< "REAR PORT" / "FRONT PORT" context header
    // Front port geometry section (BP6 only)
    QWidget           *m_frontPortSection  = nullptr;
    QComboBox         *m_portFrontShape    = nullptr;
    QWidget           *m_portFrontDiamRow  = nullptr;
    QDoubleSpinBox    *m_inPortFrontDiam   = nullptr;
    QWidget           *m_portFrontRectRows = nullptr;
    QDoubleSpinBox    *m_inPortFrontW      = nullptr;
    QDoubleSpinBox    *m_inPortFrontH      = nullptr;
    QComboBox         *m_portFrontWalls    = nullptr;
    QSpinBox          *m_numPortsFront          = nullptr;
    QDoubleSpinBox    *m_inPortFrontInsert      = nullptr;
    QSlider           *m_portFrontInsertSlider  = nullptr;
    QLabel            *m_portFrontInsertPctLbl  = nullptr;
    QDoubleSpinBox    *m_inPortFrontBraceSurf   = nullptr;
    QLabel            *m_portFrontAreaLbl     = nullptr;
    QLabel            *m_portFrontSurfAreaLbl = nullptr;
    QLabel            *m_portFrontVolDisplLbl = nullptr;
    QLabel            *m_portFrontLenLbl      = nullptr;
    QLabel            *m_portAreaLbl    = nullptr;
    QLabel            *m_portSurfAreaLbl= nullptr;  ///< inner lateral surface area
    QLabel            *m_portVolInnerLbl= nullptr;  ///< air volume inside port bore (all ports)
    QLabel            *m_portVolDisplLbl= nullptr;  ///< vol displaced in box (round, liters)
    QLabel            *m_portLenLbl     = nullptr;
    QLabel            *m_portLenEachLbl = nullptr;  ///< per-port length when >1 port
    QLabel            *m_portF2HLbl     = nullptr;  ///< 2nd pipe harmonic frequency

    // Chambers tab — rear port tuning (BP6 only: rear chamber is ported)
    QLabel            *m_fbRearBpLabel = nullptr;
    QDoubleSpinBox    *m_inFbRearBp    = nullptr;
    QLabel            *m_qlRearBpLabel = nullptr;
    QDoubleSpinBox    *m_inQLRearBp    = nullptr;

    // Chambers tab — vented fb/QL (mirrors Port tab m_inFb/m_inQL for vented mode)
    QLabel            *m_chambersFbLabel = nullptr;
    QDoubleSpinBox    *m_chambersFb      = nullptr;
    QLabel            *m_chambersQLLabel = nullptr;
    QDoubleSpinBox    *m_chambersQL      = nullptr;

    // Front port section (Port tab, BP6) — fb/QL mirrors Chambers tab m_inFbFront/m_inQLFront
    QDoubleSpinBox    *m_inFbFrontPort   = nullptr;
    QDoubleSpinBox    *m_inQLFrontPort   = nullptr;

    // Secondary driver params (locked spinboxes)
    QDoubleSpinBox *m_dpQms = nullptr, *m_dpRe  = nullptr;
    QDoubleSpinBox *m_dpBL  = nullptr, *m_dpMms = nullptr, *m_dpSd = nullptr;

    // Computed results
    QLabel *m_resAlpha  = nullptr;
    QLabel *m_resFc     = nullptr;
    QLabel *m_resQtc    = nullptr;
    QLabel *m_resF3     = nullptr;
    QLabel *m_resEta    = nullptr;
    QLabel *m_resSpl    = nullptr;
    QLabel *m_resLblFc  = nullptr;  ///< "Fc:" label (becomes "fb:" for vented)
    QLabel *m_resLblQtc = nullptr;  ///< "Qtc:" label (hidden for vented)
    QLabel *m_statusLbl = nullptr;

    // Plot tabs
    QTabWidget    *m_plotTabs  = nullptr;
    ResponsePlot     *m_splPlot  = nullptr;
    GroupDelayPlot   *m_gdPlot   = nullptr;
    VoltagePlot      *m_vpPlot   = nullptr;
    ExcursionPlot    *m_excPlot  = nullptr;
    PortVelocityPlot *m_pvPlot   = nullptr;
    // Alignment suggestion buttons (shown below Type combo)
    QWidget     *m_alignRow      = nullptr;
    QPushButton *m_btnBessel     = nullptr;
    QPushButton *m_btnB2         = nullptr;
    QPushButton *m_btnB4         = nullptr;
    QLabel      *m_lblOptSealed  = nullptr;
    QDoubleSpinBox   *m_power     = nullptr;
    QDoubleSpinBox   *m_excPower  = nullptr;
    QDoubleSpinBox   *m_pvPower   = nullptr;
    QDoubleSpinBox   *m_splPower  = nullptr;

    // Multi-driver wiring mode controls (in ndRow next to # Drivers)
    QButtonGroup     *m_wiringGroup       = nullptr;
    QRadioButton     *m_wiringBtnSeries   = nullptr;
    QRadioButton     *m_wiringBtnParallel = nullptr;
    QRadioButton     *m_wiringBtnSeparate = nullptr;

    // Per-driver / Total radio pairs — one set per power tab, all synced.
    // The member points at the "Per Driver" radio; "Total" is its group sibling.
    QRadioButton     *m_perDriverPower    = nullptr;  ///< SPL tab
    QRadioButton     *m_perDriverPowerVolt = nullptr; ///< Voltage tab
    QRadioButton     *m_perDriverPowerExc  = nullptr; ///< Excursion tab
    QRadioButton     *m_perDriverPowerPV   = nullptr; ///< Port Velocity tab

    QList<int>         m_driverIds;
    PlotRangeSettings  m_plotRanges;
};
