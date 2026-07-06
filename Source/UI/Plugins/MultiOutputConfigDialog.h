#pragma once

#include "TracktionCommon.h"
#include "Engine/MultiOutputRouting.h"

namespace skeletonhive
{

/** Dialog listing instrument output buses and linked child tracks. */
class MultiOutputConfigDialog
{
public:
    static void show (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                      juce::Component* centreAround);
};

} // namespace skeletonhive
