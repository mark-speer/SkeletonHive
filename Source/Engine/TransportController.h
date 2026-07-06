#pragma once

#include "EngineHelpers.h"

namespace skeletonhive
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

    // TE derives the punch range from the loop in/out markers, so there is no
    // separate punch range: enabling punch records only inside the loop brace.
    void enablePunchIn (bool enabled);
    bool isPunchInEnabled() const;

    void setClickEnabled (bool enabled);
    bool isClickEnabled() const;
    void setClickVolume (float gain);
    float getClickVolume() const;
    void setClickRecordingOnly (bool onlyRecording);
    bool isClickRecordingOnly() const;
    void setCountInMode (te::Edit::CountIn mode);
    te::Edit::CountIn getCountInMode() const;

    // Tempo/time-sig edits apply to the setting in effect at the playhead, so
    // the transport controls stay usable in edits with tempo changes.
    void setTempo (double bpm);
    double getTempo() const;
    void setTimeSignature (int numerator, int denominator);
    juce::String getTimeSignatureString() const;

    te::TimePosition getPosition() const;
    void setPosition (te::TimePosition pos);

    bool isPlaying() const;
    bool isRecording() const;

private:
    te::Edit& edit;
};

} // namespace skeletonhive
