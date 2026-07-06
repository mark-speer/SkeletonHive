#include "MidiScale.h"

namespace skeletonhive
{

namespace
{
constexpr int scaleMasks[][12] =
{
    { 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1 },  // Major
    { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 }   // Natural minor
};
} // namespace

const int* MidiScale::getScaleMask (ScaleMode mode)
{
    switch (mode)
    {
        case ScaleMode::major: return scaleMasks[0];
        case ScaleMode::minor: return scaleMasks[1];
        default:               return nullptr;
    }
}

bool MidiScale::isPitchInScale (int pitch, int root, ScaleMode mode)
{
    const auto* mask = getScaleMask (mode);
    if (mask == nullptr)
        return false;

    const int rootPc = ((root % 12) + 12) % 12;
    return mask[((pitch - rootPc) % 12 + 12) % 12] != 0;
}

int MidiScale::nearestInScalePitch (int pitch, int root, ScaleMode mode)
{
    if (mode == ScaleMode::none || isPitchInScale (pitch, root, mode))
        return pitch;

    for (int delta = 1; delta <= 6; ++delta)
    {
        if (isPitchInScale (pitch - delta, root, mode))
            return juce::jlimit (0, 127, pitch - delta);
        if (isPitchInScale (pitch + delta, root, mode))
            return juce::jlimit (0, 127, pitch + delta);
    }

    return pitch;
}

juce::String MidiScale::scaleModeName (ScaleMode mode)
{
    switch (mode)
    {
        case ScaleMode::major: return "Major";
        case ScaleMode::minor: return "Minor";
        default:               return "No Scale";
    }
}

} // namespace skeletonhive
