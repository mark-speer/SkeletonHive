#include "TrackMonitorRouting.h"
#include "EngineHelpers.h"
#include <map>

namespace skeletonhive
{

const juce::Identifier TrackMonitorRouting::monitorModeProperty ("skeletonHiveMonitorMode");

namespace
{

TrackMonitorMode parseModeString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("off"))   return TrackMonitorMode::off;
    if (s.equalsIgnoreCase ("in"))    return TrackMonitorMode::in;
    return TrackMonitorMode::autoMode;
}

juce::String modeToString (TrackMonitorMode mode)
{
    switch (mode)
    {
        case TrackMonitorMode::off:      return "off";
        case TrackMonitorMode::in:       return "in";
        case TrackMonitorMode::autoMode:
        default:                         return "auto";
    }
}

juce::Array<te::AudioTrack*> getTracksUsingInputDevice (te::Edit& edit, te::InputDevice& device)
{
    juce::Array<te::AudioTrack*> tracks;

    for (auto* instance : edit.getAllInputDevices())
    {
        if (&instance->getInputDevice() != &device)
            continue;

        for (auto targetId : instance->getTargets())
        {
            if (auto* t = te::findAudioTrackForID (edit, targetId))
                if (! tracks.contains (t))
                    tracks.add (t);
        }
    }

    return tracks;
}

} // namespace

TrackMonitorMode TrackMonitorRouting::getMonitorMode (const te::Track& track)
{
    if (! track.state.hasProperty (monitorModeProperty))
        return TrackMonitorMode::autoMode;

    return parseModeString (track.state.getProperty (monitorModeProperty).toString());
}

void TrackMonitorRouting::setMonitorMode (te::Track& track, TrackMonitorMode mode, juce::UndoManager* um)
{
    track.state.setProperty (monitorModeProperty, modeToString (mode), um);

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (&track))
        reconcileSharedInputDevices (audioTrack->edit);
}

juce::String TrackMonitorRouting::monitorModeDisplayName (TrackMonitorMode mode)
{
    switch (mode)
    {
        case TrackMonitorMode::off:      return "OFF";
        case TrackMonitorMode::in:       return "IN";
        case TrackMonitorMode::autoMode:
        default:                         return "AUTO";
    }
}

juce::String TrackMonitorRouting::monitorModeTooltip (TrackMonitorMode mode)
{
    switch (mode)
    {
        case TrackMonitorMode::off:
            return "Monitor OFF: playback only, no live input";
        case TrackMonitorMode::in:
            return "Monitor IN: always hear live input";
        case TrackMonitorMode::autoMode:
        default:
            return "Monitor AUTO: hear input when armed, clips when not";
    }
}

te::InputDevice::MonitorMode TrackMonitorRouting::toEngineMode (TrackMonitorMode mode)
{
    switch (mode)
    {
        case TrackMonitorMode::off:      return te::InputDevice::MonitorMode::off;
        case TrackMonitorMode::in:       return te::InputDevice::MonitorMode::on;
        case TrackMonitorMode::autoMode:
        default:                         return te::InputDevice::MonitorMode::automatic;
    }
}

void TrackMonitorRouting::applyMonitorModeForTrack (te::AudioTrack& track)
{
    reconcileSharedInputDevices (track.edit);
}

juce::StringArray TrackMonitorRouting::reconcileSharedInputDevices (te::Edit& edit)
{
    if (edit.getTransport().isRecording())
        return {};

    juce::StringArray conflicts;
    std::map<te::InputDevice*, juce::Array<TrackMonitorMode>> deviceModes;

    for (auto* t : te::getAudioTracks (edit))
    {
        if (t->isFolderTrack() || EngineHelpers::isReturnTrack (*t))
            continue;

        const auto mode = getMonitorMode (*t);

        for (auto* instance : EngineHelpers::getInputInstancesForTrack (*t))
        {
            auto& device = instance->getInputDevice();

            if (device.isTrackDevice())
                continue;

            auto& modes = deviceModes [&device];

            if (! modes.contains (mode))
                modes.add (mode);
        }
    }

    for (auto& [device, modes] : deviceModes)
    {
        if (modes.size() > 1)
        {
            const auto tracks = getTracksUsingInputDevice (edit, *device);
            juce::String trackNames;

            for (auto* t : tracks)
                trackNames << (trackNames.isEmpty() ? "" : ", ") << t->getName();

            conflicts.add ("Input \"" + device->getAlias() + "\" shared by " + trackNames
                           + " with conflicting monitor modes");
        }

        const auto resolved = highestPriorityMode (modes);
        const auto engineMode = toEngineMode (resolved);

        if (device->getMonitorMode() != engineMode)
            device->setMonitorMode (engineMode);
    }

    return conflicts;
}

void TrackMonitorRouting::ensureTrackMonitorDefaults (te::Edit& edit)
{
    for (auto* t : te::getAllTracks (edit))
    {
        if (! t->state.hasProperty (monitorModeProperty))
            t->state.setProperty (monitorModeProperty, "auto", nullptr);
    }

    reconcileSharedInputDevices (edit);
}

bool TrackMonitorRouting::shouldShowMonitorControl (const te::Track& track)
{
    if (track.isFolderTrack() || EngineHelpers::isReturnTrack (track))
        return false;

    return dynamic_cast<const te::AudioTrack*> (&track) != nullptr;
}

int TrackMonitorRouting::modePriority (TrackMonitorMode mode)
{
    switch (mode)
    {
        case TrackMonitorMode::in:       return 3;
        case TrackMonitorMode::autoMode: return 2;
        case TrackMonitorMode::off:
        default:                         return 1;
    }
}

TrackMonitorMode TrackMonitorRouting::highestPriorityMode (const juce::Array<TrackMonitorMode>& modes)
{
    TrackMonitorMode best = TrackMonitorMode::off;

    for (auto m : modes)
        if (modePriority (m) > modePriority (best))
            best = m;

    return best;
}

} // namespace skeletonhive
