#pragma once
#include "driverrecord.h"
#include <optional>

// ─────────────────────────────────────────────────────────────────
//  DriverFilter – parametric range filter for the driver list.
//  Unset bounds don't constrain. Pure logic, unit-tested.
// ─────────────────────────────────────────────────────────────────
struct DriverFilter
{
    std::optional<double> fsMin,  fsMax;    // [Hz]
    std::optional<double> qtsMin, qtsMax;   // [-]
    std::optional<double> vasLMin, vasLMax; // [litres]
    std::optional<double> reMin,  reMax;    // [Ω]

    bool isActive() const
    {
        return fsMin || fsMax || qtsMin || qtsMax
            || vasLMin || vasLMax || reMin || reMax;
    }

    bool matches(const DriverRecord &r) const
    {
        auto inRange = [](double v, const std::optional<double> &lo,
                                    const std::optional<double> &hi) {
            if (lo && v < *lo) return false;
            if (hi && v > *hi) return false;
            return true;
        };
        return inRange(r.fs,           fsMin,   fsMax)
            && inRange(r.Qts,          qtsMin,  qtsMax)
            && inRange(r.Vas * 1000.0, vasLMin, vasLMax)   // m³ → litres
            && inRange(r.Re,           reMin,   reMax);
    }
};
