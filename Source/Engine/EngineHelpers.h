#pragma once

#include "MidiScale.h"
#include "TracktionCommon.h"
#include "GrooveTemplate.h"

namespace skeletonhive
{

class ExtendedUIBehaviour;
class EditViewState;

struct EngineHelpers
{
    static const juce::Identifier clipGroupProperty;
    static const juce::Identifier clipGroupOuterProperty;
    static const juce::Identifier clipGroupColourProperty;
    static const juce::Identifier sessionSlotIdProperty;
    static const juce::Identifier clipScaleRootProperty;
    static const juce::Identifier clipScaleModeProperty;
    static const juce::Identifier clipScaleLockProperty;
    static const juce::Identifier noteProbabilityProperty;
    static const juce::Identifier noteIterationProperty;

    static const juce::Identifier soloedPluginIdProperty;
    static const juce::Identifier trackKindProperty;

    enum class TrackKind { audio, midi };

    static void setTrackKind (te::Track& track, TrackKind kind);
    static TrackKind getTrackKind (const te::Track& track);
    static bool isAudioKindTrack (const te::Track& track);
    static bool isMidiKindTrack (const te::Track& track);
    static void ensureTrackKinds (te::Edit& edit);

    static te::Project::Ptr createTempProject (te::Engine& engine);

    static void showAudioDeviceSettings (te::Engine& engine);

    static void startParameterMidiLearn (te::Edit& edit, te::AutomatableParameter& parameter);
    static void removeParameterMidiMapping (te::Edit& edit, te::AutomatableParameter& parameter);
    static bool isParameterMidiMapped (te::Edit& edit, te::AutomatableParameter& parameter);
    static void setMidiLearnActive (te::Engine& engine, te::Edit& edit, bool active);
    static bool isMidiLearnActive (te::Engine& engine);

    static void browseForAudioFile (te::Engine& engine, std::function<void (const juce::File&)> callback);

    static te::AudioTrack* getOrInsertAudioTrackAt (te::Edit& edit, int index);

    static te::AudioTrack* getOrInsertAudioTrack (te::Edit& edit);

    static te::AudioTrack* getOrInsertTrackForMidi (te::Edit& edit, int index);

    // TE tracks are content-agnostic; track kind is stored in skeletonHiveTrackKind
    // for UI/routing presentation. Clip-based helpers remain for drag/create gates.
    /** True if the track has only MIDI clips (used when kind property is absent). */
    static bool isMidiTrack (const te::Track& track);
    /** True unless the track already has audio clips (used to gate MIDI
        drag-to-create so an empty track can start out as either kind). */
    static bool canHostMidiClips (const te::Track& track);
    /** True if the track hosts an AuxReturnPlugin (see getOrCreateReturnTrack). */
    static bool isReturnTrack (const te::Track& track);
    /** Nesting depth under FolderTrack parents, for header indentation. */
    static int getTrackIndentLevel (const te::Track& track);

    /** Creates an empty folder track at the end of the track list. */
    static te::FolderTrack* createFolderTrack (te::Edit& edit, te::SelectionManager* selectionManager = nullptr);

    static te::WaveAudioClip::Ptr loadAudioFileAsClip (te::Edit& edit, const juce::File& file, int trackIndex = 0);

    static te::MidiClip::Ptr createMidiClip (te::Edit& edit, int trackIndex,
                                             te::TimeRange range, const juce::String& name = "MIDI");

    static te::MidiClip::Ptr createMidiClipOnTrack (te::Track& track, te::TimeRange range,
                                                    const juce::String& name = "MIDI");

    /** Clones a clip on its own track. If placeAfterOriginal is true the copy
        starts where the original ends, otherwise it sits on top of the original. */
    static te::Clip* duplicateClip (te::Clip& clip, bool placeAfterOriginal);

    // Two-tier clip grouping: inner group (move together) + optional outer super-group.
    static juce::String getClipGroup (const te::Clip& clip);
    static void setClipGroup (te::Clip& clip, const juce::String& groupId);
    static juce::String getClipOuterGroup (const te::Clip& clip);
    static void setClipOuterGroup (te::Clip& clip, const juce::String& outerGroupId);
    static juce::Array<te::Clip*> getClipsInGroup (te::Edit& edit, const juce::String& groupId);
    static juce::Array<te::Clip*> getClipsSharingOuterGroup (te::Edit& edit, const juce::String& outerGroupId);
    /** All clips that should move/resize with this clip (inner + outer peers, excluding clip). */
    static juce::Array<te::Clip*> getGroupedPeers (te::Clip& clip);

