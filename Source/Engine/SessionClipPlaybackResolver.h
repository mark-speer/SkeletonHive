#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Applies transient playback mutes for session MIDI clips (scale lock, probability, iteration). */
struct SessionClipPlaybackResolver
{
    static void applyPlaybackMasks (te::MidiClip& clip, int loopCycleIndex, juce::Random& rng);
    static void restorePlaybackMasks (te::MidiClip& clip);
    static bool hasActiveMasks (const te::MidiClip& clip);
};

} // namespace skeletonhive
