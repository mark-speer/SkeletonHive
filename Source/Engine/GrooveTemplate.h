#pragma once

#include "TracktionCommon.h"

#include <array>

namespace skeletonhive
{

/** Per-16th-note-step timing/velocity offsets applied by groove humanize.
    Timing is a fraction of a 16th-note grid interval; velocity is an absolute delta. */
struct GrooveTemplate
{
    juce::String id;
    juce::String name;
    std::array<double, 16> timing {};
    std::array<int, 16> velocity {};
    bool isRandom = false;
    bool isBuiltIn = false;

    bool operator== (const GrooveTemplate& other) const
    {
        return id == other.id;
    }
};

} // namespace skeletonhive
