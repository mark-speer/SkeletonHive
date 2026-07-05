#include "EngineHelpers.h"
#include "TrackPluginChainModel.h"
#include "PluginPresetManager.h"
#include "TracktionCommon.h"
#include <algorithm>

namespace arrange
{

const juce::Identifier EngineHelpers::clipGroupProperty ("arrangeClipGroup");
const juce::Identifier EngineHelpers::clipGroupColourProperty ("arrangeClipGroupColour");
const juce::Identifier EngineHelpers::soloedPluginIdProperty ("arrangeSoloedPluginId");

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

juce::String EngineHelpers::auxBusName (int busNumber)
{
    return juce::String::charToString ((juce::juce_wchar) ('A' + busNumber));
}

juce::Array<te::AuxSendPlugin*> EngineHelpers::getAllAuxSends (te::AudioTrack& track)
{
    juce::Array<te::AuxSendPlugin*> sends;
    for (auto p : track.pluginList)
        if (auto* send = dynamic_cast<te::AuxSendPlugin*> (p))
            sends.add (send);

    std::sort (sends.begin(), sends.end(), [] (const te::AuxSendPlugin* a, const te::AuxSendPlugin* b)
    {
        return a->getBusNumber() < b->getBusNumber();
    });

    return sends;
}

int EngineHelpers::getUserChainInsertIndex (te::AudioTrack& track)
{
    for (int i = 0; i < track.pluginList.size(); ++i)
        if (dynamic_cast<te::VolumeAndPanPlugin*> (track.pluginList[i]) != nullptr)
            return i;

    return track.pluginList.size();
}

te::Plugin* EngineHelpers::insertPluginOnTrack (te::AudioTrack& track, te::Plugin::Ptr plugin, int index)
{
    if (plugin == nullptr)
        return nullptr;

    if (index < 0)
        index = getUserChainInsertIndex (track);

    track.pluginList.insertPlugin (plugin, index, nullptr);
    return plugin.get();
}

juce::PluginDescription EngineHelpers::getPluginDescription (const te::Plugin& plugin)
{
    for (const auto& desc : plugin.edit.engine.getPluginManager().knownPluginList.getTypes())
    {
        if (desc.name == plugin.getName())
            return desc;
    }

    juce::PluginDescription fallback;
    fallback.name = plugin.getName();
    return fallback;
}

bool EngineHelpers::isInstrumentDescription (const juce::PluginDescription& desc)
{
    return desc.isInstrument;
}

bool EngineHelpers::isInstrumentPlugin (const te::Plugin& plugin)
{
    if (auto* ext = dynamic_cast<const te::ExternalPlugin*> (&plugin))
    {
        const auto desc = getPluginDescription (plugin);
        if (desc.name.isNotEmpty() && desc.isInstrument)
            return true;
    }

    return false;
}

int EngineHelpers::findInstrumentSlot (te::AudioTrack& track)
{
    int slot = 0;
    for (auto p : track.pluginList)
    {
        if (! isFooterVisiblePlugin (*p))
            continue;

        if (isInstrumentPlugin (*p))
            return slot;

        ++slot;
    }

    return -1;
}

bool EngineHelpers::movePluginToUserSlot (te::AudioTrack& track, te::Plugin& plugin, int userSlot)
{
    TrackPluginChainModel model (track);
    const int targetListIndex = model.resolveInsertIndex (userSlot,
                                                          isInstrumentPlugin (plugin),
                                                          &plugin);
    if (targetListIndex < 0)
        return false;

    const int fromIndex = track.pluginList.indexOf (&plugin);
    if (fromIndex < 0 || fromIndex == targetListIndex)
        return false;

    track.pluginList.state.moveChild (fromIndex, targetListIndex, &track.edit.getUndoManager());
    return true;
}

bool EngineHelpers::movePluginToTrack (te::Plugin& plugin, te::AudioTrack& destTrack, int userSlot)
{
    if (auto* srcTrack = dynamic_cast<te::AudioTrack*> (te::getTrackContainingPlugin (plugin.edit, &plugin)))
    {
        if (srcTrack == &destTrack)
            return movePluginToUserSlot (destTrack, plugin, userSlot);
    }

    const auto state = PluginPresetManager::capturePluginState (plugin);
    const auto desc = getPluginDescription (plugin);
    const bool instrument = isInstrumentPlugin (plugin) || isInstrumentDescription (desc);

    TrackPluginChainModel destModel (destTrack);
    const int insertIndex = destModel.resolveInsertIndex (userSlot, instrument, nullptr);
    if (insertIndex < 0)
        return false;

    auto& um = plugin.edit.getUndoManager();
    juce::ignoreUnused (um);

    if (EngineHelpers::isPluginSoloed (*te::getTrackContainingPlugin (plugin.edit, &plugin), plugin))
        clearSoloedPlugin (*te::getTrackContainingPlugin (plugin.edit, &plugin));

    plugin.deleteFromParent();

    if (desc.name.isEmpty())
        return false;

    if (auto newPlugin = plugin.edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc))
    {
        insertPluginOnTrack (destTrack, newPlugin, insertIndex);
        PluginPresetManager::applyPluginState (*newPlugin, state);
        return true;
    }

