#pragma once

#include "EngineHelpers.h"

namespace arrange
{

class TransportController
{
public:
    explicit TransportController (te::Edit& edit);

    void play();
    void stop();
    void record();
    void togglePlay();
    void toggleRecord();
    void returnToStart();

    void setLooping (bool shouldLoop);
    bool isLooping() const;
    void setLoopRange (te::TimeRange range);
    te::TimeRange getLoopRange() const;

    void setPunchRange (te::TimeRange range);
    void enablePunchIn (bool enabled);
    bool isPunchInEnabled() const;

    void setTempo (double bpm);
    double getTempo() const;
    void setTimeSignature (int numerator, int denominator);

    te::TimePosition getPosition() const;
    void setPosition (te::TimePosition pos);

    bool isPlaying() const;
    bool isRecording() const;

private:
    te::Edit& edit;
    bool punchInEnabled = false;
};

} // namespace arrange
