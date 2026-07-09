#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

enum class TrackInputKind { audio, midi };

struct TrackInputOption
{
    enum class Type { none, externalDevice, trackOutput, allMidiInputs };

    Type type = Type::none;
    te::EditItemID trackId;
    te::InputDevice* device = nullptr;
    juce::String displayName;
    /** 0 = all MIDI channels; 1-16 = specific channel. Device-level filter in TE. */
    int midiChannel = 0;
    /** Wave input channel index for mono/stereo selection (-1 = device default). */
    int waveChannelIndex = -1;
    bool available = true;

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

    static juce::String getInputTooltip (te::AudioTrack& dest, TrackInputKind kind);
    static void applyMidiChannelFilter (te::InputDevice& device, int channel);
    static juce::Array<TrackInputOption> filterOptionsBySearch (const juce::Array<TrackInputOption>& options,
                                                                const juce::String& search);
};

} // namespace skeletonhive