    return false;
}

te::Plugin* EngineHelpers::duplicatePluginOnTrack (te::Plugin& source, te::AudioTrack& track, int userSlot)
{
    const auto state = PluginPresetManager::capturePluginState (source);
    const auto desc = getPluginDescription (source);
    if (desc.name.isEmpty())
        return nullptr;

    TrackPluginChainModel model (track);
    const int slot = userSlot < 0 ? model.userSlotForPlugin (source) + 1 : userSlot;
    const int insertIndex = model.resolveInsertIndex (slot,
                                                      isInstrumentPlugin (source),
                                                      nullptr);
    if (insertIndex < 0)
        return nullptr;

    if (auto newPlugin = track.edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc))
    {
        auto* inserted = insertPluginOnTrack (track, newPlugin, insertIndex);
        PluginPresetManager::applyPluginState (*newPlugin, state);
        return inserted;
    }

    return nullptr;
}

void EngineHelpers::renamePlugin (te::Plugin& plugin, const juce::String& newName)
{
    if (newName.isNotEmpty())
        plugin.state.setProperty (te::IDs::name, newName, &plugin.edit.getUndoManager());
}

juce::PluginDescription EngineHelpers::lookupKnownPlugin (te::Engine& engine, const juce::String& identifierString)
{
    for (const auto& desc : engine.getPluginManager().knownPluginList.getTypes())
        if (desc.createIdentifierString() == identifierString)
            return desc;

    return {};
}

te::AuxSendPlugin* EngineHelpers::addAuxSend (te::AudioTrack& track, int busNumber)
{
    return getOrCreateAuxSend (track, busNumber);
}

te::AuxSendPlugin* EngineHelpers::getOrCreateAuxSend (te::AudioTrack& track, int busNumber)
{
    if (auto* existing = track.getAuxSendPlugin (busNumber))
        return existing;

    auto plugin = track.edit.getPluginCache().createNewPlugin (te::AuxSendPlugin::xmlTypeName, {});
    if (plugin == nullptr)
        return nullptr;

    track.pluginList.insertPlugin (plugin, getUserChainInsertIndex (track), nullptr);

    auto* send = dynamic_cast<te::AuxSendPlugin*> (plugin.get());
    if (send != nullptr)
        send->busNumber = busNumber;

    return send;
}

bool EngineHelpers::isSendPreFader (te::AudioTrack& track, const te::AuxSendPlugin& send)
{
    int volIndex = -1, sendIndex = -1;
    for (int i = 0; i < track.pluginList.size(); ++i)
    {
        if (dynamic_cast<const te::VolumeAndPanPlugin*> (track.pluginList[i]) != nullptr)
            volIndex = i;
        if (track.pluginList[i] == &send)
            sendIndex = i;
    }

    return volIndex >= 0 && sendIndex >= 0 && sendIndex < volIndex;
}

void EngineHelpers::setSendPreFader (te::AudioTrack& track, te::AuxSendPlugin& send, bool preFader)
{
    int volIndex = -1, sendIndex = track.pluginList.indexOf (&send);
    for (int i = 0; i < track.pluginList.size(); ++i)
        if (dynamic_cast<te::VolumeAndPanPlugin*> (track.pluginList[i]) != nullptr)
            volIndex = i;

    if (volIndex < 0 || sendIndex < 0)
        return;

    const int targetIndex = preFader ? volIndex : juce::jmin (volIndex + 1, track.pluginList.size() - 1);
    if (sendIndex != targetIndex)
        track.pluginList.state.moveChild (sendIndex, targetIndex, &track.edit.getUndoManager());
}

