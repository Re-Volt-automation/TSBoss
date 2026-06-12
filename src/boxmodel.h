#pragma once
#include <QString>
#include <QColor>

// Plain data model for one enclosure design. No Qt Widgets dependency so the
// acoustic math in portphysics.h can be unit-tested against it.
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

    // Velocity sensitivity of the port end correction (acoustic-mass term).
    // 0 = off (v1 default). Wired into portZ() for later validation.
    double  mapVelCoeff = 0.0;

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
    double  pe_W     = 0.0;  ///< Rated continuous (RMS) power per driver [W] — 0 if unknown

    // Optional added cone mass (e.g. felt rings, mass-loading test) [g]
    // Shifts fs, Qts, Qes, Qms while leaving Vas/Cms/Re/BL/Sd unchanged.
    double  addedMass_g = 0.0;

    // Subsonic protection filter (Butterworth high-pass before the amp).
    int     hpfOrder = 0;     ///< 0 = off, 2 = 12 dB/oct, 4 = 24 dB/oct
    double  hpfFreq  = 20.0;  ///< −3 dB corner frequency [Hz]

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
