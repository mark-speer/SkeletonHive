#include "EngineHelpers.h"
#include "TracktionCommon.h"
#include <algorithm>

namespace arrange
{

const juce::Identifier EngineHelpers::trackKindProperty ("arrangeTrackKind");
const juce::Identifier EngineHelpers::clipGroupProperty ("arrangeClipGroup");
const juce::Identifier EngineHelpers::clipGroupColourProperty ("arrangeClipGroupColour");

te::Clip* EngineHelpers::duplicateClip (te::Clip& clip, bool placeAfterOriginal)
{
    auto* track = clip.getClipTrack();
    if (track == nullptr)
        return nullptr;

    // Same pattern Tracktion uses internally when splitting clips
    auto newState = clip.state.createCopy();
    clip.edit.createNewItemID().writeID (newState, nullptr);
    te::assignNewIDsToAutomationCurveModifiers (clip.edit, newState);

    auto* newClip = track->insertClipWithState (newState);

    if (newClip != nullptr && placeAfterOriginal)
        newClip->setStart (clip.getPosition().getEnd(), false, true);

    return newClip;
}

juce::String EngineHelpers::getClipGroup (const te::Clip& clip)
{
    return clip.state.getProperty (clipGroupProperty).toString();
}

void EngineHelpers::setClipGroup (te::Clip& clip, const juce::String& groupId)
{
    if (groupId.isEmpty())
    {
        clip.state.removeProperty (clipGroupProperty, &clip.edit.getUndoManager());
        clip.state.removeProperty (clipGroupColourProperty, &clip.edit.getUndoManager());
    }
    else
    {
        clip.state.setProperty (clipGroupProperty, groupId, &clip.edit.getUndoManager());
    }
}

juce::Array<te::Clip*> EngineHelpers::getClipsInGroup (te::Edit& edit, const juce::String& groupId)
{
    juce::Array<te::Clip*> result;

    if (groupId.isEmpty())
        return result;

    for (auto track : te::getAllTracks (edit))
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track))
            for (auto* c : clipTrack->getClips())
                if (getClipGroup (*c) == groupId)
                    result.add (c);

    return result;
}

juce::Array<te::Clip*> EngineHelpers::getClipsStartingAfter (te::ClipTrack& track, te::TimePosition anchor)
{
    juce::Array<te::Clip*> result;

    for (auto* c : track.getClips())
        if (c->getPosition().getStart() > anchor)
            result.add (c);

    std::sort (result.begin(), result.end(), [] (const te::Clip* a, const te::Clip* b)
    {
        return a->getPosition().getStart() < b->getPosition().getStart();
    });

    return result;
}

juce::Colour EngineHelpers::colourForGroupId (const juce::String& groupId)
{
    static const juce::Colour palette[] =
    {
        juce::Colour (0xffffd166), juce::Colour (0xff06d6a0), juce::Colour (0xff118ab2),
        juce::Colour (0xffef476f), juce::Colour (0xffc77dff), juce::Colour (0xfff4a261),
    };

    if (groupId.isEmpty())
        return palette[0];

    const auto index = (juce::uint32) groupId.hashCode() % (juce::uint32) juce::numElementsInArray (palette);
    return palette[index];
}

juce::Colour EngineHelpers::getClipGroupColour (const te::Clip& clip)
{
    const auto stored = clip.state.getProperty (clipGroupColourProperty).toString();
    if (stored.isNotEmpty())
        return juce::Colour::fromString (stored);

    return colourForGroupId (getClipGroup (clip));
}

void EngineHelpers::setClipGroupColour (te::Clip& clip, juce::Colour colour)
{
    clip.state.setProperty (clipGroupColourProperty, colour.toString(), &clip.edit.getUndoManager());
}

te::AudioTrack* EngineHelpers::getOrCreateReturnTrack (te::Edit& edit, int busNumber)
{
    for (auto* track : te::getAudioTracks (edit))
        if (auto* ret = track->pluginList.findFirstPluginOfType<te::AuxReturnPlugin>())
            if (ret->busNumber.get() == busNumber)
                return track;

    auto track = edit.insertNewAudioTrack (te::TrackInsertPoint::getEndOfTracks (edit), nullptr);
    if (track == nullptr)
        return nullptr;

    track->setName ("Return " + juce::String::charToString ((juce::juce_wchar) ('A' + busNumber)));

    if (auto plugin = edit.getPluginCache().createNewPlugin (te::AuxReturnPlugin::xmlTypeName, {}))
    {
        track->pluginList.insertPlugin (plugin, 0, nullptr);
        if (auto* ret = dynamic_cast<te::AuxReturnPlugin*> (plugin.get()))
            ret->busNumber = busNumber;
    }

    return track.get();
}

