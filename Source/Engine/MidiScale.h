#pragma once

#include <juce_core/juce_core.h>

namespace skeletonhive
{

enum class ScaleMode
{
    none = 0,
    major,
    minor
};

struct MidiScale
{
    static bool isPitchInScale (int pitch, int root, ScaleMode mode);
    static int nearestInScalePitch (int pitch, int root, ScaleMode mode);
    static const int* getScaleMask (ScaleMode mode);
    static juce::String scaleModeName (ScaleMode mode);
};

} // namespace skeletonhive
