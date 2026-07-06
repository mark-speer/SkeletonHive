#include "SessionArrangementBridge.h"
#include "EngineHelpers.h"
#include "SessionManager.h"
#include "TransportController.h"
#include "UI/Arrangement/TimelineGrid.h"

namespace skeletonhive
{

namespace
{
te::TimeDuration scaleDuration (te::TimeDuration length, int count)
{
    return te::TimeDuration::fromSeconds (length.inSeconds() * (double) count);
}
} // namespace

SessionArrangementBridge::SessionArrangementBridge (te::Edit& e, EditViewState& viewState,
                                                    SessionManager& session, TransportController& transport)
    : edit (e),
      editViewState (viewState),
      sessionManager (session),
      transportController (transport)
{
    lastTransportPos = transportController.getPosition();
    startTimerHz (30);
}

SessionArrangementBridge::~SessionArrangementBridge()
{
    stopTimer();
}

void SessionArrangementBridge::setRecordToArrangementEnabled (bool enabled)
{
    if (editViewState.isRecordToArrangementEnabled() == enabled)
        return;

    if (! enabled)
        finalizeAllCaptures();

    editViewState.setRecordToArrangementEnabled (enabled);
    sendChangeMessage();
}

bool SessionArrangementBridge::isRecordToArrangementEnabled() const
{
    return editViewState.isRecordToArrangementEnabled();
}

void SessionArrangementBridge::syncWritePositionFromTransport()
{
    editViewState.setArrangementWritePosition (transportController.getPosition());
}

te::TimePosition SessionArrangementBridge::getArrangementWritePosition() const
{
    return editViewState.getArrangementWritePosition();
}

te::TimePosition SessionArrangementBridge::getSnappedWritePosition() const
{
    return TimelineGrid::snapTime (edit, editViewState, editViewState.getArrangementWritePosition());
}

te::TimeDuration SessionArrangementBridge::getSessionClipLoopLength (const te::Clip& sessionClip) const
{
    if (auto* audio = dynamic_cast<const te::AudioClipBase*> (&sessionClip))
    {
        if (audio->isLooping())
        {
            const auto loopBeats = audio->getLoopLengthBeats();
            const auto startTime = edit.tempoSequence.toTime (te::BeatPosition());
            const auto endTime = edit.tempoSequence.toTime (te::BeatPosition() + loopBeats);
            return endTime - startTime;
        }
    }

    return sessionClip.getPosition().getLength();
}

te::Clip* SessionArrangementBridge::findArrangementClip (te::EditItemID clipId) const
{
    return EngineHelpers::findClipById (edit, clipId);
}

te::Clip* SessionArrangementBridge::commitSessionClipToArrangement (te::Clip& sessionClip,
                                                                    te::TimePosition start,
                                                                    te::TimeDuration length)
{
    auto* copy = EngineHelpers::duplicateClip (sessionClip, false);
    if (copy == nullptr)
        return nullptr;

    EngineHelpers::clearSessionClipTag (*copy);

    if (auto* audio = dynamic_cast<te::AudioClipBase*> (copy))
        audio->disableLooping();

    copy->setStart (start, false, true);

    if (length > 0s)
        copy->setEnd (start + length, false);

    return copy;
}

void SessionArrangementBridge::advanceWritePositionTo (te::TimePosition end)
{
    if (end > editViewState.getArrangementWritePosition())
        editViewState.setArrangementWritePosition (end);
}

te::Clip* SessionArrangementBridge::duplicateLoopToArrangement (te::EditItemID trackId, int sceneIndex)
{
    auto* sessionClip = sessionManager.getSlotClip (trackId, sceneIndex);
    if (sessionClip == nullptr)
        return nullptr;

    const auto start = getSnappedWritePosition();
    const auto length = getSessionClipLoopLength (*sessionClip);

    if (auto* created = commitSessionClipToArrangement (*sessionClip, start, length))
    {
        advanceWritePositionTo (start + length);
        sendChangeMessage();
        return created;
    }

    return nullptr;
}

juce::Array<te::Clip*> SessionArrangementBridge::captureAndInsert()
{
    juce::Array<te::Clip*> created;
    const auto start = getSnappedWritePosition();
    te::TimeDuration maxLength = 0s;

    for (const auto& key : sessionManager.getPlayingSlots())
    {
        auto* sessionClip = sessionManager.getSlotClip (key.trackId, key.sceneIndex);
        if (sessionClip == nullptr)
            continue;

        const auto length = getSessionClipLoopLength (*sessionClip);

        if (auto* copy = commitSessionClipToArrangement (*sessionClip, start, length))
        {
            created.add (copy);
            maxLength = juce::jmax (maxLength, length);
        }
    }

    if (maxLength > 0s)
        advanceWritePositionTo (start + maxLength);

    if (created.size() > 0)
        sendChangeMessage();

    return created;
}

void SessionArrangementBridge::extendActiveCapture (ActiveCapture& capture)
{
    capture.loopsRecorded++;

    if (auto* clip = findArrangementClip (capture.arrangementClipId))
    {
        const auto totalLength = scaleDuration (capture.loopLength, capture.loopsRecorded);
        clip->setEnd (capture.captureStart + totalLength, false);
    }

    sendChangeMessage();
}

void SessionArrangementBridge::finalizeActiveCapture (ActiveCapture& capture)
{
    const auto totalLength = scaleDuration (capture.loopLength, capture.loopsRecorded);
    advanceWritePositionTo (capture.captureStart + totalLength);
}

void SessionArrangementBridge::finalizeAllCaptures()
{
    for (auto& capture : activeCaptures)
        finalizeActiveCapture (capture);

    activeCaptures.clear();
}

void SessionArrangementBridge::beginCaptureForSlot (SessionSlotKey key)
{
    auto* sessionClip = sessionManager.getSlotClip (key.trackId, key.sceneIndex);
    if (sessionClip == nullptr)
        return;

    for (const auto& existing : activeCaptures)
    {
        if (existing.key == key)
            return;
    }

    const auto start = getSnappedWritePosition();
    const auto loopLength = getSessionClipLoopLength (*sessionClip);

    if (auto* copy = commitSessionClipToArrangement (*sessionClip, start, loopLength))
    {
        ActiveCapture capture;
        capture.key = key;
        capture.arrangementClipId = copy->itemID;
        capture.captureStart = start;
        capture.loopLength = loopLength;
        activeCaptures.add (capture);
        sendChangeMessage();
    }
}

void SessionArrangementBridge::onSlotLaunched (SessionSlotKey key)
{
    if (! isRecordToArrangementEnabled())
        return;

    beginCaptureForSlot (key);
    lastTransportPos = transportController.getPosition();
}

void SessionArrangementBridge::onSlotStopped (SessionSlotKey key)
{
    for (int i = activeCaptures.size(); --i >= 0;)
    {
        if (activeCaptures.getReference (i).key == key)
        {
            finalizeActiveCapture (activeCaptures.getReference (i));
            activeCaptures.remove (i);
            sendChangeMessage();
            break;
        }
    }
}

void SessionArrangementBridge::handleLoopWrap()
{
    if (activeCaptures.isEmpty() || ! transportController.isPlaying())
        return;

    const auto currentPos = transportController.getPosition();
    const auto loopRange = transportController.getLoopRange();

    if (loopRange.getLength() <= 0s)
    {
        lastTransportPos = currentPos;
        return;
    }

    const auto loopStart = loopRange.getStart();
    const auto threshold = loopRange.getLength() * 0.5;

    if (lastTransportPos > loopStart + threshold && currentPos <= loopStart + threshold)
    {
        for (auto& capture : activeCaptures)
            extendActiveCapture (capture);
    }

    lastTransportPos = currentPos;
}

void SessionArrangementBridge::timerCallback()
{
    handleLoopWrap();
}

} // namespace skeletonhive