    static bool canMoveClipToTrack (const te::Clip& clip, te::ClipTrack& dest);
    static void moveClipToTrack (te::Clip& clip, te::ClipTrack& dest, te::TimePosition start);
    /** Moves clip and grouped peers by the same track-row offset; returns false if any move is invalid. */
    static bool moveClipGroupToTrack (te::Clip& leader, te::ClipTrack& destTrack, te::TimePosition start);

    enum class TrackDropZone { above, below, intoFolder, promoteTopLevel };
    static bool canReparentTrack (te::Track& dragged, te::Track& hoverRow, TrackDropZone zone);
    static te::TrackInsertPoint insertPointForDrop (te::Edit& edit, te::Track& hoverRow, TrackDropZone zone);
    static void moveTrackToInsertPoint (te::Edit& edit, te::Track& track, te::TrackInsertPoint point);
    static void moveTrackOutOfFolder (te::Track& track);
    static void moveTrackBySiblingDelta (te::Track& track, int delta);
    static te::ClipTrack* getClipTrackAtArrangementIndex (te::Edit& edit, int index);
    static int getArrangementTrackIndex (te::Edit& edit, te::Track& track);

    /** Payload for track header drag-and-drop. */
    static juce::String encodeTrackDrag (te::EditItemID trackId);
    static te::EditItemID parseTrackDrag (const juce::var& description);

    /** Clips on the given track starting strictly after the anchor time, sorted by start time.
        The ordering primitive ripple editing shifts to make/close room. */
    static juce::Array<te::Clip*> getClipsStartingAfter (te::ClipTrack& track, te::TimePosition anchor);

    /** Deterministic colour for a group id, used as the fallback when a clip has no
        explicit stored colour (e.g. groups created before this property existed). */
    static juce::Colour colourForGroupId (const juce::String& groupId);
    static juce::Colour getClipGroupColour (const te::Clip& clip);
    static void setClipGroupColour (te::Clip& clip, juce::Colour colour);
    /** TE clip colour when set, otherwise the supplied default chrome colour. */
    static juce::Colour getClipFillColour (const te::Clip& clip, juce::Colour defaultColour);

    /** Finds the audio track hosting an AuxReturnPlugin for the given bus,
        creating a new return track (with the return plugin at chain start) if needed. */
    static te::AudioTrack* getOrCreateReturnTrack (te::Edit& edit, int busNumber = 0);

    /** Returns the track's AuxSendPlugin for a bus, inserting one just before
        the volume plugin if the track doesn't have one yet. */
    static te::AuxSendPlugin* getOrCreateAuxSend (te::AudioTrack& track, int busNumber = 0);

    static constexpr int maxAuxBuses = 3;

    static juce::String auxBusName (int busNumber);
    static juce::Array<te::AuxSendPlugin*> getAllAuxSends (te::AudioTrack& track);
    static te::AuxSendPlugin* addAuxSend (te::AudioTrack& track, int busNumber);
    static bool isSendPreFader (te::AudioTrack& track, const te::AuxSendPlugin& send);
    static void setSendPreFader (te::AudioTrack& track, te::AuxSendPlugin& send, bool preFader);

    /** User-chain plugins shown in the track footer (excludes built-ins and aux). */
    static bool isFooterVisiblePlugin (const te::Plugin& plugin);

    /** Index in pluginList to insert before the volume plugin (end of user chain). */
    static int getUserChainInsertIndex (te::AudioTrack& track);

    static te::Plugin* insertPluginOnTrack (te::AudioTrack& track, te::Plugin::Ptr plugin, int index = -1);

    static juce::PluginDescription getPluginDescription (const te::Plugin& plugin);
    static te::Plugin::Ptr createPluginFromDescription (te::Edit& edit, const juce::PluginDescription& desc);
    static bool isInstrumentDescription (const juce::PluginDescription& desc);
    static bool isInstrumentPlugin (const te::Plugin& plugin);
    static int findInstrumentSlot (te::AudioTrack& track);

