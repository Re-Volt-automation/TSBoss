#pragma once
#include <QString>
#include <QDate>
#include <QtGlobal>

// ── Field origin bitmask constants ────────────────────────────────
namespace FieldOrigin {
    static constexpr quint64 Re  = 1ULL <<  0;
    static constexpr quint64 Qts = 1ULL <<  1;
    static constexpr quint64 Qms = 1ULL <<  2;
    static constexpr quint64 Qes = 1ULL <<  3;
    static constexpr quint64 Mms = 1ULL <<  4;
    static constexpr quint64 Cms = 1ULL <<  5;
    static constexpr quint64 Rms = 1ULL <<  6;
    static constexpr quint64 BL  = 1ULL <<  7;
    static constexpr quint64 Sd  = 1ULL <<  8;
    static constexpr quint64 Dd  = 1ULL <<  9;
    static constexpr quint64 Vas = 1ULL << 10;
    static constexpr quint64 Spl = 1ULL << 11;
    static constexpr quint64 AllTS = Qts|Qms|Qes|Mms|Cms|Rms|BL|Sd|Vas;
}

// ─────────────────────────────────────────────────────────────────
//  DriverRecord – stores every raw measurement AND every calculated
//  Thiele/Small parameter for one loudspeaker driver.
//
//  Method: SB Acoustics delta-mass / constant-voltage impedance.
//  Reference: "Measuring Thiele/Small parameters", SB Acoustics.
// ─────────────────────────────────────────────────────────────────

struct DriverRecord
{
    // ── Identity ──────────────────────────────────────────────────
    int     id           = -1;
    QString make;
    QString model;
    QDate   dateMeasured;
    QString measuredBy;
    QString notes;

    // ── Measurement setup ─────────────────────────────────────────
    double V_meas      = 1.0;   ///< Applied measurement voltage (user's chosen format)  [V]
    double R_s         = 50.0;  ///< Series resistor in test circuit                      [Ω]
    int    voltageMode = 0;     ///< 0 = RMS, 1 = Peak-to-Peak

    // ── Raw measured values (SI units) ───────────────────────────
    double Re     = 0.0;  ///< Voice coil DC resistance           [Ω]
    double fs     = 0.0;  ///< Free-air resonance frequency        [Hz]
    double Zmax   = 0.0;  ///< Peak impedance at fs               [Ω]
    double f1     = 0.0;  ///< Lower side frequency (Z = Z₁₂)     [Hz]
    double f2     = 0.0;  ///< Upper side frequency (Z = Z₁₂)     [Hz]
    double deltaM = 0.0;  ///< Added mass (plasticine)             [kg]
    double fo     = 0.0;  ///< Resonance freq with added mass      [Hz]
    double Dd     = 0.0;  ///< Effective piston diameter           [m]
    double Zmin   = 0.0;  ///< Min impedance above resonance      [Ω]
    double f3     = 0.0;  ///< Frequency where Z = √2 · Zmin      [Hz]

    // ── Calculated Thiele/Small parameters ───────────────────────
    double Z12      = 0.0;  ///< Side freq impedance √(Re·Zmax)   [Ω]
    double fsVerify = 0.0;  ///< √(f1·f2) – should ≈ fs           [Hz]
    double R0       = 0.0;  ///< Zmax / Re                         [-]
    double Qms      = 0.0;  ///< Mechanical Q-factor               [-]
    double Qes      = 0.0;  ///< Electrical Q-factor               [-]
    double Qts      = 0.0;  ///< Total Q-factor                    [-]
    double mms      = 0.0;  ///< Moving mass incl. air             [kg]
    double Rms      = 0.0;  ///< Mechanical loss resistance        [kg/s]
    double BL       = 0.0;  ///< Force factor                      [Tm]
    double Cms      = 0.0;  ///< Suspension compliance             [m/N]
    double Sd       = 0.0;  ///< Effective piston area             [m²]
    double Vas      = 0.0;  ///< Equivalent air volume             [m³]
    double Le       = 0.0;  ///< Voice-coil inductance (empirical) [H]

    // ── Additional linear parameters ─────────────────────────────
    double Znom = 0.0;  ///< Nominal impedance rating              [Ω]
    double fLe  = 0.0;  ///< Frequency at which Le was measured    [Hz]
    double KLe  = 0.0;  ///< Semi-inductance coefficient           [H·s^(1-n)]

    // ── Large signal parameters ───────────────────────────────────
    double Xmax = 0.0;  ///< Peak linear excursion                 [mm]
    double Xlim = 0.0;  ///< Mechanical travel limit               [mm]
    double Pe   = 0.0;  ///< Rated continuous power                [W]
    double Hg   = 0.0;  ///< Magnetic gap height                   [mm]
    double Vd   = 0.0;  ///< Peak volume displacement (Sd·Xmax)    [cm³] – calculated
    double Spl  = 0.0;  ///< Reference sensitivity (1W/1m)          [dB SPL]

    // ── Voice coil type ───────────────────────────────────────────
    bool isDVC     = false; ///< Dual voice coil?
    int  dvcWiring = 0;     ///< 0=series, 1=parallel (only when isDVC)

    // ── Field provenance ──────────────────────────────────────────
    quint64 fieldOrigins     = 0; ///< Bitmask — bit set = value was accepted from a suggest button
    quint64 userEnteredFields = 0; ///< Bitmask — bit set = value was typed by the user

    bool hasResults() const { return Qts > 0.0 && Vas > 0.0; }
    bool isValid()    const { return !make.isEmpty() && !model.isEmpty()
                                     && fs > 0; }
};
