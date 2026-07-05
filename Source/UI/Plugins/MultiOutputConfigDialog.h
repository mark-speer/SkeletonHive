#pragma once

#include "TracktionCommon.h"
#include "Engine/MultiOutputRouting.h"

namespace arrange
{

/** Dialog listing instrument output buses and linked child tracks. */
class MultiOutputConfigDialog
{
public:
    static void show (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                      juce::Component* centreAround);
};

} // namespace arrange