bool EngineHelpers::isFooterVisiblePlugin (const te::Plugin& plugin)
{
    if (dynamic_cast<const te::VolumeAndPanPlugin*> (&plugin) != nullptr
        || dynamic_cast<const te::LevelMeterPlugin*> (&plugin) != nullptr
        || dynamic_cast<const te::AuxSendPlugin*> (&plugin) != nullptr
        || dynamic_cast<const te::AuxReturnPlugin*> (&plugin) != nullptr)
        return false;

    return true;
}

bool EngineHelpers::hasWetDryMix (const te::Plugin& plugin)
{
    return getDryParam (const_cast<te::Plugin&> (plugin)) != nullptr;
}

te::AutomatableParameter* EngineHelpers::getDryParam (te::Plugin& plugin)
{
    if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&plugin))
        return ext->dryGain.get();

    if (auto* rack = dynamic_cast<te::RackInstance*> (&plugin))
        return rack->dryGain.get();

    return nullptr;
}

te::AutomatableParameter* EngineHelpers::getWetParam (te::Plugin& plugin)
{
    if (auto* ext = dynamic_cast<te::ExternalPlugin*> (&plugin))
        return ext->wetGain.get();

    if (auto* rack = dynamic_cast<te::RackInstance*> (&plugin))
        return rack->wetGain.get();

    return nullptr;
}

te::EditItemID EngineHelpers::getSoloedPluginId (const te::Track& track)
{
    return te::EditItemID::fromProperty (track.state, soloedPluginIdProperty);
}

void EngineHelpers::setSoloedPlugin (te::Track& track, te::Plugin* plugin)
{
    auto& um = track.edit.getUndoManager();
    auto* audioTrack = dynamic_cast<te::AudioTrack*> (&track);
    if (audioTrack == nullptr || plugin == nullptr)
        return;

    const auto soloId = plugin->itemID;
    soloId.setProperty (track.state, soloedPluginIdProperty, &um);

    for (auto p : audioTrack->pluginList)
    {
        if (! isFooterVisiblePlugin (*p))
            continue;

        p->setEnabled (p->itemID == soloId);
    }
}

void EngineHelpers::clearSoloedPlugin (te::Track& track)
{
    if (! track.state.hasProperty (soloedPluginIdProperty))
        return;

    track.state.removeProperty (soloedPluginIdProperty, &track.edit.getUndoManager());

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (&track))
        for (auto p : audioTrack->pluginList)
            if (isFooterVisiblePlugin (*p))
                p->setEnabled (true);
}

bool EngineHelpers::isPluginSoloed (const te::Track& track, const te::Plugin& plugin)
{
    return getSoloedPluginId (track) == plugin.itemID;
}

te::RackInstance* EngineHelpers::wrapPluginsInRack (te::SelectionManager& selection)
{
    return te::Plugin::wrapSelectedPluginsInRack (selection);
}

te::RackInstance* EngineHelpers::insertEmptyRack (te::AudioTrack& track)
{
    auto rackType = track.edit.getRackList().addNewRack();
    if (rackType == nullptr)
        return nullptr;

    rackType->rackName = "Rack";

    if (auto plugin = track.edit.getPluginCache().createNewPlugin (te::RackInstance::create (*rackType)))
        return dynamic_cast<te::RackInstance*> (insertPluginOnTrack (track, plugin));

    return nullptr;
}

namespace
{
int rackPluginStateIndex (te::RackType& type, const te::Plugin& plugin)
{
    for (int i = 0; i < type.state.getNumChildren(); ++i)
    {
        const auto child = type.state.getChild (i);
        if (! child.hasType (te::IDs::PLUGININSTANCE))
            continue;

        const auto pluginTree = child.getChildWithName (te::IDs::PLUGIN);
        if (pluginTree.isValid()
            && te::EditItemID::fromProperty (pluginTree, te::IDs::id) == plugin.itemID)
            return i;
    }

    return -1;
}
} // namespace

