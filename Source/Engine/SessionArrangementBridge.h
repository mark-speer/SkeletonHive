#pragma once

#include "SessionManager.h"
#include "TransportController.h"
#include "UI/Arrangement/EditViewState.h"

namespace skeletonhive
{

/** Record Session → Arrangement, Capture & Insert, Duplicate loop. */
class SessionArrangementBridge : public juce::ChangeBroadcaster,
                                 private juce::Timer
{
public:
    SessionArrangementBridge (te::Edit& edit, EditViewState& viewState,
                              SessionManager& sessionManager, TransportController& transport);
    ~SessionArrangementBridge() override;

    void setRecordToArrangementEnabled (bool enabled);
    bool isRecordToArrangementEnabled() const;

    te::Clip* duplicateLoopToArrangement (te::EditItemID trackId, int sceneIndex);
    juce::Array<te::Clip*> captureAndInsert();

    void onSlotLaunched (SessionSlotKey key);
    void onSlotStopped (SessionSlotKey key);

    void syncWritePositionFromTransport();
    te::TimePosition getArrangementWritePosition() const;

private:
    struct ActiveCapture
    {
        SessionSlotKey key;
        te::EditItemID arrangementClipId;
        te::TimePosition captureStart;
        te::TimeDuration loopLength;
        int loopsRecorded = 1;
    };

    te::Clip* commitSessionClipToArrangement (te::Clip& sessionClip, te::TimePosition start,
                                              te::TimeDuration length);
    te::TimeDuration getSessionClipLoopLength (const te::Clip& sessionClip) const;
    te::TimePosition getSnappedWritePosition() const;
    void advanceWritePositionTo (te::TimePosition end);
    te::Clip* findArrangementClip (te::EditItemID clipId) const;
    void extendActiveCapture (ActiveCapture& capture);
    void finalizeActiveCapture (ActiveCapture& capture);
    void finalizeAllCaptures();
    void handleLoopWrap();
    void beginCaptureForSlot (SessionSlotKey key);

    void timerCallback() override;

    te::Edit& edit;
    EditViewState& editViewState;
    SessionManager& sessionManager;
    TransportController& transportController;

    juce::Array<ActiveCapture> activeCaptures;
    te::TimePosition lastTransportPos {};
};

} // namespace skeletonhive
