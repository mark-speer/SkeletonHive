#pragma once

#include "EngineHelpers.h"
#include "TransportController.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

class ClipLibraryManager;
class SessionArrangementBridge;

struct SessionSlotKey
{
    te::EditItemID trackId;
    int sceneIndex = 0;

    bool operator== (const SessionSlotKey& other) const noexcept
    {
        return trackId == other.trackId && sceneIndex == other.sceneIndex;
    }
};

/** Session clip slots, launch/stop, and quantized scheduling. */
class SessionManager : public juce::ChangeBroadcaster,
                       private juce::Timer,
                       private juce::ChangeListener
{
public:
    SessionManager (te::Edit& edit, EditViewState& viewState, TransportController& transport);
    ~SessionManager() override;

    static bool isSessionClip (const te::Clip& clip) { return EngineHelpers::isSessionClip (clip); }

    int getSceneCount() const { return sceneCount.get(); }
    int getSlotsPerTrack() const { return slotsPerTrack.get(); }

    void setSceneCount (int count);
    void setSlotsPerTrack (int count);

    LaunchQuantization getLaunchQuantization() const;
    void setLaunchQuantization (LaunchQuantization q);

    SceneLaunchMode getSceneLaunchMode() const;
    void setSceneLaunchMode (SceneLaunchMode mode);

    te::EditItemID getSlotClipId (te::EditItemID trackId, int sceneIndex) const;
    te::Clip* getSlotClip (te::EditItemID trackId, int sceneIndex) const;

    bool assignClipToSlot (te::Clip& clip, te::EditItemID trackId, int sceneIndex);
    void clearSlot (te::EditItemID trackId, int sceneIndex);
    bool duplicateSlotToScene (te::EditItemID trackId, int fromScene, int toScene);

    te::Clip* loadSampleIntoSlot (te::ClipTrack& track, int sceneIndex, const juce::File& file);
    te::Clip* loadPresetIntoSlot (te::ClipTrack& track, int sceneIndex, const juce::File& presetFile,
                                  ClipLibraryManager& clipLibrary);

    bool isSlotPlaying (te::EditItemID trackId, int sceneIndex) const;
    bool isSlotRecording (te::EditItemID trackId, int sceneIndex) const;

    void toggleSlot (te::EditItemID trackId, int sceneIndex);
    void launchSlot (te::EditItemID trackId, int sceneIndex);
    void stopSlot (te::EditItemID trackId, int sceneIndex);
    void launchScene (int sceneIndex);
    void stopAll();

    void setArrangementBridge (SessionArrangementBridge* bridge) { arrangementBridge = bridge; }

    juce::Array<SessionSlotKey> getPlayingSlots() const { return playingSlots; }

    FollowAction getSlotFollowAction (te::EditItemID trackId, int sceneIndex) const;
    void setSlotFollowAction (te::EditItemID trackId, int sceneIndex, FollowAction action);

    bool getSlotLegatoLaunch (te::EditItemID trackId, int sceneIndex) const;
    void setSlotLegatoLaunch (te::EditItemID trackId, int sceneIndex, bool enabled);

private:
    struct PendingLaunch
    {
        SessionSlotKey key;
        double targetBeat = 0.0;
    };

    struct SlotPhaseState
    {
        SessionSlotKey key;
        double lastPhaseBeats = 0.0;
    };

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::ValueTree findOrCreateSlotTree (te::EditItemID trackId, int sceneIndex) const;
    juce::ValueTree findSlotTree (te::EditItemID trackId, int sceneIndex) const;
    void removeClipForSlot (te::EditItemID trackId, int sceneIndex);
    te::ClipTrack* getClipTrack (te::EditItemID trackId) const;

    void queueLaunch (SessionSlotKey key);
    void executeLaunch (SessionSlotKey key);
    void processPendingLaunches();
    void processFollowActions();
    void dispatchFollowAction (SessionSlotKey key);
    int findNextLoadedScene (te::EditItemID trackId, int fromScene) const;
    int findPreviousLoadedScene (te::EditItemID trackId, int fromScene) const;
    int findRandomLoadedScene (te::EditItemID trackId, int excludeScene) const;
    double getSlotClipLoopLengthBeats (te::Clip& clip) const;
    void parkOtherClipsOnTrack (te::EditItemID trackId, const juce::String& exceptSlotId);
    void updateTransportLoopForPlayingClips();
    double getCurrentBeat() const;
    double getNextQuantizeBeat (double fromBeat) const;
    double getQuantizeIntervalBeats() const;
    SessionSlotKey makeKey (te::EditItemID trackId, int sceneIndex) const;

    te::Edit& edit;
    EditViewState& editViewState;
    TransportController& transportController;

    juce::CachedValue<int> sceneCount;
    juce::CachedValue<int> slotsPerTrack;
    juce::CachedValue<int> launchQuantization;
    juce::CachedValue<int> sceneLaunchMode;

    juce::Array<SessionSlotKey> playingSlots;
    juce::Array<PendingLaunch> pendingLaunches;
    juce::Array<SlotPhaseState> slotPhaseStates;
    SessionArrangementBridge* arrangementBridge = nullptr;
};

} // namespace skeletonhive
