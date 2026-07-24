#pragma once
#include "boxmodel.h"

// ─────────────────────────────────────────────────────────────────
//  mounting – isobaric driver transform (see the 2026-07-24 design
//  spec). An ideal rigidly-coupled pair behaves as one composite
//  driver; baking that into a copy of the model lets every simulation
//  (sealed closed-form, ported, bandpass, thermal) stay
//  mounting-agnostic, exactly like the added-mass transform.
//  Unit-tested in tests/isobaric_tests.cpp against the classic
//  equivalence: pair in Vb ≡ single driver in 2·Vb, −3 dB, 2×|Z|.
// ─────────────────────────────────────────────────────────────────

namespace mounting {

/// Bake the mounting mode into the bare driver parameters and clear the
/// flag (idempotent). fs and the Q factors are untouched — the sims derive
/// Cms from fs+mms and Rms from fs+mms+Qms, so the pair's halved
/// compliance and doubled mechanical resistance follow automatically.
inline BoxModel withMounting(BoxModel m)
{
    if (m.mounting != BoxModel::Mounting::Isobaric)
        return m;
    m.mms_g *= 2.0;          // coupled moving masses
    m.BL    *= 2.0;          // pair internally series: forces add
    m.Re    *= 2.0;          //   … and resistances add
    m.Vas_L /= 2.0;          // two suspensions flexing together
    m.pe_W  *= 2.0;          // two voice coils dissipating
    m.mounting = BoxModel::Mounting::Normal;
    return m;
}

inline bool isIsobaric(const BoxModel &m)
{ return m.mounting == BoxModel::Mounting::Isobaric; }

} // namespace mounting
