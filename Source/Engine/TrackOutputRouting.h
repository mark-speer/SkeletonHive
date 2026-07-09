#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct TrackOutputOption
{
    enum class Type { master, hardware, track, auxReturn, none };

    Type type = Type::none;
    juce::String deviceId;
    juce::String displayName;
    te::EditItemID trackId;
    int auxBusNumber = 0;
    bool available = true;
    bool isMidi = false;

    bool operator== (const TrackOutputOption& other) const noexcept;
    bool operator!= (const TrackOutputOption& other) const noexcept { return ! (*this == other); }
};

/** Per-track output routing via Tracktion TrackOutput (no parallel routing graph). */
struct TrackOutputRouting
{
    static juce::Array<TrackOutputOption> getOutputOptions (te::Edit& edit, te::AudioTrack& dest);
    static TrackOutputOption getActiveOutput (te::AudioTrack& dest);
    static bool setActiveOutput (te::AudioTrack& dest, const TrackOutputOption& option,
                                 juce::String& errorOut);
    static bool wouldCreateRoutingLoop (te::AudioTrack& source, te::AudioTrack& candidateDest);
    static bool shouldShowOutputSelector (const te::Track& track);
    static juce::String getOutputTooltip (te::AudioTrack& dest);
};

} // namespace skeletonhive