    static bool movePluginToUserSlot (te::AudioTrack& track, te::Plugin& plugin, int userSlot);
    static bool movePluginToTrack (te::Plugin& plugin, te::AudioTrack& destTrack, int userSlot);
    static te::Plugin* duplicatePluginOnTrack (te::Plugin& source, te::AudioTrack& track, int userSlot = -1);
    static te::Plugin* replacePluginOnTrack (te::AudioTrack& track, te::Plugin& oldPlugin,
                                             const juce::PluginDescription& newDesc);
    static te::Plugin* replacePluginInRack (te::RackInstance& rack, te::Plugin& oldPlugin,
                                            const juce::PluginDescription& newDesc);
    static void applyDefaultDeviceChain (te::AudioTrack& track, const juce::StringArray& pluginIdentifiers,
                                         te::Engine& engine, bool expectsInstrumentFirst);
    static void renamePlugin (te::Plugin& plugin, const juce::String& newName);

    static juce::PluginDescription lookupKnownPlugin (te::Engine& engine, const juce::String& identifierString);

    enum class PluginLoadState
    {
        ok,
        loading,
        failed
    };

    static PluginLoadState getExternalPluginLoadState (te::Plugin& plugin, juce::String& statusMessage);
    static juce::String getExternalPluginLoadError (te::Plugin& plugin);
    static void showPluginLoadFailureAlert (juce::Component* parent,
                                            const juce::String& pluginName,
                                            const juce::String& errorMessage);
    static void showPluginLoadFailureAlert (juce::Component* parent, te::Plugin& plugin);
    static void showPluginInsertFailureAlert (juce::Component* parent, const juce::PluginDescription& desc);

    // Wet/dry mix (ExternalPlugin + RackInstance)
    static bool hasWetDryMix (const te::Plugin& plugin);
    static te::AutomatableParameter* getDryParam (te::Plugin& plugin);
    static te::AutomatableParameter* getWetParam (te::Plugin& plugin);

    // Solo-device monitoring (per-track ValueTree property)
    static te::EditItemID getSoloedPluginId (const te::Track& track);
    static void setSoloedPlugin (te::Track& track, te::Plugin* plugin);
    static void clearSoloedPlugin (te::Track& track);
    static bool isPluginSoloed (const te::Track& track, const te::Plugin& plugin);

    static te::RackInstance* wrapPluginsInRack (te::SelectionManager& selection);
    static te::RackInstance* insertEmptyRack (te::AudioTrack& track);

    static juce::Array<te::Plugin*> getRackInternalPlugins (te::RackInstance& rack);
    static int rackSlotForPlugin (te::RackInstance& rack, const te::Plugin& plugin);
    static bool movePluginInRack (te::RackInstance& rack, te::Plugin& plugin, int targetSlot);
    static te::RackInstance* findRackOnTrack (te::AudioTrack& track, te::EditItemID rackInstanceId);

    static void togglePlay (te::Edit& edit, bool returnToStart = false);

    static void toggleRecord (te::Edit& edit);

    static void armTrack (te::AudioTrack& track, bool arm, int position = 0);

    static bool isTrackArmed (te::AudioTrack& track, int position = 0);

    /** Input device instances currently targeting the track. */
    static juce::Array<te::InputDeviceInstance*> getInputInstancesForTrack (te::AudioTrack& track);

    /** Assigns all physical/virtual MIDI inputs (preferMidi) or the first wave
        input to the track so it can be armed for recording. */
    static void assignDefaultInputToTrack (te::AudioTrack& track, bool preferMidi);

    /** Arms the track for recording. If no input targets it yet, a default
        input is assigned first: MIDI inputs for tracks whose content is MIDI,
        otherwise the first wave input. */
    static void armTrackWithDefaultInput (te::AudioTrack& track, bool arm);

    /** True if the input instance is assigned to the track (either direction). */
    static bool isInputAssignedToTrack (te::InputDeviceInstance& instance, te::AudioTrack& track);

    /** Assigns or removes a specific input instance on the track. */
    static void setInputAssignedToTrack (te::InputDeviceInstance& instance, te::AudioTrack& track, bool assign);

    static void enableAllInputs (te::Edit& edit);

    static void setupDefaultTracks (te::Edit& edit);

    static juce::String timeToTimecodeString (double seconds);

    static juce::String getPositionString (te::Edit& edit);

    static void prepareEngineForShutdown (te::Engine& engine, te::Edit* edit);
    static void releaseAudioDevices (te::Engine& engine);

