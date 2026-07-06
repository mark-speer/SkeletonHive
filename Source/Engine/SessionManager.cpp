#include "SessionManager.h"
#include "ClipLibraryManager.h"
#include "EngineHelpers.h"
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
    editViewState.sessionState.appendChild (slot, &edit.getUndoManager());
    return slot;
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

void SessionManager::executeLaunch (SessionSlotKey key)
{
    auto* clip = getSlotClip (key.trackId, key.sceneIndex);
    if (clip == nullptr)
        return;

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
        EngineHelpers::parkSessionClip (*clip);

    if (playingSlots.isEmpty())
        transportController.stop();
    else
        updateTransportLoopForPlayingClips();

    sendChangeMessage();
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

    for (const auto& key : playingSlots)
    {
        if (auto* clip = getSlotClip (key.trackId, key.sceneIndex))
            EngineHelpers::parkSessionClip (*clip);
    }

    playingSlots.clear();
    transportController.stop();
    sendChangeMessage();
}

void SessionManager::timerCallback()
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

void SessionManager::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (! transportController.isPlaying() && ! transportController.isRecording())
    {
        if (playingSlots.isEmpty())
            return;

        for (const auto& key : playingSlots)
        {
            if (auto* clip = getSlotClip (key.trackId, key.sceneIndex))
                EngineHelpers::parkSessionClip (*clip);
        }

        playingSlots.clear();
        pendingLaunches.clear();
        sendChangeMessage();
    }
}

} // namespace skeletonhive
