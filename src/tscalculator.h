#pragma once
#include "driverrecord.h"

// ─────────────────────────────────────────────────────────────────
//  TSCalculator – pure-static calculation engine.
//
//  All formulae from SB Acoustics "Measuring Thiele/Small
//  parameters" technical note (delta-mass method).
// ─────────────────────────────────────────────────────────────────

class TSCalculator
{
public:
    TSCalculator() = delete;

    /// Run all calculations and write results back into \p r.
    static void calculate(DriverRecord &r);

    /// Returns true if the raw inputs are self-consistent.
    static bool validateInputs(const DriverRecord &r, QString &errorMsg);

    /// Voltage across the driver in a series-resistor test circuit.
    /// V_source is in whatever unit the user chose (PP or RMS);
    /// the returned value is in the same unit.
    static double driverVoltage(double V_source, double R_s, double Z_driver);

    // Physical constants (20 °C, 50 % RH, 1 atm)
    static constexpr double RHO = 1.2;           ///< Air density    [kg/m³]
    static constexpr double C   = 344.0;          ///< Speed of sound [m/s]
    static constexpr double PI  = 3.14159265358979323846;
};