te::AuxSendPlugin* EngineHelpers::getOrCreateAuxSend (te::AudioTrack& track, int busNumber)
{
    if (auto* existing = track.getAuxSendPlugin (busNumber))
        return existing;

    auto plugin = track.edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
    if (plugin == nullptr)
        return nullptr;

    // Post-fx / pre-fader: insert just before the volume plugin
    int insertIndex = track.pluginList.size();
    for (int i = 0; i < track.pluginList.size(); ++i)
    {
        if (dynamic_cast<te::VolumeAndPanPlugin*> (track.pluginList[i]) != nullptr)
        {
            insertIndex = i;
            break;
        }
    }

    track.pluginList.insertPlugin (plugin, insertIndex, nullptr);

    auto* send = dynamic_cast<te::AuxSendPlugin*> (plugin.get());
    if (send != nullptr)
        send->busNumber = busNumber;

    return send;
}

void EngineHelpers::setTrackKind (te::Track& track, TrackKind kind)
{
    track.state.setProperty (trackKindProperty, kind == TrackKind::midi ? "midi" : "audio", nullptr);
}

TrackKind EngineHelpers::getTrackKind (const te::Track& track)
{
    const auto stored = track.state.getProperty (trackKindProperty).toString();
    if (stored == "midi")
        return TrackKind::midi;
    if (stored == "audio")
        return TrackKind::audio;

    if (auto* clipTrack = dynamic_cast<const te::ClipTrack*> (&track))
    {
        bool hasMidi = false, hasAudio = false;
        for (auto* c : clipTrack->getClips())
        {
            if (dynamic_cast<te::MidiClip*> (c) != nullptr)
                hasMidi = true;
            else if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr)
                hasAudio = true;
        }
        if (hasMidi && ! hasAudio)
            return TrackKind::midi;
    }

    return TrackKind::audio;
}

te::Project::Ptr EngineHelpers::createTempProject (te::Engine& engine)
{
    auto file = engine.getTemporaryFileManager().getTempDirectory()
                    .getChildFile ("temp_project")
                    .withFileExtension (te::projectFileSuffix);
    te::ProjectManager::TempProject tempProject (engine.getProjectManager(), file, true);
    return tempProject.project;
}

void EngineHelpers::showAudioDeviceSettings (te::Engine& engine)
{
    juce::DialogWindow::LaunchOptions o;
    o.dialogTitle = "Audio Settings";
    o.dialogBackgroundColour = juce::LookAndFeel::getDefaultLookAndFeel()
                                   .findColour (juce::ResizableWindow::backgroundColourId);
    o.content.setOwned (new juce::AudioDeviceSelectorComponent (engine.getDeviceManager().deviceManager,
                                                                0, 512, 1, 512,
                                                                false, false, true, true));
    o.content->setSize (400, 600);
    o.launchAsync();
}

void EngineHelpers::browseForAudioFile (te::Engine& engine, std::function<void (const juce::File&)> callback)
{
    auto fc = std::make_shared<juce::FileChooser> ("Select an audio file...",
                                                    engine.getPropertyStorage().getDefaultLoadSaveDirectory ("importAudio"),
                                                    engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats());

    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                     [fc, &engine, cb = std::move (callback)] (const juce::FileChooser&)
                     {
                         const auto f = fc->getResult();
                         if (f.existsAsFile())
                         {
                             engine.getPropertyStorage().setDefaultLoadSaveDirectory ("importAudio", f.getParentDirectory());
                             cb (f);
                         }
                     });
}

te::AudioTrack* EngineHelpers::getOrInsertAudioTrackAt (te::Edit& edit, int index)
{
    edit.ensureNumberOfAudioTracks (index + 1);
    return te::getAudioTracks (edit)[index];
}

te::AudioTrack* EngineHelpers::getOrInsertAudioTrack (te::Edit& edit)
{
    auto* track = getOrInsertAudioTrackAt (edit, (int) te::getAudioTracks (edit).size());
    if (track != nullptr)
        setTrackKind (*track, TrackKind::audio);
    return track;
}

te::AudioTrack* EngineHelpers::getOrInsertTrackForMidi (te::Edit& edit, int index)
{
    auto* track = getOrInsertAudioTrackAt (edit, index);
    if (track != nullptr)
        setTrackKind (*track, TrackKind::midi);
    return track;
}

te::WaveAudioClip::Ptr EngineHelpers::loadAudioFileAsClip (te::Edit& edit, const juce::File& file, int trackIndex)
{
    if (auto* track = getOrInsertAudioTrackAt (edit, trackIndex))
    {
        te::AudioFile audioFile (edit.engine, file);
        if (audioFile.isValid())
        {
            const te::TimeRange range { 0s, te::TimeDuration::fromSeconds (audioFile.getLength()) };
            if (auto* clip = track->insertNewClip (te::TrackItem::Type::wave, file.getFileNameWithoutExtension(), range, nullptr))
                return dynamic_cast<te::WaveAudioClip*> (clip);
        }
    }
    return {};
}

