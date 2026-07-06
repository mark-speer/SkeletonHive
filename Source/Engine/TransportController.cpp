#include "TransportController.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

TransportController::TransportController (te::Edit& e)
    : edit (e)
{
}

void TransportController::play()
{
    edit.getTransport().play (false);
}

void TransportController::stop()
{
    edit.getTransport().stop (false, false);
}

void TransportController::record()
{
    edit.getTransport().record (false);
}

void TransportController::togglePlay()
{
    EngineHelpers::togglePlay (edit);
}

void TransportController::toggleRecord()
{
    EngineHelpers::toggleRecord (edit);
}

void TransportController::returnToStart()
{
    edit.getTransport().setPosition (0s);
}

void TransportController::setLooping (bool shouldLoop)
{
    edit.getTransport().looping = shouldLoop;
}

bool TransportController::isLooping() const
{
    return edit.getTransport().looping;
}

void TransportController::setLoopRange (te::TimeRange range)
{
    edit.getTransport().setLoopRange (range);
}

te::TimeRange TransportController::getLoopRange() const
{
    return edit.getTransport().getLoopRange();
}

void TransportController::enablePunchIn (bool enabled)
{
    edit.recordingPunchInOut = enabled;
}

bool TransportController::isPunchInEnabled() const
{
    return edit.recordingPunchInOut;
}

void TransportController::setClickEnabled (bool enabled)
{
    edit.clickTrackEnabled = enabled;
}

bool TransportController::isClickEnabled() const
{
    return edit.clickTrackEnabled;
}

void TransportController::setClickVolume (float gain)
{
    edit.setClickTrackVolume (gain);
}

float TransportController::getClickVolume() const
{
    return edit.getClickTrackVolume();
}

void TransportController::setClickRecordingOnly (bool onlyRecording)
{
    edit.clickTrackRecordingOnly = onlyRecording;
}

bool TransportController::isClickRecordingOnly() const
{
    return edit.clickTrackRecordingOnly;
}

void TransportController::setCountInMode (te::Edit::CountIn mode)
{
    edit.setCountInMode (mode);
}

te::Edit::CountIn TransportController::getCountInMode() const
{
    return edit.getCountInMode();
}

void TransportController::setTempo (double bpm)
{
    edit.tempoSequence.getTempoAt (getPosition()).setBpm (bpm);
}

double TransportController::getTempo() const
{
    return edit.tempoSequence.getTempoAt (getPosition()).getBpm();
}

void TransportController::setTimeSignature (int numerator, int denominator)
{
    edit.tempoSequence.getTimeSigAt (getPosition())
        .setStringTimeSig (juce::String (numerator) + "/" + juce::String (denominator));
}

juce::String TransportController::getTimeSignatureString() const
{
    return edit.tempoSequence.getTimeSigAt (getPosition()).getStringTimeSig();
}

te::TimePosition TransportController::getPosition() const
{
    return edit.getTransport().getPosition();
}

void TransportController::setPosition (te::TimePosition pos)
{
    edit.getTransport().setPosition (pos);
}

bool TransportController::isPlaying() const
{
    return edit.getTransport().isPlaying();
}

bool TransportController::isRecording() const
{
    return edit.getTransport().isRecording();
}

} // namespace skeletonhive
