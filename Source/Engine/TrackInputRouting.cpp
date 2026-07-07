#include "TrackInputRouting.h"
#include "EngineHelpers.h"
#include "TrackPluginChainModel.h"

namespace skeletonhive
{

namespace
{

bool isAudioDeviceType (te::InputDevice::DeviceType type)
{
    return type == te::InputDevice::waveDevice
        || type == te::InputDevice::trackWaveDevice;
}

bool isMidiDeviceType (te::InputDevice::DeviceType type)
{
    return type == te::InputDevice::physicalMidiDevice
        || type == te::InputDevice::virtualMidiDevice
        || type == te::InputDevice::trackMidiDevice;
}

bool matchesKind (te::InputDevice::DeviceType type, TrackInputKind kind)
{
    return kind == TrackInputKind::audio ? isAudioDeviceType (type) : isMidiDeviceType (type);
}

bool trackHasWaveClips (const te::Track& track)
{
    if (auto* clipTrack = dynamic_cast<const te::ClipTrack*> (&track))
        for (auto* c : clipTrack->getClips())
            if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr)
                return true;

    return false;
}

bool trackHasInstrument (const te::AudioTrack& track)
{
    TrackPluginChainModel model (const_cast<te::AudioTrack&> (track));

    for (auto* plugin : model.getUserChainPlugins())
        if (plugin != nullptr && EngineHelpers::isInstrumentPlugin (*plugin))
            return true;

    return false;
}

bool isMidiCapableTrack (const te::AudioTrack& track)
{
    return EngineHelpers::isMidiTrack (track)
        || trackHasInstrument (track)
        || (EngineHelpers::canHostMidiClips (track) && ! trackHasWaveClips (track));
}

juce::String formatTrackSourceName (int oneBasedIndex, const juce::String& trackName)
{
    return juce::String (oneBasedIndex) + ". " + trackName;
}

te::AudioTrack* findTrackForTrackDevice (te::Edit& edit, const te::InputDevice& device, TrackInputKind kind)
{
    for (auto* t : te::getAudioTracks (edit))
    {
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
        {
            if (kind == TrackInputKind::audio && &at->getWaveInputDevice() == &device)
                return at;

            if (kind == TrackInputKind::midi && &at->getMidiInputDevice() == &device)
                return at;
        }
    }

    return nullptr;
}

te::InputDeviceInstance* getTrackDeviceInstance (te::Edit& edit, te::AudioTrack& sourceTrack, TrackInputKind kind)
{
    edit.getTransport().ensureContextAllocated();

    auto& device = kind == TrackInputKind::audio
                       ? static_cast<te::InputDevice&> (sourceTrack.getWaveInputDevice())
                       : static_cast<te::InputDevice&> (sourceTrack.getMidiInputDevice());

    edit.getEditInputDevices().getInstanceStateForInputDevice (device);

    if (auto* epc = edit.getCurrentPlaybackContext())
        return epc->getInputFor (&device);

    return nullptr;
}

void clearInputsOfKind (te::AudioTrack& dest, TrackInputKind kind)
{
    auto& edit = dest.edit;
    auto& um = edit.getUndoManager();

    for (auto* instance : edit.getAllInputDevices())
    {
        if (! matchesKind (instance->getInputDevice().getDeviceType(), kind))
            continue;

        if (te::isOnTargetTrack (*instance, dest, 0))
            [[maybe_unused]] const auto result = instance->removeTarget (dest.itemID, &um);
    }
}

juce::Array<te::AudioTrack*> getAudioTrackSourceCandidates (te::Edit& edit, te::AudioTrack& dest)
{
    juce::Array<te::AudioTrack*> candidates;

    for (auto* t : te::getAudioTracks (edit))
    {
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
        {
            if (at->itemID == dest.itemID)
                continue;

            if (at->isFolderTrack() || EngineHelpers::isReturnTrack (*at))
                continue;

            candidates.add (at);
        }
    }

    return candidates;
}

juce::Array<te::AudioTrack*> getMidiTrackSourceCandidates (te::Edit& edit, te::AudioTrack& dest)
{
    juce::Array<te::AudioTrack*> candidates;

    for (auto* at : getAudioTrackSourceCandidates (edit, dest))
        if (EngineHelpers::isMidiKindTrack (*at))
            candidates.add (at);

    return candidates;
}

} // namespace

bool TrackInputOption::operator== (const TrackInputOption& other) const noexcept
{
    if (type != other.type)
        return false;

    if (type == Type::none)
        return true;

    if (type == Type::trackOutput)
        return trackId == other.trackId;

    return device == other.device;
}

juce::Array<TrackInputOption> TrackInputRouting::getSourceOptions (te::Edit& edit, te::AudioTrack& dest,
                                                                   TrackInputKind kind)
{
    juce::Array<TrackInputOption> options;

    {
        TrackInputOption none;
        none.type = TrackInputOption::Type::none;
        none.displayName = "None";
        options.add (none);
    }

    for (auto* instance : edit.getAllInputDevices())
    {
        const auto deviceType = instance->getInputDevice().getDeviceType();

        if (deviceType != te::InputDevice::waveDevice
            && deviceType != te::InputDevice::physicalMidiDevice
            && deviceType != te::InputDevice::virtualMidiDevice)
            continue;

        if (! matchesKind (deviceType, kind))
            continue;

        TrackInputOption opt;
        opt.type = TrackInputOption::Type::externalDevice;
        opt.device = &instance->getInputDevice();
        opt.displayName = instance->getInputDevice().getName();
        options.add (opt);
    }

    const auto trackCandidates = kind == TrackInputKind::audio
                                     ? getAudioTrackSourceCandidates (edit, dest)
                                     : getMidiTrackSourceCandidates (edit, dest);

    for (int i = 0; i < trackCandidates.size(); ++i)
    {
        TrackInputOption opt;
        opt.type = TrackInputOption::Type::trackOutput;
        opt.trackId = trackCandidates[i]->itemID;
        opt.displayName = formatTrackSourceName (i + 1, trackCandidates[i]->getName());
        options.add (opt);
    }

    return options;
}