te::MidiClip::Ptr EngineHelpers::createMidiClip (te::Edit& edit, int trackIndex,
                                                 te::TimeRange range, const juce::String& name)
{
    if (auto* track = getOrInsertTrackForMidi (edit, trackIndex))
    {
        if (auto* clip = track->insertNewClip (te::TrackItem::Type::midi, name, range, nullptr))
            return dynamic_cast<te::MidiClip*> (clip);
    }
    return {};
}

te::MidiClip::Ptr EngineHelpers::createMidiClipOnTrack (te::Track& track, te::TimeRange range,
                                                        const juce::String& name)
{
    if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (&track))
        if (auto* clip = clipTrack->insertNewClip (te::TrackItem::Type::midi, name, range, nullptr))
            return dynamic_cast<te::MidiClip*> (clip);

    return {};
}

void EngineHelpers::togglePlay (te::Edit& edit, bool returnToStart)
{
    auto& transport = edit.getTransport();
    if (transport.isPlaying())
        transport.stop (false, false);
    else if (returnToStart)
        transport.playFromStart (true);
    else
        transport.play (false);
}

void EngineHelpers::toggleRecord (te::Edit& edit)
{
    auto& transport = edit.getTransport();
    if (transport.isRecording())
        transport.stop (true, false);
    else
        transport.record (false);
}

void EngineHelpers::armTrack (te::AudioTrack& track, bool arm, int position)
{
    auto& edit = track.edit;
    for (auto* instance : edit.getAllInputDevices())
        if (te::isOnTargetTrack (*instance, track, position))
            instance->setRecordingEnabled (track.itemID, arm);
}

bool EngineHelpers::isTrackArmed (te::AudioTrack& track, int position)
{
    auto& edit = track.edit;
    for (auto* instance : edit.getAllInputDevices())
        if (te::isOnTargetTrack (*instance, track, position))
            return instance->isRecordingEnabled (track.itemID);
    return false;
}

void EngineHelpers::enableAllInputs (te::Edit& edit)
{
    auto& dm = edit.engine.getDeviceManager();

    for (auto& midiIn : dm.getMidiInDevices())
    {
        midiIn->setMonitorMode (te::InputDevice::MonitorMode::automatic);
        midiIn->setEnabled (true);
    }

    for (int i = 0; i < dm.getNumWaveInDevices(); ++i)
        if (auto* wip = dm.getWaveInDevice (i))
        {
            wip->setStereoPair (false);
            wip->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            wip->setEnabled (true);
        }

    edit.getTransport().ensureContextAllocated();
}

void EngineHelpers::setupDefaultTracks (te::Edit& edit)
{
    enableAllInputs (edit);
    if (auto* t = getOrInsertAudioTrackAt (edit, 0))
        setTrackKind (*t, TrackKind::audio);
    getOrInsertTrackForMidi (edit, 1);
    edit.getTransport().ensureContextAllocated();

    int audioTrackNum = 0;
    for (auto* instance : edit.getAllInputDevices())
    {
        if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
        {
            if (auto* t = getOrInsertAudioTrackAt (edit, audioTrackNum))
            {
                setTrackKind (*t, TrackKind::audio);
                [[maybe_unused]] const auto audioTargetResult = instance->setTarget (t->itemID, true, &edit.getUndoManager(), 0);
                ++audioTrackNum;
            }
        }
    }

    int midiTrackNum = 1;
    for (auto* instance : edit.getAllInputDevices())
    {
        if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice)
        {
            if (auto* t = getOrInsertTrackForMidi (edit, midiTrackNum))
            {
                [[maybe_unused]] const auto midiTargetResult = instance->setTarget (t->itemID, true, &edit.getUndoManager(), 0);
                ++midiTrackNum;
            }
        }
    }

    edit.getTransport().setLoopRange ({ 0s, te::TimeDuration::fromSeconds (30.0) });
}

juce::String EngineHelpers::timeToTimecodeString (double seconds)
{
    auto millisecs = juce::roundToInt (seconds * 1000.0);
    auto absMillisecs = std::abs (millisecs);
    return juce::String::formatted ("%02d:%02d:%02d.%03d",
                                    millisecs / 3600000,
                                    (absMillisecs / 60000) % 60,
                                    (absMillisecs / 1000) % 60,
                                    absMillisecs % 1000);
}

juce::String EngineHelpers::getPositionString (te::Edit& edit)
{
    return timeToTimecodeString (edit.getTransport().getPosition().inSeconds());
}

void EngineHelpers::prepareEngineForShutdown (te::Engine& engine, te::Edit* edit)
{
    te::TransportControl::stopAllTransports (engine, false, true);

    if (edit != nullptr)
    {
        for (auto* windowState : te::PluginWindowState::getAllWindows (*edit))
            if (windowState != nullptr)
                windowState->closeWindowExplicitly();
    }
}

void EngineHelpers::releaseAudioDevices (te::Engine& engine)
{
    engine.getDeviceManager().closeDevices();
}

} // namespace arrange
