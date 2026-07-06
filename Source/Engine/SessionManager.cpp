#include "SessionManager.h"
#include "ClipLibraryManager.h"
#include "EngineHelpers.h"
#include "SessionArrangementBridge.h"
#include "SessionClipPlaybackResolver.h"
#include "UI/Arrangement/TimelineGrid.h"

namespace skeletonhive
{

namespace
{
double quantizeIntervalBeats (LaunchQuantization q, const te::Edit& edit, te::TimePosition atTime)
{
    const auto barBeats = TimelineGrid::beatsPerBar (edit, atTime);

    switch (q)
    {
        case LaunchQuantization::none: return 0.0;
        case LaunchQuantization::bar: return barBeats;
        case LaunchQuantization::halfBar: return barBeats * 0.5;
        case LaunchQuantization::beat: return 1.0;
        case LaunchQuantization::halfBeat: return 0.5;
        case LaunchQuantization::eighthBeat: return 0.125;
        default: break;
    }

    return barBeats;
}

double getSessionClipLoopLengthBeats (const te::Clip& clip)
{
    if (auto* audio = dynamic_cast<const te::AudioClipBase*> (&clip))
    {
        const auto loopRange = audio->getLoopRangeBeats();
        const double loopLen = loopRange.getLength().inBeats();
        if (loopLen > 0.0)
            return loopLen;
    }

    return clip.edit.tempoSequence.toBeats (clip.getPosition().time).getLength().inBeats();
}
} // namespace

SessionManager::SessionManager (te::Edit& e, EditViewState& viewState, TransportController& transport)
    : edit (e),
      editViewState (viewState),
      transportController (transport)
{
    auto& sessionState = viewState.sessionState;
    auto* um = &edit.getUndoManager();

    sceneCount.referTo (sessionState, IDs::sceneCount, um, 8);
    slotsPerTrack.referTo (sessionState, IDs::slotsPerTrack, um, 8);
    launchQuantization.referTo (sessionState, IDs::launchQuantization, um, (int) LaunchQuantization::bar);
    sceneLaunchMode.referTo (sessionState, IDs::sceneLaunchMode, um, (int) SceneLaunchMode::stopOthers);

    edit.getTransport().addChangeListener (this);
    startTimerHz (30);
}

SessionManager::~SessionManager()
{
    stopTimer();
    edit.getTransport().removeChangeListener (this);
}

void SessionManager::setSceneCount (int count)
{
    sceneCount = juce::jlimit (1, 64, count);
    sendChangeMessage();
}

void SessionManager::setSlotsPerTrack (int count)
{
    slotsPerTrack = juce::jlimit (1, 64, count);
    sendChangeMessage();
}

LaunchQuantization SessionManager::getLaunchQuantization() const
{
    return static_cast<LaunchQuantization> (juce::jlimit (0, (int) LaunchQuantization::eighthBeat,
                                                          launchQuantization.get()));
}

void SessionManager::setLaunchQuantization (LaunchQuantization q)
{
    launchQuantization = (int) q;
    sendChangeMessage();
}

SceneLaunchMode SessionManager::getSceneLaunchMode() const
{
    return static_cast<SceneLaunchMode> (juce::jlimit (0, 1, sceneLaunchMode.get()));
}

void SessionManager::setSceneLaunchMode (SceneLaunchMode mode)
{
    sceneLaunchMode = (int) mode;
    sendChangeMessage();
}

juce::ValueTree SessionManager::findSlotTree (te::EditItemID trackId, int sceneIndex) const
{
    for (int i = 0; i < editViewState.sessionState.getNumChildren(); ++i)
    {
        auto child = editViewState.sessionState.getChild (i);
        if (child.hasType (IDs::SLOT)
            && (juce::int64) child.getProperty (IDs::trackId) == (juce::int64) trackId.getRawID()
            && (int) child.getProperty (IDs::sceneIndex) == sceneIndex)
            return child;
    }

    return {};
}

juce::ValueTree SessionManager::findOrCreateSlotTree (te::EditItemID trackId, int sceneIndex) const
{
    if (auto existing = findSlotTree (trackId, sceneIndex); existing.isValid())
        return existing;

    juce::ValueTree slot (IDs::SLOT);
    slot.setProperty (IDs::trackId, (juce::int64) trackId.getRawID(), &edit.getUndoManager());
    slot.setProperty (IDs::sceneIndex, sceneIndex, &edit.getUndoManager());
    slot.setProperty (IDs::clipId, (juce::int64) 0, &edit.getUndoManager());
    slot.setProperty (IDs::followAction, (int) FollowAction::none, &edit.getUndoManager());
    slot.setProperty (IDs::legatoLaunch, false, &edit.getUndoManager());
    editViewState.sessionState.appendChild (slot, &edit.getUndoManager());
    return slot;
}

FollowAction SessionManager::getSlotFollowAction (te::EditItemID trackId, int sceneIndex) const
{
    const auto slot = findSlotTree (trackId, sceneIndex);
    if (! slot.isValid())
        return FollowAction::none;

    return static_cast<FollowAction> (juce::jlimit (0, (int) FollowAction::stop,
                                                    (int) slot.getProperty (IDs::followAction, (int) FollowAction::none)));
}

void SessionManager::setSlotFollowAction (te::EditItemID trackId, int sceneIndex, FollowAction action)
{
    auto slot = findOrCreateSlotTree (trackId, sceneIndex);
    slot.setProperty (IDs::followAction, (int) action, &edit.getUndoManager());
    sendChangeMessage();
}

bool SessionManager::getSlotLegatoLaunch (te::EditItemID trackId, int sceneIndex) const
{
    const auto slot = findSlotTree (trackId, sceneIndex);
    if (! slot.isValid())
        return false;

    return (bool) slot.getProperty (IDs::legatoLaunch, false);
}

void SessionManager::setSlotLegatoLaunch (te::EditItemID trackId, int sceneIndex, bool enabled)
{
    auto slot = findOrCreateSlotTree (trackId, sceneIndex);
    slot.setProperty (IDs::legatoLaunch, enabled, &edit.getUndoManager());
    sendChangeMessage();
}

te::EditItemID SessionManager::getSlotClipId (te::EditItemID trackId, int sceneIndex) const
{
    const auto slot = findSlotTree (trackId, sceneIndex);
    if (! slot.isValid())
        return {};

    const auto rawId = (juce::int64) slot.getProperty (IDs::clipId);
    if (rawId == 0)
        return {};

    return te::EditItemID::fromRawID ((juce::uint64) rawId);
}

te::Clip* SessionManager::getSlotClip (te::EditItemID trackId, int sceneIndex) const
{
    return EngineHelpers::findClipById (edit, getSlotClipId (trackId, sceneIndex));
}

te::ClipTrack* SessionManager::getClipTrack (te::EditItemID trackId) const
{
    for (auto track : te::getAllTracks (edit))
    {
        if (track->itemID == trackId)
            return dynamic_cast<te::ClipTrack*> (track);
    }

    return nullptr;
}

void SessionManager::removeClipForSlot (te::EditItemID trackId, int sceneIndex)
{
    if (auto* clip = getSlotClip (trackId, sceneIndex))
    {
        const auto key = makeKey (trackId, sceneIndex);
        playingSlots.removeAllInstancesOf (key);
        clip->removeFromParent();
    }

    if (auto slot = findSlotTree (trackId, sceneIndex); slot.isValid())
        slot.setProperty (IDs::clipId, (juce::int64) 0, &edit.getUndoManager());
}

bool SessionManager::assignClipToSlot (te::Clip& clip, te::EditItemID trackId, int sceneIndex)
{
    if (auto* track = getClipTrack (trackId))
    {
        if (clip.getClipTrack() != track)
        {
            if (! EngineHelpers::canMoveClipToTrack (clip, *track))
                return false;

            EngineHelpers::moveClipToTrack (clip, *track, 0s);
        }
    }

    removeClipForSlot (trackId, sceneIndex);

    const auto slotId = EngineHelpers::makeSessionSlotId (trackId, sceneIndex);
    EngineHelpers::setSessionSlotId (clip, slotId);
    EngineHelpers::parkSessionClip (clip);

    auto slot = findOrCreateSlotTree (trackId, sceneIndex);
    slot.setProperty (IDs::clipId, (juce::int64) clip.itemID.getRawID(), &edit.getUndoManager());
    sendChangeMessage();
    return true;
}

void SessionManager::clearSlot (te::EditItemID trackId, int sceneIndex)
{
    stopSlot (trackId, sceneIndex);
    removeClipForSlot (trackId, sceneIndex);
    sendChangeMessage();
}

bool SessionManager::duplicateSlotToScene (te::EditItemID trackId, int fromScene, int toScene)
{
    if (fromScene == toScene)
        return false;

    auto* sourceClip = getSlotClip (trackId, fromScene);
    if (sourceClip == nullptr)
        return false;

    if (auto* copy = EngineHelpers::duplicateClip (*sourceClip, false))
    {
        assignClipToSlot (*copy, trackId, toScene);
        return true;
    }

    return false;
}

te::Clip* SessionManager::loadSampleIntoSlot (te::ClipTrack& track, int sceneIndex, const juce::File& file)
{
    removeClipForSlot (track.itemID, sceneIndex);

    if (auto* clip = EngineHelpers::insertWaveClipFromFile (track, file, 0s, file.getFileNameWithoutExtension()))
    {
        assignClipToSlot (*clip, track.itemID, sceneIndex);
        return clip;
    }

    return nullptr;
}

te::Clip* SessionManager::loadPresetIntoSlot (te::ClipTrack& track, int sceneIndex, const juce::File& presetFile,
                                              ClipLibraryManager& clipLibrary)
{
    removeClipForSlot (track.itemID, sceneIndex);

    if (auto* clip = clipLibrary.instantiateClip (track, 0s, presetFile))
    {
        assignClipToSlot (*clip, track.itemID, sceneIndex);
        return clip;
    }

    return nullptr;
}

SessionSlotKey SessionManager::makeKey (te::EditItemID trackId, int sceneIndex) const
{
    return { trackId, sceneIndex };
}

bool SessionManager::isSlotPlaying (te::EditItemID trackId, int sceneIndex) const
{
    return playingSlots.contains (makeKey (trackId, sceneIndex));
}

bool SessionManager::isSlotRecording (te::EditItemID trackId, int sceneIndex) const
{
    return isSlotPlaying (trackId, sceneIndex) && transportController.isRecording();
}

double SessionManager::getCurrentBeat() const
{
    return edit.tempoSequence.toBeats (transportController.getPosition()).inBeats();
}

double SessionManager::getQuantizeIntervalBeats() const
{
    return quantizeIntervalBeats (getLaunchQuantization(), edit, transportController.getPosition());
}

double SessionManager::getNextQuantizeBeat (double fromBeat) const
{
    const auto interval = getQuantizeIntervalBeats();
    if (interval <= 0.0)
        return fromBeat;

    const double next = std::ceil (fromBeat / interval) * interval;
    return juce::approximatelyEqual (next, fromBeat) ? next + interval : next;
}

void SessionManager::parkOtherClipsOnTrack (te::EditItemID trackId, const juce::String& exceptSlotId)
{
    if (auto* track = getClipTrack (trackId))
    {
        for (auto* clip : track->getClips())
        {
            if (clip == nullptr || ! EngineHelpers::isSessionClip (*clip))
                continue;

            if (EngineHelpers::getSessionSlotId (*clip) == exceptSlotId)
                continue;

            EngineHelpers::parkSessionClip (*clip);
        }
    }
}

void SessionManager::updateTransportLoopForPlayingClips()
{
    te::TimePosition loopEnd = 0s;

    for (const auto& key : playingSlots)
    {
        if (auto* clip = getSlotClip (key.trackId, key.sceneIndex))
            loopEnd = juce::jmax (loopEnd, clip->getPosition().getEnd());
    }

    if (loopEnd > 0s)
    {
        transportController.setLoopRange ({ 0s, loopEnd });
        transportController.setLooping (true);
    }
}

void SessionManager::applySessionPlaybackForKey (SessionSlotKey key, int loopCycleIndex)
{
    auto* clip = getSlotClip (key.trackId, key.sceneIndex);
    if (clip == nullptr)
        return;

    if (auto* midi = dynamic_cast<te::MidiClip*> (clip))
        SessionClipPlaybackResolver::applyPlaybackMasks (*midi, loopCycleIndex, playbackRandom);
}

void SessionManager::restoreSessionPlaybackForKey (SessionSlotKey key)
{
    auto* clip = getSlotClip (key.trackId, key.sceneIndex);
    if (clip == nullptr)
        return;

    if (auto* midi = dynamic_cast<te::MidiClip*> (clip))
        SessionClipPlaybackResolver::restorePlaybackMasks (*midi);
}

void SessionManager::executeLaunch (SessionSlotKey key)
{
    auto* clip = getSlotClip (key.trackId, key.sceneIndex);
    if (clip == nullptr)
        return;

    if (isSlotPlaying (key.trackId, key.sceneIndex) && getSlotLegatoLaunch (key.trackId, key.sceneIndex))
    {
        playingSlots.addIfNotAlreadyThere (key);
        updateTransportLoopForPlayingClips();
        sendChangeMessage();
        return;
    }

    const auto slotId = EngineHelpers::makeSessionSlotId (key.trackId, key.sceneIndex);
    parkOtherClipsOnTrack (key.trackId, slotId);
    EngineHelpers::activateSessionClipAtStart (*clip);
    EngineHelpers::enableSessionClipLoop (*clip);

    playingSlots.addIfNotAlreadyThere (key);
    updateTransportLoopForPlayingClips();

    if (! transportController.isPlaying())
    {
        transportController.setPosition (0s);
        transportController.play();
    }

    if (arrangementBridge != nullptr)
        arrangementBridge->onSlotLaunched (key);

    applySessionPlaybackForKey (key, 0);

    for (int i = slotPhaseStates.size(); --i >= 0;)
    {
        if (slotPhaseStates.getReference (i).key == key)
            slotPhaseStates.remove (i);
    }

    sendChangeMessage();
}

void SessionManager::queueLaunch (SessionSlotKey key)
{
    if (getLaunchQuantization() == LaunchQuantization::none)
    {
        executeLaunch (key);
        return;
    }

    PendingLaunch pending;
    pending.key = key;
    pending.targetBeat = getNextQuantizeBeat (getCurrentBeat());

    for (int i = pendingLaunches.size(); --i >= 0;)
    {
        if (pendingLaunches.getReference (i).key == key)
            pendingLaunches.remove (i);
    }

    pendingLaunches.add (pending);
}

void SessionManager::launchSlot (te::EditItemID trackId, int sceneIndex)
{
    if (getSlotClip (trackId, sceneIndex) == nullptr)
        return;

    queueLaunch (makeKey (trackId, sceneIndex));
}

void SessionManager::stopSlot (te::EditItemID trackId, int sceneIndex)
{
    const auto key = makeKey (trackId, sceneIndex);
    playingSlots.removeAllInstancesOf (key);

    for (int i = pendingLaunches.size(); --i >= 0;)
    {
        if (pendingLaunches.getReference (i).key == key)
            pendingLaunches.remove (i);
    }

    if (auto* clip = getSlotClip (trackId, sceneIndex))
    {
        restoreSessionPlaybackForKey (key);
        EngineHelpers::parkSessionClip (*clip);
    }

    for (int i = slotPhaseStates.size(); --i >= 0;)
    {
        if (slotPhaseStates.getReference (i).key == key)
            slotPhaseStates.remove (i);
    }

    if (arrangementBridge != nullptr)
        arrangementBridge->onSlotStopped (key);

    if (playingSlots.isEmpty())
        transportController.stop();
    else
        updateTransportLoopForPlayingClips();

    sendChangeMessage();
}

double SessionManager::getSlotClipLoopLengthBeats (te::Clip& clip) const
{
    return getSessionClipLoopLengthBeats (clip);
}

int SessionManager::findNextLoadedScene (te::EditItemID trackId, int fromScene) const
{
    for (int s = fromScene + 1; s < getSceneCount(); ++s)
        if (getSlotClip (trackId, s) != nullptr)
            return s;

    return -1;
}

int SessionManager::findPreviousLoadedScene (te::EditItemID trackId, int fromScene) const
{
    for (int s = fromScene - 1; s >= 0; --s)
        if (getSlotClip (trackId, s) != nullptr)
            return s;

    return -1;
}

int SessionManager::findRandomLoadedScene (te::EditItemID trackId, int excludeScene) const
{
    juce::Array<int> candidates;

    for (int s = 0; s < getSceneCount(); ++s)
    {
        if (s != excludeScene && getSlotClip (trackId, s) != nullptr)
            candidates.add (s);
    }

    if (candidates.isEmpty())
        return -1;

    return candidates[juce::Random::getSystemRandom().nextInt (candidates.size())];
}

void SessionManager::dispatchFollowAction (SessionSlotKey key)
{
    switch (getSlotFollowAction (key.trackId, key.sceneIndex))
    {
        case FollowAction::none:
            return;

        case FollowAction::stop:
            stopSlot (key.trackId, key.sceneIndex);
            return;

        case FollowAction::playNext:
        {
            const int nextScene = findNextLoadedScene (key.trackId, key.sceneIndex);
            stopSlot (key.trackId, key.sceneIndex);

            if (nextScene >= 0)
                queueLaunch ({ key.trackId, nextScene });

            return;
        }

        case FollowAction::playPrevious:
        {
            const int prevScene = findPreviousLoadedScene (key.trackId, key.sceneIndex);
            stopSlot (key.trackId, key.sceneIndex);

            if (prevScene >= 0)
                queueLaunch ({ key.trackId, prevScene });

            return;
        }

        case FollowAction::playRandom:
        {
            const int randomScene = findRandomLoadedScene (key.trackId, key.sceneIndex);
            stopSlot (key.trackId, key.sceneIndex);

            if (randomScene >= 0)
                queueLaunch ({ key.trackId, randomScene });

            return;
        }

        default:
            break;
    }
}

void SessionManager::processFollowActions()
{
    if (! transportController.isPlaying() || playingSlots.isEmpty())
        return;

    const double currentBeat = getCurrentBeat();

    for (const auto& key : playingSlots)
    {
        auto* clip = getSlotClip (key.trackId, key.sceneIndex);
        if (clip == nullptr)
            continue;

        const auto action = getSlotFollowAction (key.trackId, key.sceneIndex);
        if (action == FollowAction::none)
            continue;

        const double loopLen = getSlotClipLoopLengthBeats (*clip);
        if (loopLen <= 0.0)
            continue;

        const double phase = std::fmod (currentBeat, loopLen);
        const double highThreshold = loopLen * 0.75;
        const double lowThreshold = loopLen * 0.25;

        double lastPhase = 0.0;
        bool found = false;

        for (auto& state : slotPhaseStates)
        {
            if (state.key == key)
            {
                lastPhase = state.lastPhaseBeats;
                state.lastPhaseBeats = phase;
                found = true;
                break;
            }
        }

        if (! found)
        {
            SlotPhaseState state;
            state.key = key;
            state.lastPhaseBeats = phase;
            slotPhaseStates.add (state);
            continue;
        }

        if (lastPhase > highThreshold && phase <= lowThreshold)
        {
            int loopCycleIndex = 1;

            for (auto& state : slotPhaseStates)
            {
                if (state.key == key)
                {
                    state.loopCycleIndex += 1;
                    loopCycleIndex = state.loopCycleIndex;
                    break;
                }
            }

            applySessionPlaybackForKey (key, loopCycleIndex);
            dispatchFollowAction (key);
        }
    }
}

void SessionManager::processPendingLaunches()
{
    if (pendingLaunches.isEmpty())
        return;

    const double currentBeat = getCurrentBeat();
    juce::Array<SessionSlotKey> ready;

    for (const auto& pending : pendingLaunches)
    {
        if (currentBeat + 0.001 >= pending.targetBeat)
            ready.addIfNotAlreadyThere (pending.key);
    }

    for (const auto& key : ready)
    {
        for (int i = pendingLaunches.size(); --i >= 0;)
        {
            if (pendingLaunches.getReference (i).key == key)
                pendingLaunches.remove (i);
        }

        executeLaunch (key);
    }
}

void SessionManager::toggleSlot (te::EditItemID trackId, int sceneIndex)
{
    if (isSlotPlaying (trackId, sceneIndex))
        stopSlot (trackId, sceneIndex);
    else
        launchSlot (trackId, sceneIndex);
}

void SessionManager::launchScene (int sceneIndex)
{
    if (getSceneLaunchMode() == SceneLaunchMode::stopOthers)
        stopAll();

    juce::Array<SessionSlotKey> toLaunch;

    for (auto track : te::getAllTracks (edit))
    {
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track))
        {
            if (getSlotClip (clipTrack->itemID, sceneIndex) != nullptr)
                toLaunch.add (makeKey (clipTrack->itemID, sceneIndex));
        }
    }

    for (const auto& key : toLaunch)
        queueLaunch (key);
}