juce::Array<te::Plugin*> EngineHelpers::getRackInternalPlugins (te::RackInstance& rack)
{
    return rack.type->getPlugins();
}

int EngineHelpers::rackSlotForPlugin (te::RackInstance& rack, const te::Plugin& plugin)
{
    const auto plugins = getRackInternalPlugins (rack);
    for (int i = 0; i < plugins.size(); ++i)
        if (plugins[i] == &plugin)
            return i;

    return -1;
}

bool EngineHelpers::movePluginInRack (te::RackInstance& rack, te::Plugin& plugin, int targetSlot)
{
    auto& type = *rack.type;
    const auto plugins = type.getPlugins();
    const int fromSlot = rackSlotForPlugin (rack, plugin);

    if (fromSlot < 0)
        return false;

    const int clampedTarget = juce::jlimit (0, plugins.size(), targetSlot);
    int destSlot = clampedTarget;

    if (destSlot > fromSlot)
        --destSlot;

    if (fromSlot == destSlot)
        return false;

    const int fromIndex = rackPluginStateIndex (type, plugin);

    if (fromIndex < 0)
        return false;

    int toIndex = fromIndex;

    if (destSlot >= plugins.size())
    {
        for (int i = type.state.getNumChildren(); --i >= 0;)
        {
            if (type.state.getChild (i).hasType (te::IDs::PLUGININSTANCE))
            {
                toIndex = i + 1;
                break;
            }
        }
    }
    else if (auto* destPlugin = plugins[destSlot])
    {
        toIndex = rackPluginStateIndex (type, *destPlugin);
        if (toIndex < 0)
            return false;
    }

    if (fromIndex == toIndex)
        return false;

    type.state.moveChild (fromIndex, toIndex, type.getUndoManager());
    return true;
}

te::RackInstance* EngineHelpers::findRackOnTrack (te::AudioTrack& track, te::EditItemID rackInstanceId)
{
    if (rackInstanceId.isInvalid())
        return nullptr;

    for (auto p : track.pluginList)
        if (auto* rack = dynamic_cast<te::RackInstance*> (p))
            if (rack->itemID == rackInstanceId)
                return rack;

    return nullptr;
}

bool EngineHelpers::isMidiTrack (const te::Track& track)
{
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
        return hasMidi && ! hasAudio;
    }

    return false;
}

bool EngineHelpers::canHostMidiClips (const te::Track& track)
{
    if (auto* clipTrack = dynamic_cast<const te::ClipTrack*> (&track))
    {
        for (auto* c : clipTrack->getClips())
            if (dynamic_cast<te::WaveAudioClip*> (c) != nullptr)
                return false;

        return true;
    }

    return false;
}

bool EngineHelpers::isReturnTrack (const te::Track& track)
{
    if (auto* audioTrack = dynamic_cast<const te::AudioTrack*> (&track))
        return audioTrack->pluginList.findFirstPluginOfType<te::AuxReturnPlugin>() != nullptr;

    return false;
}

int EngineHelpers::getTrackIndentLevel (const te::Track& track)
{
    int depth = 0;
    for (auto* parent = track.getParentFolderTrack(); parent != nullptr; parent = parent->getParentFolderTrack())
        ++depth;
    return depth;
}

te::FolderTrack* EngineHelpers::createFolderTrack (te::Edit& edit, te::SelectionManager* selectionManager)
{
    auto folder = edit.insertNewFolderTrack (te::TrackInsertPoint::getEndOfTracks (edit), selectionManager, false);
    return folder.get();
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
    return getOrInsertAudioTrackAt (edit, (int) te::getAudioTracks (edit).size());
}

te::AudioTrack* EngineHelpers::getOrInsertTrackForMidi (te::Edit& edit, int index)
{
    return getOrInsertAudioTrackAt (edit, index);
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
    getOrInsertAudioTrackAt (edit, 0);
    getOrInsertTrackForMidi (edit, 1);
    edit.getTransport().ensureContextAllocated();

    int audioTrackNum = 0;
    for (auto* instance : edit.getAllInputDevices())
    {
        if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
        {
            if (auto* t = getOrInsertAudioTrackAt (edit, audioTrackNum))
            {
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
