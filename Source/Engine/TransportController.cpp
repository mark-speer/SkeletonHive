#include "TransportController.h"
#include "TracktionCommon.h"

namespace arrange
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

void TransportController::setPunchRange (te::TimeRange range)
{
    edit.getTransport().setLoopRange (range);
}

void TransportController::enablePunchIn (bool enabled)
{
    punchInEnabled = enabled;
}

bool TransportController::isPunchInEnabled() const
{
    return punchInEnabled;
}

void TransportController::setTempo (double bpm)
{
    if (auto* tempo = edit.tempoSequence.getTempo (0))
        tempo->setBpm (bpm);
}

double TransportController::getTempo() const
{
    return edit.tempoSequence.getTempo (0)->getBpm();
}

void TransportController::setTimeSignature (int numerator, int denominator)
{
    if (auto* tempo = edit.tempoSequence.getTempo (0))
        tempo->getMatchingTimeSig().setStringTimeSig (juce::String (numerator) + "/" + juce::String (denominator));
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

} // namespace arrange
