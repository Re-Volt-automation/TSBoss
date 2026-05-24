#pragma once
#include <QString>
#include "boxmodel.h"
#include "diagrams/diagrampainter.h"

// Does the outer (baffle-side) end flare?  portFlare: 0 none, 1 outer, 2 both.
inline bool portFlareOuter(int portFlare) { return portFlare >= 1; }
// Does the inner (in-box) end flare?
inline bool portFlareInner(int portFlare) { return portFlare >= 2; }

// Face-dimension label, e.g. "Ø 75 mm" (round) or "80 × 50 mm" (rect).
inline QString portFaceLabel(const BoxModel &m)
{
    if (m.portShape == 0)
        return QString("Ø %1 mm").arg(int(m.portWidth_mm + 0.5));
    return QString("%1 × %2 mm").arg(int(m.portWidth_mm + 0.5))
                                .arg(int(m.portHeight_mm + 0.5));
}

// Bandpass rear chamber: sealed (BP4) vs vented/ported (BP6).
inline bool bandpassRearSealed(const BoxModel &m)
{ return m.encType == BoxModel::EncType::Bandpass4; }

// Front-port face label (mirror of portFaceLabel, reads the portFront* fields).
inline QString portFaceLabelFront(const BoxModel &m)
{
    if (m.portFrontShape == 0)
        return QString("Ø %1 mm").arg(int(m.portFrontWidth_mm + 0.5));
    return QString("%1 × %2 mm").arg(int(m.portFrontWidth_mm + 0.5))
                                .arg(int(m.portFrontHeight_mm + 0.5));
}

// Draw a schematic side cross-section of the port into the painter's area.
void drawPortSection(DiagramPainter &d, const BoxModel &m);