void SessionManager::stopAll()
{
    pendingLaunches.clear();

    const auto keys = playingSlots;

    for (const auto& key : keys)
    {
        restoreSessionPlaybackForKey (key);

        if (auto* clip = getSlotClip (key.trackId, key.sceneIndex))
            EngineHelpers::parkSessionClip (*clip);

        if (arrangementBridge != nullptr)
            arrangementBridge->onSlotStopped (key);
    }

    playingSlots.clear();
    slotPhaseStates.clear();
    transportController.stop();
    sendChangeMessage();
}

void SessionManager::timerCallback()
{
    processPendingLaunches();
    processFollowActions();
}

void SessionManager::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! transportController.isPlaying() && ! transportController.isRecording())
    {
        if (playingSlots.isEmpty())
            return;

        const auto keys = playingSlots;

        for (const auto& key : keys)
        {
            restoreSessionPlaybackForKey (key);

            if (auto* clip = getSlotClip (key.trackId, key.sceneIndex))
                EngineHelpers::parkSessionClip (*clip);

            if (arrangementBridge != nullptr)
                arrangementBridge->onSlotStopped (key);
        }

        playingSlots.clear();
        pendingLaunches.clear();
        slotPhaseStates.clear();
        sendChangeMessage();
    }
}

} // namespace skeletonhive
