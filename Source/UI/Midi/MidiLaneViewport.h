#pragma once

#include "TracktionCommon.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

/** Shared beat/time mapping for piano-roll lane components. Coordinates are
    absolute within the parent PianoRollEditor (lane local X == parent X). */
struct MidiLaneViewport
{
    std::function<float (double clipRelativeBeat)> beatToX;
    std::function<double (int x)> xToBeat;
    std::function<double (double beat, bool altDown)> snapBeat;
    std::function<juce::UndoManager*()> getUndoManager;
    std::function<double()> getClipOffsetBeats;
    EditViewState& editViewState;
};

} // namespace skeletonhive