TrackInputOption TrackInputRouting::getActiveSource (te::AudioTrack& dest, TrackInputKind kind)
{
    auto& edit = dest.edit;

    for (auto* instance : edit.getAllInputDevices())
    {
        if (! te::isOnTargetTrack (*instance, dest, 0))
            continue;

        const auto deviceType = instance->getInputDevice().getDeviceType();

        if (! matchesKind (deviceType, kind))
            continue;

        TrackInputOption opt;

        if (deviceType == te::InputDevice::trackWaveDevice || deviceType == te::InputDevice::trackMidiDevice)
        {
            if (auto* sourceTrack = findTrackForTrackDevice (edit, instance->getInputDevice(), kind))
            {
                opt.type = TrackInputOption::Type::trackOutput;
                opt.trackId = sourceTrack->itemID;

                const auto candidates = kind == TrackInputKind::audio
                                            ? getAudioTrackSourceCandidates (edit, dest)
                                            : getMidiTrackSourceCandidates (edit, dest);

                for (int i = 0; i < candidates.size(); ++i)
                {
                    if (candidates[i]->itemID == opt.trackId)
                    {
                        opt.displayName = formatTrackSourceName (i + 1, sourceTrack->getName());
                        return opt;
                    }
                }

                opt.displayName = sourceTrack->getName();
                return opt;
            }
        }
        else
        {
            opt.type = TrackInputOption::Type::externalDevice;
            opt.device = &instance->getInputDevice();
            opt.displayName = instance->getInputDevice().getName();
            return opt;
        }
    }

    TrackInputOption none;
    none.type = TrackInputOption::Type::none;
    none.displayName = "None";
    return none;
}

void TrackInputRouting::setActiveSource (te::AudioTrack& dest, const TrackInputOption& option, TrackInputKind kind)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto& edit = dest.edit;
    auto& um = edit.getUndoManager();

    clearInputsOfKind (dest, kind);

    if (option.type == TrackInputOption::Type::none)
        return;

    if (option.type == TrackInputOption::Type::externalDevice)
    {
        edit.getTransport().ensureContextAllocated();

        for (auto* instance : edit.getAllInputDevices())
        {
            if (&instance->getInputDevice() != option.device)
                continue;

            if (! instance->setTarget (dest.itemID, false, &um, 0))
                [[maybe_unused]] const auto result = instance->setTarget (dest.itemID, true, &um, 0);

            return;
        }

        return;
    }

    if (option.type == TrackInputOption::Type::trackOutput)
    {
        if (auto* sourceTrack = te::findAudioTrackForID (edit, option.trackId))
        {
            const auto deviceType = kind == TrackInputKind::audio
                                        ? te::InputDevice::trackWaveDevice
                                        : te::InputDevice::trackMidiDevice;

            if (auto* instance = getTrackDeviceInstance (edit, *sourceTrack, kind))
            {
                if (! instance->setTarget (dest.itemID, false, &um, 0))
                    [[maybe_unused]] const auto result = instance->setTarget (dest.itemID, true, &um, 0);
            }
            else
            {
                [[maybe_unused]] const auto destAssignment = te::assignTrackAsInput (dest, *sourceTrack, deviceType);
            }
        }
    }
}

bool TrackInputRouting::shouldShowMidiSource (const te::Track& track)
{
    if (track.isFolderTrack() || EngineHelpers::isReturnTrack (track))
        return false;

    if (dynamic_cast<const te::AudioTrack*> (&track) == nullptr)
        return false;

    return EngineHelpers::isMidiKindTrack (track);
}

bool TrackInputRouting::shouldShowAudioSource (const te::Track& track)
{
    if (track.isFolderTrack() || EngineHelpers::isReturnTrack (track))
        return false;

    if (dynamic_cast<const te::AudioTrack*> (&track) == nullptr)
        return false;

    return EngineHelpers::isAudioKindTrack (track);
}

TrackInputOption TrackInputRouting::getFirstExternalOption (te::Edit& edit, TrackInputKind kind)
{
    for (auto* instance : edit.getAllInputDevices())
    {
        const auto deviceType = instance->getInputDevice().getDeviceType();

        if (deviceType != te::InputDevice::waveDevice
            && deviceType != te::InputDevice::physicalMidiDevice
            && deviceType != te::InputDevice::virtualMidiDevice)
            continue;

        if (! matchesKind (deviceType, kind))
            continue;

        TrackInputOption opt;
        opt.type = TrackInputOption::Type::externalDevice;
        opt.device = &instance->getInputDevice();
        opt.displayName = instance->getInputDevice().getName();
        return opt;
    }

    TrackInputOption none;
    none.type = TrackInputOption::Type::none;
    none.displayName = "None";
    return none;
}

void TrackInputRouting::assignFirstExternalSource (te::AudioTrack& dest, TrackInputKind kind)
{
    setActiveSource (dest, getFirstExternalOption (dest.edit, kind), kind);
}

} // namespace skeletonhive
