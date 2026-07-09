#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

enum class TrackMonitorMode
{
    off,
    autoMode,
    in
};

/** Per-track input monitoring (OFF / AUTO / IN) mapped to Tracktion InputDevice::MonitorMode. */
struct TrackMonitorRouting
{
    static const juce::Identifier monitorModeProperty;

    static TrackMonitorMode getMonitorMode (const te::Track& track);
    static void setMonitorMode (te::Track& track, TrackMonitorMode mode, juce::UndoManager* um = nullptr);

    static juce::String monitorModeDisplayName (TrackMonitorMode mode);
    static juce::String monitorModeTooltip (TrackMonitorMode mode);

    /** Applies the track's stored mode to its assigned input device(s). */
    static void applyMonitorModeForTrack (te::AudioTrack& track);

    /** Reconciles MonitorMode on devices shared by multiple tracks (IN > AUTO > OFF). */
    static juce::StringArray reconcileSharedInputDevices (te::Edit& edit);

    /** Sets default AUTO mode on tracks missing the property (backward compatibility). */
    static void ensureTrackMonitorDefaults (te::Edit& edit);

    static bool shouldShowMonitorControl (const te::Track& track);

private:
    static te::InputDevice::MonitorMode toEngineMode (TrackMonitorMode mode);
    static int modePriority (TrackMonitorMode mode);
    static TrackMonitorMode highestPriorityMode (const juce::Array<TrackMonitorMode>& modes);
};

} // namespace skeletonhive
