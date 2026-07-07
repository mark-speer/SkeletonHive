#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

enum class TrackInputKind { audio, midi };

struct TrackInputOption
{
    enum class Type { none, externalDevice, trackOutput };

    Type type = Type::none;
    te::EditItemID trackId;
    te::InputDevice* device = nullptr;
    juce::String displayName;

    bool operator== (const TrackInputOption& other) const noexcept;
    bool operator!= (const TrackInputOption& other) const noexcept { return ! (*this == other); }
};

/** Per-track exclusive input routing (audio wave / track wave, MIDI device / track MIDI). */
struct TrackInputRouting
{
    static juce::Array<TrackInputOption> getSourceOptions (te::Edit& edit, te::AudioTrack& dest, TrackInputKind kind);
    static TrackInputOption getActiveSource (te::AudioTrack& dest, TrackInputKind kind);
    static void setActiveSource (te::AudioTrack& dest, const TrackInputOption& option, TrackInputKind kind);

    static bool shouldShowAudioSource (const te::Track& track);
    static bool shouldShowMidiSource (const te::Track& track);

    static TrackInputOption getFirstExternalOption (te::Edit& edit, TrackInputKind kind);
    static void assignFirstExternalSource (te::AudioTrack& dest, TrackInputKind kind);
};

} // namespace skeletonhive