    // Take / comp helpers (TE WaveCompManager / MidiCompManager)
    static bool hasMultipleTakes (const te::Clip& clip);
    static int getTakeCount (const te::Clip& clip, bool includeComps = true);
    static juce::String getTakeName (const te::Clip& clip, int index);
    static juce::StringArray getTakeDescriptions (const te::Clip& clip);
    static void setActiveTake (te::Clip& clip, int takeIndex);
    static bool isCurrentTakeComp (const te::Clip& clip);
    static void ensureCompTake (te::Clip& clip);
    static void flattenCompToMain (te::Clip& clip, bool deleteSourceFiles);
    static juce::File getTakeSourceFile (te::Clip& clip, int takeIndex);
    static te::CompManager* getCompManager (te::Clip& clip);

    static bool isTakeLanesExpanded (EditViewState& editViewState, const te::Clip& clip);
    static void setTakeLanesExpanded (EditViewState& editViewState, te::Clip* clip);
    static void toggleTakeLanesExpanded (EditViewState& editViewState, te::Clip& clip);
    static int getTakeLaneExtraHeight (EditViewState& editViewState, const te::Track& track);

    static bool isCreateTakesOnLoopEnabled (te::Edit& edit);
    static void setCreateTakesOnLoopEnabled (te::Edit& edit, bool enabled);

    /** Union selection with all inner/outer clip-group peers. */
    static juce::Array<te::Clip*> expandWithGroupedPeers (const juce::Array<te::Clip*>& clips);

    static te::WaveAudioClip* insertWaveClipFromFile (te::ClipTrack& track, const juce::File& file,
                                                      te::TimePosition start, const juce::String& name);

    /** Bounce selected clips in place — one WAV clip per affected track.
        Returns newly created clips; sets errorMessage on failure. */
    static juce::Array<te::Clip*> consolidateClips (te::Edit& edit, te::SelectionManager& selection,
                                                    juce::String* errorMessage = nullptr);

    /** Render the track device chain to a timeline audio clip and remove user plugins. */
    static te::WaveAudioClip* flattenTrackToAudioClip (te::AudioTrack& track, te::TimeRange range,
                                                       bool deleteCoveredMidiClips = true,
                                                       juce::String* errorMessage = nullptr);

    /** Selected clips on track, else transport loop, else full edit length. */
    static te::TimeRange resolveProductionRange (te::Edit& edit, te::ClipTrack& track,
                                                 te::SelectionManager& selection);

    /** Apply a groove template to all notes in selected MIDI clips. Returns note count changed. */
    static int applyGrooveToSelection (te::Edit& edit, te::SelectionManager& selection,
                                       const GrooveTemplate& groove, juce::String* errorMessage = nullptr);

    static juce::String makeSessionSlotId (te::EditItemID trackId, int sceneIndex);
    static bool isSessionClip (const te::Clip& clip);
    static juce::String getSessionSlotId (const te::Clip& clip);
    static void setSessionSlotId (te::Clip& clip, const juce::String& slotId);
    static void clearSessionClipTag (te::Clip& clip);
    static te::TimePosition sessionClipParkingPosition (const te::Edit& edit);
    static void parkSessionClip (te::Clip& clip);
    static void activateSessionClipAtStart (te::Clip& clip);
    static void enableSessionClipLoop (te::Clip& clip);
    static te::Clip* findClipById (te::Edit& edit, te::EditItemID clipId);

    static int getClipScaleRoot (const te::Clip& clip);
    static void setClipScaleRoot (te::Clip& clip, int root);
    static ScaleMode getClipScaleMode (const te::Clip& clip);
    static void setClipScaleMode (te::Clip& clip, ScaleMode mode);
    static bool getClipScaleLock (const te::Clip& clip);
    static void setClipScaleLock (te::Clip& clip, bool locked);

    static int getNoteProbability (juce::ValueTree noteState);
    static void setNoteProbability (juce::ValueTree noteState, int probability, juce::UndoManager* um);
    static int getNoteIteration (juce::ValueTree noteState);
    static void setNoteIteration (juce::ValueTree noteState, int iteration, juce::UndoManager* um);

#if JUCE_DEBUG
    static void createStressTestTracks (te::Edit& edit, int trackCount, int sceneCount);
#endif
};

} // namespace skeletonhive
