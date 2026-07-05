#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class LevelMeter;
class PlayheadOverlay;

/** Single 30 Hz timer driving playhead position and level meter repaints. */
class UiTelemetryHub : private juce::Timer
{
public:
    UiTelemetryHub();
    ~UiTelemetryHub() override;

    void registerPlayhead (PlayheadOverlay* playhead);
    void unregisterPlayhead (PlayheadOverlay* playhead);

    void registerMeter (LevelMeter* meter);
    void unregisterMeter (LevelMeter* meter);

private:
    void timerCallback() override;

    juce::Array<PlayheadOverlay*> playheads;
    juce::Array<LevelMeter*> meters;
};

} // namespace arrange
