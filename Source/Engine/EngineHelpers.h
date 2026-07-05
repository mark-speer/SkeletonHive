#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class ExtendedUIBehaviour;

enum class TrackKind { audio, midi };

struct EngineHelpers
{
    static const juce::Identifier trackKindProperty;
    static const juce::Identifier clipGroupProperty;

    static te::Project::Ptr createTempProject (te::Engine& engine);

    static void showAudioDeviceSettings (te::Engine& engine);

    static void browseForAudioFile (te::Engine& engine, std::function<void (const juce::File&)> callback);

    static te::AudioTrack* getOrInsertAudioTrackAt (te::Edit& edit, int index);

    static te::AudioTrack* getOrInsertAudioTrack (te::Edit& edit);

    static te::AudioTrack* getOrInsertTrackForMidi (te::Edit& edit, int index);

    static void setTrackKind (te::Track& track, TrackKind kind);
    static TrackKind getTrackKind (const te::Track& track);

    static te::WaveAudioClip::Ptr loadAudioFileAsClip (te::Edit& edit, const juce::File& file, int trackIndex = 0);

    static te::MidiClip::Ptr createMidiClip (te::Edit& edit, int trackIndex,
                                             te::TimeRange range, const juce::String& name = "MIDI");

    static te::MidiClip::Ptr createMidiClipOnTrack (te::Track& track, te::TimeRange range,
                                                    const juce::String& name = "MIDI");

    /** Clones a clip on its own track. If placeAfterOriginal is true the copy
        starts where the original ends, otherwise it sits on top of the original. */
    static te::Clip* duplicateClip (te::Clip& clip, bool placeAfterOriginal);

    // Minimal clip grouping: clips sharing a non-empty group id move together.
    static juce::String getClipGroup (const te::Clip& clip);
    static void setClipGroup (te::Clip& clip, const juce::String& groupId);
    static juce::Array<te::Clip*> getClipsInGroup (te::Edit& edit, const juce::String& groupId);

    /** Finds the audio track hosting an AuxReturnPlugin for the given bus,
        creating a new return track (with the return plugin at chain start) if needed. */
    static te::AudioTrack* getOrCreateReturnTrack (te::Edit& edit, int busNumber = 0);

    /** Returns the track's AuxSendPlugin for a bus, inserting one just before
        the volume plugin if the track doesn't have one yet. */
    static te::AuxSendPlugin* getOrCreateAuxSend (te::AudioTrack& track, int busNumber = 0);

    static void togglePlay (te::Edit& edit, bool returnToStart = false);

    static void toggleRecord (te::Edit& edit);

    static void armTrack (te::AudioTrack& track, bool arm, int position = 0);

    static bool isTrackArmed (te::AudioTrack& track, int position = 0);

    static void enableAllInputs (te::Edit& edit);

    static void setupDefaultTracks (te::Edit& edit);

    static juce::String timeToTimecodeString (double seconds);

    static juce::String getPositionString (te::Edit& edit);

    static void prepareEngineForShutdown (te::Engine& engine, te::Edit* edit);
    static void releaseAudioDevices (te::Engine& engine);
};

} // namespace arrange
