#pragma once
#include "boxmodel.h"
#include "driverrecord.h"
#include "driverdb.h"
#include "driverdetailwidget.h"
#include "diagrams/diagramview.h"
#include "plots/modelplots.h"
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
    void onAlignBP4Flat();
    void onAlignBP6Flat();

    void onPortShapeChanged(int index);
    void onViewDriverParams();

    void onSaveModel();
    void onLoadModel();
    void onSaveProject();
    void onLoadProject();
    void onExportPdf();

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
    void    rebuildPortArrangement(bool bp6);  ///< re-home Port-tab blocks per enclosure type
    QColor  nextColor() const;
    /// Build a default filename stem from the loaded models:
    /// "Nx_Brand1-Brand2" with hyphens in brand names escaped to underscores.
    /// Brands are deduplicated case-insensitively and sorted; driver IDs are
    /// deduplicated so the same driver isn't queried twice. Falls back to
    /// "enclosure" when no brands resolve.
    static QString deriveProjectFileBaseName(const QList<BoxModel> &models,
                                             DriverDatabase *db);

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
    QComboBox         *m_hpfOrder        = nullptr;  ///< subsonic filter: Off / 12 / 24 dB/oct
    QDoubleSpinBox    *m_hpfFreq         = nullptr;  ///< subsonic corner frequency [Hz]
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
    DiagramView       *m_portDiagram    = nullptr;  ///< schematic port cross-section
    QLabel            *m_portFbLabel    = nullptr;  ///< "fb:" row label in Port tab (hidden for BP)
    QLabel            *m_portQLLabel    = nullptr;  ///< "QL:" row label in Port tab (hidden for BP)
    QLabel            *m_portSectionHdr = nullptr;  ///< "REAR PORT" / "FRONT PORT" context header
    QWidget           *m_portColControls = nullptr; ///< rear tuning/shape/flare column (col1)
    QWidget           *m_portDimsBlock   = nullptr; ///< rear dimension rows (round/rect)
    QWidget           *m_portBraceBlock  = nullptr; ///< rear bracing-surface row
    QWidget           *m_portColResults  = nullptr; ///< rear results grid column (col3)
    QWidget           *m_portArrangeHost = nullptr; ///< host whose layout is rebuilt per enclosure type
    // Front port geometry section (BP6 only)
    QWidget           *m_frontPortSection  = nullptr;
    QComboBox         *m_portFrontShape    = nullptr;
    QComboBox         *m_portFrontFlare    = nullptr;  ///< 0=straight,1=one end,2=both
    QWidget           *m_portFrontDiamRow  = nullptr;
    QDoubleSpinBox    *m_inPortFrontDiam   = nullptr;
    QDoubleSpinBox    *m_inPortFrontWallThick = nullptr; ///< front round port wall thickness
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
    QLabel            *m_portFrontVolInnerLbl = nullptr;  ///< front port air volume
    QLabel            *m_portFrontVolDisplLbl = nullptr;
    QLabel            *m_portFrontLenLbl      = nullptr;
    QLabel            *m_portFrontLenEachLbl  = nullptr;  ///< front per-port length
    QLabel            *m_portFrontF2HLbl      = nullptr;  ///< front 2nd pipe harmonic
    QLabel            *m_portAreaLbl    = nullptr;
    QLabel            *m_portSurfAreaLbl= nullptr;  ///< inner lateral surface area
    QLabel            *m_portVolInnerLbl= nullptr;  ///< air volume inside port bore (all ports)
    QLabel            *m_portVolDisplLbl= nullptr;  ///< vol displaced in box (round, liters)
    QLabel            *m_portLenLbl     = nullptr;
    QLabel            *m_portLenEachLbl = nullptr;  ///< per-port length when >1 port
    QLabel            *m_portLenEachRowLbl = nullptr; ///< its row label — row hidden for 1 port
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
    QPushButton *m_btnExportPdf = nullptr;

    // Plot tabs
    QTabWidget    *m_plotTabs  = nullptr;
    ResponsePlot     *m_splPlot  = nullptr;
    GroupDelayPlot   *m_gdPlot   = nullptr;
    VoltagePlot      *m_vpPlot   = nullptr;
    ExcursionPlot    *m_excPlot  = nullptr;
    PortVelocityPlot *m_pvPlot   = nullptr;
    MaxSplPlot       *m_maxPlot  = nullptr;
    // Alignment suggestion buttons (shown below Type combo)
    QWidget     *m_alignRow      = nullptr;
    QPushButton *m_btnBessel     = nullptr;
    QPushButton *m_btnB2         = nullptr;
    QPushButton *m_btnB4         = nullptr;
    QPushButton *m_btnBP4        = nullptr;
    QPushButton *m_btnBP6        = nullptr;
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
