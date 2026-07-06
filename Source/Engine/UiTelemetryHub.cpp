#include "UiTelemetryHub.h"
#include "UI/Mixer/ChannelStrip.h"
#include "UI/Arrangement/TrackComponents.h"

namespace skeletonhive
{

UiTelemetryHub::UiTelemetryHub()
{
    startTimerHz (30);
}

UiTelemetryHub::~UiTelemetryHub()
{
    stopTimer();
}

void UiTelemetryHub::registerPlayhead (PlayheadOverlay* playhead)
{
    if (playhead != nullptr && ! playheads.contains (playhead))
        playheads.add (playhead);
}

void UiTelemetryHub::unregisterPlayhead (PlayheadOverlay* playhead)
{
    playheads.removeAllInstancesOf (playhead);
}

void UiTelemetryHub::registerMeter (LevelMeter* meter)
{
    if (meter != nullptr && ! meters.contains (meter))
        meters.add (meter);
}

void UiTelemetryHub::unregisterMeter (LevelMeter* meter)
{
    meters.removeAllInstancesOf (meter);
}

void UiTelemetryHub::timerCallback()
{
    for (auto* playhead : playheads)
        if (playhead != nullptr)
            playhead->updateFromTransport();

    for (auto* meter : meters)
        if (meter != nullptr)
            meter->updateFromMeasurer();
}

} // namespace skeletonhive
