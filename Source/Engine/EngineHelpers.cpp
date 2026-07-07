#include "EngineHelpers.h"
#include "GrooveEngine.h"
#include "ExportManager.h"
#include "NativePluginCatalog.h"
#include "DrumRackHelpers.h"
#include "TrackInputRouting.h"
#include "TrackPluginChainModel.h"
#include "UI/AppLookAndFeel.h"
#include "UI/Arrangement/EditViewState.h"
#include "UI/Arrangement/TimelineTypes.h"
#include "TrackPluginChainModel.h"
#include "PluginPresetManager.h"
#include "TracktionCommon.h"
#include <algorithm>

namespace skeletonhive
{

const juce::Identifier EngineHelpers::clipGroupProperty ("skeletonHiveClipGroup");
const juce::Identifier EngineHelpers::clipGroupOuterProperty ("skeletonHiveClipGroupOuter");
const juce::Identifier EngineHelpers::clipGroupColourProperty ("skeletonHiveClipGroupColour");
const juce::Identifier EngineHelpers::sessionSlotIdProperty ("skeletonHiveSessionSlotId");
const juce::Identifier EngineHelpers::clipScaleRootProperty ("skeletonHiveScaleRoot");
const juce::Identifier EngineHelpers::clipScaleModeProperty ("skeletonHiveScaleMode");
const juce::Identifier EngineHelpers::clipScaleLockProperty ("skeletonHiveScaleLock");
const juce::Identifier EngineHelpers::noteProbabilityProperty ("skeletonHiveNoteProbability");
const juce::Identifier EngineHelpers::noteIterationProperty ("skeletonHiveNoteIteration");
const juce::Identifier EngineHelpers::soloedPluginIdProperty ("skeletonHiveSoloedPluginId");
const juce::Identifier EngineHelpers::trackKindProperty ("skeletonHiveTrackKind");

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

juce::String EngineHelpers::getClipOuterGroup (const te::Clip& clip)
{
    return clip.state.getProperty (clipGroupOuterProperty).toString();
}

void EngineHelpers::setClipOuterGroup (te::Clip& clip, const juce::String& outerGroupId)
{
    if (outerGroupId.isEmpty())
        clip.state.removeProperty (clipGroupOuterProperty, &clip.edit.getUndoManager());
    else
        clip.state.setProperty (clipGroupOuterProperty, outerGroupId, &clip.edit.getUndoManager());
}

juce::Array<te::Clip*> EngineHelpers::getClipsSharingOuterGroup (te::Edit& edit, const juce::String& outerGroupId)
{
    juce::Array<te::Clip*> result;

    if (outerGroupId.isEmpty())
        return result;

    for (auto track : te::getAllTracks (edit))
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track))
            for (auto* c : clipTrack->getClips())
                if (getClipOuterGroup (*c) == outerGroupId)
                    result.add (c);

    return result;
}

juce::Array<te::Clip*> EngineHelpers::getGroupedPeers (te::Clip& clip)
{
    juce::Array<te::Clip*> peers;
    juce::Array<te::Clip*> stack;
    stack.add (&clip);

    for (int i = 0; i < stack.size(); ++i)
    {
        auto* current = stack.getReference (i);

        const auto outerId = getClipOuterGroup (*current);
        if (outerId.isNotEmpty())
        {
            for (auto* c : getClipsSharingOuterGroup (clip.edit, outerId))
                if (! stack.contains (c))
                    stack.add (c);
        }

        const auto innerId = getClipGroup (*current);
        if (innerId.isNotEmpty())
        {
            for (auto* c : getClipsInGroup (clip.edit, innerId))
                if (! stack.contains (c))
                    stack.add (c);
        }
    }

    for (auto* c : stack)
        if (c != &clip)
            peers.addIfNotAlreadyThere (c);

    return peers;
}

namespace
{
bool isArrangementTrack (te::Track& track)
{
    return ! track.isMarkerTrack() && ! track.isTempoTrack() && ! track.isChordTrack()
           && ! track.isMasterTrack() && ! track.isArrangerTrack();
}

juce::Array<te::Track*> getArrangementTracks (te::Edit& edit)
{
    juce::Array<te::Track*> tracks;

    for (auto track : te::getAllTracks (edit))
        if (isArrangementTrack (*track))
            tracks.add (track);

    return tracks;
}
} // namespace

int EngineHelpers::getArrangementTrackIndex (te::Edit& edit, te::Track& track)
{
    return getArrangementTracks (edit).indexOf (&track);
}

te::ClipTrack* EngineHelpers::getClipTrackAtArrangementIndex (te::Edit& edit, int index)
{
    const auto tracks = getArrangementTracks (edit);
    if (! juce::isPositiveAndBelow (index, tracks.size()))
        return nullptr;

    return dynamic_cast<te::ClipTrack*> (tracks[index]);
}

bool EngineHelpers::canMoveClipToTrack (const te::Clip& clip, te::ClipTrack& dest)
{
    if (clip.getClipTrack() == &dest)
        return true;

    if (dest.isFolderTrack())
        return false;

    if (isReturnTrack (dest))
        return false;

    if (dynamic_cast<const te::MidiClip*> (&clip) != nullptr && ! canHostMidiClips (dest))
        return false;

    return const_cast<te::Clip&> (clip).canBeAddedTo (dest);
}

void EngineHelpers::moveClipToTrack (te::Clip& clip, te::ClipTrack& dest, te::TimePosition start)
{
    if (! canMoveClipToTrack (clip, dest))
        return;

    if (clip.getClipTrack() != &dest)
        clip.moveTo (dest);

    clip.setStart (start, false, true);
}

bool EngineHelpers::moveClipGroupToTrack (te::Clip& leader, te::ClipTrack& destTrack, te::TimePosition start)
{
    auto* sourceTrack = leader.getClipTrack();
    if (sourceTrack == nullptr)
        return false;

    const int sourceIdx = getArrangementTrackIndex (leader.edit, *sourceTrack);
    const int destIdx = getArrangementTrackIndex (leader.edit, destTrack);

    if (sourceIdx < 0 || destIdx < 0)
        return false;

    const int offset = destIdx - sourceIdx;
    juce::Array<te::Clip*> toMove;
    toMove.add (&leader);

    for (auto* peer : getGroupedPeers (leader))
        toMove.addIfNotAlreadyThere (peer);

    for (auto* c : toMove)
    {
        auto* src = c->getClipTrack();
        if (src == nullptr)
            return false;

        const int targetIdx = getArrangementTrackIndex (leader.edit, *src) + offset;
        if (auto* target = getClipTrackAtArrangementIndex (leader.edit, targetIdx))
        {
            if (! canMoveClipToTrack (*c, *target))
                return false;
        }
        else
        {
            return false;
        }
    }

    const auto timeDelta = start - leader.getPosition().getStart();

    for (auto* c : toMove)
    {
        auto* src = c->getClipTrack();
        const int targetIdx = getArrangementTrackIndex (leader.edit, *src) + offset;
        auto* target = getClipTrackAtArrangementIndex (leader.edit, targetIdx);
        moveClipToTrack (*c, *target, c->getPosition().getStart() + timeDelta);
    }

    return true;
}

bool EngineHelpers::canReparentTrack (te::Track& dragged, te::Track& hoverRow, TrackDropZone zone)
{
    if (&dragged == &hoverRow)
        return false;

    if (! dragged.isMovable())
        return false;

    if (hoverRow.isAChildOf (dragged))
        return false;

    if (zone == TrackDropZone::intoFolder && ! hoverRow.isFolderTrack())
        return false;

    if (zone == TrackDropZone::promoteTopLevel && dragged.getParentFolderTrack() == nullptr)
        return false;

    return true;
}

te::TrackInsertPoint EngineHelpers::insertPointForDrop (te::Edit& edit, te::Track& hoverRow, TrackDropZone zone)
{
    switch (zone)
    {
        case TrackDropZone::above:
            return te::TrackInsertPoint (hoverRow, true);

        case TrackDropZone::below:
            return te::TrackInsertPoint (hoverRow, false);

        case TrackDropZone::intoFolder:
        {
            if (auto* folder = dynamic_cast<te::FolderTrack*> (&hoverRow))
            {
                te::Track* lastChild = nullptr;
                for (auto* child : folder->getAllSubTracks (false))
                    lastChild = child;
                return te::TrackInsertPoint (folder, lastChild);
            }
            return te::TrackInsertPoint (hoverRow, false);
        }

        case TrackDropZone::promoteTopLevel:
        {
            auto topLevel = te::getTopLevelTracks (edit);
            return te::TrackInsertPoint (nullptr, topLevel.getLast());
        }
    }

    return te::TrackInsertPoint (hoverRow, false);
}

void EngineHelpers::moveTrackToInsertPoint (te::Edit& edit, te::Track& track, te::TrackInsertPoint point)
{
    for (auto t : te::getAllTracks (edit))
    {
        if (t->itemID == track.itemID)
        {
            edit.moveTrack (t, point);
            break;
        }
    }
}

void EngineHelpers::moveTrackOutOfFolder (te::Track& track)
{
    if (track.getParentFolderTrack() == nullptr)
        return;

    auto& edit = track.edit;
    const auto topLevel = te::getTopLevelTracks (edit);
    const te::TrackInsertPoint point (nullptr, topLevel.getLast());

    for (auto t : te::getAllTracks (edit))
    {
        if (t->itemID == track.itemID)
        {
            edit.moveTrack (t, point);
            break;
        }
    }
}

void EngineHelpers::moveTrackBySiblingDelta (te::Track& track, int delta)
{
    if (delta == 0 || ! track.isMovable())
        return;

    if (auto* sibling = track.getSiblingTrack (delta, true))
    {
        const bool moveDown = delta > 0;
        const te::TrackInsertPoint point (*sibling, moveDown);

        for (auto t : te::getAllTracks (track.edit))
        {
            if (t->itemID == track.itemID)
            {
                track.edit.moveTrack (t, point);
                break;
            }
        }
    }
}

juce::String EngineHelpers::encodeTrackDrag (te::EditItemID trackId)
{
    return "skeletonHiveTrackDrag:" + trackId.toVar().toString();
}

te::EditItemID EngineHelpers::parseTrackDrag (const juce::var& description)
{
    const auto text = description.toString();
    if (! text.startsWith ("skeletonHiveTrackDrag:"))
        return {};

    return te::EditItemID::fromVar (text.fromFirstOccurrenceOf (":", false, false));
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
    if (groupId.isEmpty())
        return AppColours::clipGroupPalette (0);

    const auto index = (int) ((juce::uint32) groupId.hashCode() % 6);
    return AppColours::clipGroupPalette (index);
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

juce::Colour EngineHelpers::getClipFillColour (const te::Clip& clip, juce::Colour defaultColour)
{
    const auto colour = clip.getColour();
    if (colour != clip.getDefaultColour())
        return colour;

    return defaultColour;
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
    if (const auto nativeDesc = NativePluginCatalog::descriptionForPlugin (plugin);
        nativeDesc.name.isNotEmpty())
        return nativeDesc;

    for (const auto& desc : plugin.edit.engine.getPluginManager().knownPluginList.getTypes())
    {
        if (desc.name == plugin.getName())
            return desc;
    }

    juce::PluginDescription fallback;
    fallback.name = plugin.getName();
    return fallback;
}

te::Plugin::Ptr EngineHelpers::createPluginFromDescription (te::Edit& edit, const juce::PluginDescription& desc)
{
    if (NativePluginCatalog::isNativeDescription (desc))
        return NativePluginCatalog::createPlugin (edit, desc);

    return edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);
}

bool EngineHelpers::isInstrumentDescription (const juce::PluginDescription& desc)
{
    if (NativePluginCatalog::isNativeDescription (desc))
    {
        const auto xmlType = NativePluginCatalog::xmlTypeNameFromDescription (desc);
        return xmlType == te::SamplerPlugin::xmlTypeName
            || xmlType == te::FourOscPlugin::xmlTypeName
            || xmlType == DrumRackHelpers::drumRackXmlTypeName;
    }

    return desc.isInstrument;
}

bool EngineHelpers::isInstrumentPlugin (const te::Plugin& plugin)
{
    if (NativePluginCatalog::isNativeInstrumentPlugin (plugin))
        return true;

    if (auto* rack = dynamic_cast<const te::RackInstance*> (&plugin))
        return DrumRackHelpers::isDrumRack (*rack);

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

    if (auto newPlugin = createPluginFromDescription (plugin.edit, desc))
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

    if (auto newPlugin = createPluginFromDescription (track.edit, desc))
    {
        auto* inserted = insertPluginOnTrack (track, newPlugin, insertIndex);
        PluginPresetManager::applyPluginState (*newPlugin, state);
        return inserted;
    }

    return nullptr;
}

te::Plugin* EngineHelpers::replacePluginOnTrack (te::AudioTrack& track, te::Plugin& oldPlugin,
                                                 const juce::PluginDescription& newDesc)
{
    if (newDesc.name.isEmpty())
        return nullptr;

    if (dynamic_cast<te::RackInstance*> (&oldPlugin) != nullptr)
        return nullptr;

    const auto state = PluginPresetManager::capturePluginState (oldPlugin);
    TrackPluginChainModel model (track);
    const int userSlot = model.userSlotForPlugin (oldPlugin);

    if (userSlot < 0)
        return nullptr;

    const int insertIndex = model.resolveInsertIndex (userSlot,
                                                      isInstrumentDescription (newDesc),
                                                      &oldPlugin);

    if (insertIndex < 0)
        return nullptr;

    if (auto* trackPtr = te::getTrackContainingPlugin (oldPlugin.edit, &oldPlugin))
        if (isPluginSoloed (*trackPtr, oldPlugin))
            clearSoloedPlugin (*trackPtr);

    oldPlugin.deleteFromParent();

    if (auto newPlugin = createPluginFromDescription (track.edit, newDesc))
    {
        auto* inserted = insertPluginOnTrack (track, newPlugin, insertIndex);

        if (inserted != nullptr)
            PluginPresetManager::applyPluginState (*inserted, state);

        return inserted;
    }

    return nullptr;
}

te::Plugin* EngineHelpers::replacePluginInRack (te::RackInstance& rack, te::Plugin& oldPlugin,
                                                const juce::PluginDescription& newDesc)
{
    if (newDesc.name.isEmpty())
        return nullptr;

    const auto state = PluginPresetManager::capturePluginState (oldPlugin);
    const int targetSlot = rackSlotForPlugin (rack, oldPlugin);

    if (targetSlot < 0)
        return nullptr;

    if (auto* trackPtr = te::getTrackContainingPlugin (oldPlugin.edit, &rack))
        if (isPluginSoloed (*trackPtr, oldPlugin))
            clearSoloedPlugin (*trackPtr);

    oldPlugin.deleteFromParent();

    if (auto newPlugin = createPluginFromDescription (rack.edit, newDesc))
    {
        if (rack.type == nullptr || ! rack.type->addPlugin (*newPlugin, {}, true))
            return nullptr;

        auto* inserted = newPlugin.get();
        const int newSlot = rackSlotForPlugin (rack, *inserted);

        if (newSlot >= 0 && newSlot != targetSlot)
            movePluginInRack (rack, *inserted, targetSlot);

        PluginPresetManager::applyPluginState (*inserted, state);
        return inserted;
    }

    return nullptr;
}

void EngineHelpers::applyDefaultDeviceChain (te::AudioTrack& track, const juce::StringArray& pluginIdentifiers,
                                             te::Engine& engine, bool expectsInstrumentFirst)
{
    for (const auto& id : pluginIdentifiers)
    {
        const auto desc = lookupKnownPlugin (engine, id);

        if (desc.name.isEmpty())
            continue;

        if (auto plugin = createPluginFromDescription (track.edit, desc))
        {
            TrackPluginChainModel model (track);
            const int insertIndex = model.resolveInsertIndex (model.getUserChainSize(),
                                                              isInstrumentDescription (desc),
                                                              nullptr);

            if (insertIndex >= 0)
                insertPluginOnTrack (track, plugin, insertIndex);
        }
    }

    juce::ignoreUnused (expectsInstrumentFirst);
}

void EngineHelpers::renamePlugin (te::Plugin& plugin, const juce::String& newName)
{
    if (newName.isNotEmpty())
        plugin.state.setProperty (te::IDs::name, newName, &plugin.edit.getUndoManager());
}

juce::PluginDescription EngineHelpers::lookupKnownPlugin (te::Engine& engine, const juce::String& identifierString)
{
    if (const auto nativeDesc = NativePluginCatalog::lookupDescription (identifierString);
        nativeDesc.name.isNotEmpty())
        return nativeDesc;

    for (const auto& desc : engine.getPluginManager().knownPluginList.getTypes())
        if (desc.createIdentifierString() == identifierString)
            return desc;

    return {};
}

EngineHelpers::PluginLoadState EngineHelpers::getExternalPluginLoadState (te::Plugin& plugin,
                                                                          juce::String& statusMessage)
{
    auto* external = dynamic_cast<te::ExternalPlugin*> (&plugin);

    if (external == nullptr)
    {
        statusMessage = {};
        return PluginLoadState::ok;
    }

    if (external->isInitialisingAsync())
    {
        statusMessage = "Loading plugin...";
        return PluginLoadState::loading;
    }

    if (external->getAudioPluginInstance() != nullptr)
    {
        statusMessage = {};
        return PluginLoadState::ok;
    }

    statusMessage = external->getLoadError();

    if (statusMessage.isEmpty())
        statusMessage = "Plugin failed to load.";

    return PluginLoadState::failed;
}

juce::String EngineHelpers::getExternalPluginLoadError (te::Plugin& plugin)
{
    juce::String message;
    getExternalPluginLoadState (plugin, message);
    return message;
}

void EngineHelpers::showPluginLoadFailureAlert (juce::Component* parent,
                                                const juce::String& pluginName,
                                                const juce::String& errorMessage)
{
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                            "Plugin Load Failed",
                                            pluginName + "\n\n" + errorMessage,
                                            "OK",
                                            parent);
}

void EngineHelpers::showPluginLoadFailureAlert (juce::Component* parent, te::Plugin& plugin)
{
    showPluginLoadFailureAlert (parent, plugin.getName(), getExternalPluginLoadError (plugin));
}

void EngineHelpers::showPluginInsertFailureAlert (juce::Component* parent,
                                                  const juce::PluginDescription& desc)
{
    showPluginLoadFailureAlert (parent,
                                desc.name.isNotEmpty() ? desc.name : "Plugin",
                                "The plugin could not be created. It may have failed to load or is still initialising.");
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

void EngineHelpers::setTrackKind (te::Track& track, TrackKind kind)
{
    track.state.setProperty (trackKindProperty,
                             kind == TrackKind::midi ? "midi" : "audio",
                             &track.edit.getUndoManager());
}

EngineHelpers::TrackKind EngineHelpers::getTrackKind (const te::Track& track)
{
    const auto stored = track.state.getProperty (trackKindProperty).toString();

    if (stored == "midi")
        return TrackKind::midi;

    if (stored == "audio")
        return TrackKind::audio;

    if (isMidiTrack (track))
        return TrackKind::midi;

    if (auto* at = dynamic_cast<const te::AudioTrack*> (&track))
    {
        TrackPluginChainModel model (const_cast<te::AudioTrack&> (*at));

        for (auto* plugin : model.getUserChainPlugins())
            if (plugin != nullptr && isInstrumentPlugin (*plugin))
                return TrackKind::midi;
    }

    return TrackKind::audio;
}

bool EngineHelpers::isAudioKindTrack (const te::Track& track)
{
    return getTrackKind (track) == TrackKind::audio;
}

bool EngineHelpers::isMidiKindTrack (const te::Track& track)
{
    return getTrackKind (track) == TrackKind::midi;
}

void EngineHelpers::ensureTrackKinds (te::Edit& edit)
{
    for (auto* t : te::getAudioTracks (edit))
    {
        if (t == nullptr || t->isFolderTrack() || isReturnTrack (*t))
            continue;

        if (t->state.hasProperty (trackKindProperty))
            continue;

        setTrackKind (*t, getTrackKind (*t));
    }
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

void EngineHelpers::startParameterMidiLearn (te::Edit& edit, te::AutomatableParameter& parameter)
{
    edit.getParameterChangeHandler().setParameterLearnActive (true);
    edit.engine.getMidiLearnState().setActive (true);
    edit.getParameterChangeHandler().parameterChanged (parameter, false);
}

void EngineHelpers::removeParameterMidiMapping (te::Edit& edit, te::AutomatableParameter& parameter)
{
    edit.getParameterControlMappings().removeParameterMapping (parameter);
    edit.getParameterControlMappings().saveToEdit();
}

bool EngineHelpers::isParameterMidiMapped (te::Edit& edit, te::AutomatableParameter& parameter)
{
    return edit.getParameterControlMappings().isParameterMapped (parameter);
}

void EngineHelpers::setMidiLearnActive (te::Engine& engine, te::Edit& edit, bool active)
{
    engine.getMidiLearnState().setActive (active);

    if (! active)
        edit.getParameterChangeHandler().setParameterLearnActive (false);
}

bool EngineHelpers::isMidiLearnActive (te::Engine& engine)
{
    return engine.getMidiLearnState().isActive();
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
    const int before = (int) te::getAudioTracks (edit).size();
    edit.ensureNumberOfAudioTracks (index + 1);
    auto tracks = te::getAudioTracks (edit);

    for (int i = before; i < tracks.size(); ++i)
        if (tracks[i] != nullptr)
            setTrackKind (*tracks[i], TrackKind::audio);

    return tracks[index];
}

te::AudioTrack* EngineHelpers::getOrInsertAudioTrack (te::Edit& edit)
{
    return getOrInsertAudioTrackAt (edit, (int) te::getAudioTracks (edit).size());
}

te::AudioTrack* EngineHelpers::getOrInsertTrackForMidi (te::Edit& edit, int index)
{
    const int before = (int) te::getAudioTracks (edit).size();
    edit.ensureNumberOfAudioTracks (index + 1);
    auto tracks = te::getAudioTracks (edit);

    for (int i = before; i < tracks.size(); ++i)
        if (tracks[i] != nullptr)
            setTrackKind (*tracks[i], TrackKind::midi);

    return tracks[index];
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

juce::Array<te::InputDeviceInstance*> EngineHelpers::getInputInstancesForTrack (te::AudioTrack& track)
{
    juce::Array<te::InputDeviceInstance*> result;

    for (auto* instance : track.edit.getAllInputDevices())
        if (te::isOnTargetTrack (*instance, track, 0))
            result.add (instance);

    return result;
}

void EngineHelpers::assignDefaultInputToTrack (te::AudioTrack& track, bool preferMidi)
{
    auto& edit = track.edit;

    for (auto* instance : edit.getAllInputDevices())
    {
        const auto type = instance->getInputDevice().getDeviceType();
        const bool isMidiInput = type == te::InputDevice::physicalMidiDevice
                                 || type == te::InputDevice::virtualMidiDevice;

        if (isMidiInput != preferMidi)
            continue;

        setInputAssignedToTrack (*instance, track, true);

        if (! preferMidi)
            return;   // one wave input is enough; MIDI inputs are all assigned
    }
}

void EngineHelpers::armTrackWithDefaultInput (te::AudioTrack& track, bool arm)
{
    if (arm && getInputInstancesForTrack (track).isEmpty())
    {
        const auto kind = isMidiKindTrack (track) ? TrackInputKind::midi : TrackInputKind::audio;
        TrackInputRouting::assignFirstExternalSource (track, kind);
    }

    armTrack (track, arm);
}

bool EngineHelpers::isInputAssignedToTrack (te::InputDeviceInstance& instance, te::AudioTrack& track)
{
    return te::isOnTargetTrack (instance, track, 0);
}

void EngineHelpers::setInputAssignedToTrack (te::InputDeviceInstance& instance, te::AudioTrack& track, bool assign)
{
    auto& edit = track.edit;

    if (assign)
    {
        // Add as an extra destination first so an input can feed several
        // tracks; some devices only allow one target, so fall back to moving.
        if (! instance.setTarget (track.itemID, false, &edit.getUndoManager(), 0))
            [[maybe_unused]] const auto result = instance.setTarget (track.itemID, true, &edit.getUndoManager(), 0);
    }
    else
    {
        [[maybe_unused]] const auto result = instance.removeTarget (track.itemID, &edit.getUndoManager());
    }
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

    if (auto* audioTrack = getOrInsertAudioTrackAt (edit, 0))
        setTrackKind (*audioTrack, TrackKind::audio);

    if (auto* midiTrack = getOrInsertTrackForMidi (edit, 1))
        setTrackKind (*midiTrack, TrackKind::midi);

    edit.getTransport().ensureContextAllocated();

    int audioTrackNum = 0;
    for (auto* instance : edit.getAllInputDevices())
    {
        if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
        {
            if (auto* t = getOrInsertAudioTrackAt (edit, audioTrackNum))
            {
                TrackInputOption opt;
                opt.type = TrackInputOption::Type::externalDevice;
                opt.device = &instance->getInputDevice();
                TrackInputRouting::setActiveSource (*t, opt, TrackInputKind::audio);
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
                TrackInputOption opt;
                opt.type = TrackInputOption::Type::externalDevice;
                opt.device = &instance->getInputDevice();
                TrackInputRouting::setActiveSource (*t, opt, TrackInputKind::midi);
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

bool EngineHelpers::hasMultipleTakes (const te::Clip& clip)
{
    return getTakeCount (clip, true) > 1;
}

int EngineHelpers::getTakeCount (const te::Clip& clip, bool includeComps)
{
    if (auto* wave = dynamic_cast<const te::WaveAudioClip*> (&clip))
        return const_cast<te::WaveAudioClip*> (wave)->getNumTakes (includeComps);

    if (auto* midi = dynamic_cast<const te::MidiClip*> (&clip))
        return const_cast<te::MidiClip*> (midi)->getNumTakes (includeComps);

    return 0;
}

juce::String EngineHelpers::getTakeName (const te::Clip& clip, int index)
{
    const auto descriptions = getTakeDescriptions (clip);

    if (juce::isPositiveAndBelow (index, descriptions.size()))
        return descriptions[index];

    return "Take " + juce::String (index + 1);
}

juce::StringArray EngineHelpers::getTakeDescriptions (const te::Clip& clip)
{
    return clip.getTakeDescriptions();
}

void EngineHelpers::setActiveTake (te::Clip& clip, int takeIndex)
{
    clip.setCurrentTake (takeIndex);
}

bool EngineHelpers::isCurrentTakeComp (const te::Clip& clip)
{
    if (auto* wave = dynamic_cast<const te::WaveAudioClip*> (&clip))
        return const_cast<te::WaveAudioClip*> (wave)->isCurrentTakeComp();

    if (auto* midi = dynamic_cast<const te::MidiClip*> (&clip))
        return const_cast<te::MidiClip*> (midi)->isCurrentTakeComp();

    return false;
}

te::CompManager* EngineHelpers::getCompManager (te::Clip& clip)
{
    if (auto* wave = dynamic_cast<te::WaveAudioClip*> (&clip))
        return &wave->getCompManager();

    if (auto* midi = dynamic_cast<te::MidiClip*> (&clip))
        return &midi->getCompManager();

    return nullptr;
}

void EngineHelpers::ensureCompTake (te::Clip& clip)
{
    if (auto* cm = getCompManager (clip))
    {
        if (! cm->isCurrentTakeComp())
        {
            cm->addNewComp();
            cm->setActiveTakeIndex (cm->getTotalNumTakes() - 1);
        }
    }
}

void EngineHelpers::flattenCompToMain (te::Clip& clip, bool deleteSourceFiles)
{
    if (auto* cm = getCompManager (clip))
        cm->flattenTake (clip.getCurrentTake(), deleteSourceFiles);
}

juce::File EngineHelpers::getTakeSourceFile (te::Clip& clip, int takeIndex)
{
    if (auto* wave = dynamic_cast<te::WaveAudioClip*> (&clip))
    {
        const auto takes = wave->getTakes();

        if (! juce::isPositiveAndBelow (takeIndex, takes.size()))
            return {};

        if (auto item = clip.edit.engine.getProjectManager().getProjectItem (takes.getReference (takeIndex)))
            return item->getSourceFile();
    }

    return {};
}

bool EngineHelpers::isTakeLanesExpanded (EditViewState& editViewState, const te::Clip& clip)
{
    return editViewState.expandedTakeClipId.get() == (juce::int64) clip.itemID.getRawID();
}

void EngineHelpers::setTakeLanesExpanded (EditViewState& editViewState, te::Clip* clip)
{
    if (clip != nullptr && hasMultipleTakes (*clip))
        editViewState.expandedTakeClipId = (juce::int64) clip->itemID.getRawID();
    else
        editViewState.expandedTakeClipId = 0;
}

void EngineHelpers::toggleTakeLanesExpanded (EditViewState& editViewState, te::Clip& clip)
{
    if (isTakeLanesExpanded (editViewState, clip))
        setTakeLanesExpanded (editViewState, nullptr);
    else
        setTakeLanesExpanded (editViewState, &clip);
}

int EngineHelpers::getTakeLaneExtraHeight (EditViewState& editViewState, const te::Track& track)
{
    const auto expandedId = editViewState.expandedTakeClipId.get();

    if (expandedId == 0)
        return 0;

    if (auto* clipTrack = dynamic_cast<const te::ClipTrack*> (&track))
    {
        for (auto* c : clipTrack->getClips())
        {
            if ((juce::int64) c->itemID.getRawID() == expandedId && hasMultipleTakes (*c))
            {
                const int numRawTakes = getTakeCount (*c, false);
                return compLaneStripHeight + numRawTakes * takeLaneStripHeight;
            }
        }
    }

    return 0;
}

bool EngineHelpers::isCreateTakesOnLoopEnabled (te::Edit& edit)
{
    if (auto state = edit.state.getChildWithName ("EDITVIEWSTATE"); state.isValid())
        if (state.hasProperty ("createTakesOnLoop"))
            return (bool) state.getProperty ("createTakesOnLoop");

    auto& dm = edit.engine.getDeviceManager();

    for (auto& midiIn : dm.getMidiInDevices())
    {
        if (auto* midi = dynamic_cast<te::MidiInputDevice*> (midiIn.get()))
            return ! midi->mergeRecordings;
    }

    return true;
}

void EngineHelpers::setCreateTakesOnLoopEnabled (te::Edit& edit, bool enabled)
{
    if (auto state = edit.state.getOrCreateChildWithName ("EDITVIEWSTATE", nullptr); state.isValid())
        state.setProperty ("createTakesOnLoop", enabled, nullptr);

    auto& dm = edit.engine.getDeviceManager();

    for (auto& midiIn : dm.getMidiInDevices())
    {
        if (auto* midi = dynamic_cast<te::MidiInputDevice*> (midiIn.get()))
        {
            midi->mergeRecordings = ! enabled;
            midi->replaceExistingClips = false;
        }
    }
}

juce::Array<te::Clip*> EngineHelpers::expandWithGroupedPeers (const juce::Array<te::Clip*>& clips)
{
    juce::Array<te::Clip*> expanded;

    for (auto* clip : clips)
    {
        if (clip == nullptr)
            continue;

        expanded.addIfNotAlreadyThere (clip);

        for (auto* peer : getGroupedPeers (*clip))
            expanded.addIfNotAlreadyThere (peer);
    }

    return expanded;
}

te::WaveAudioClip* EngineHelpers::insertWaveClipFromFile (te::ClipTrack& track, const juce::File& file,
                                                          te::TimePosition start, const juce::String& name)
{
    te::AudioFile audioFile (track.edit.engine, file);
    if (! audioFile.isValid())
        return nullptr;

    const auto length = te::TimeDuration::fromSeconds (audioFile.getLength());
    const auto clipName = name.isNotEmpty() ? name : file.getFileNameWithoutExtension();
    const te::ClipPosition clipPos { { start, length }, {} };
    auto waveClip = te::insertWaveClip (track, clipName, file, clipPos, te::DeleteExistingClips::no);

    if (waveClip != nullptr)
    {
        waveClip->setAutoPitch (false);
        waveClip->setAutoTempo (false);
    }

    return waveClip.get();
}

namespace
{
juce::File createScopedRenderTempFile (te::Edit& edit, const juce::String& prefix)
{
    return edit.engine.getTemporaryFileManager().getTempDirectory()
               .getNonexistentChildFile (prefix, ".wav");
}

te::TimeRange boundingRangeForClips (const juce::Array<te::Clip*>& clips)
{
    if (clips.isEmpty())
        return {};

    te::TimePosition start = clips.getFirst()->getPosition().getStart();
    te::TimePosition end = clips.getFirst()->getPosition().getEnd();

    for (int i = 1; i < clips.size(); ++i)
    {
        if (auto* clip = clips[i])
        {
            const auto pos = clip->getPosition();
            start = juce::jmin (start, pos.getStart());
            end = juce::jmax (end, pos.getEnd());
        }
    }

    if (end <= start)
        return {};

    return { start, end };
}

juce::Array<te::Clip*> clipsOnTrack (const juce::Array<te::Clip*>& clips, te::ClipTrack& track)
{
    juce::Array<te::Clip*> result;

    for (auto* clip : clips)
        if (clip != nullptr && clip->getClipTrack() == &track)
            result.add (clip);

    return result;
}
} // namespace

juce::Array<te::Clip*> EngineHelpers::consolidateClips (te::Edit& edit, te::SelectionManager& selection,
                                                      juce::String* errorMessage)
{
    auto fail = [errorMessage] (const juce::String& message)
    {
        if (errorMessage != nullptr)
            *errorMessage = message;
        return juce::Array<te::Clip*> {};
    };

    const auto selected = selection.getItemsOfType<te::Clip>();
    if (selected.isEmpty())
        return fail ("Select one or more clips to consolidate.");

    for (auto* clip : selected)
        if (hasMultipleTakes (*clip))
            return fail ("Flatten comp takes before consolidating clips with take lanes.");

    juce::Array<te::ClipTrack*> tracks;
    for (auto* clip : selected)
    {
        if (auto* clipTrack = clip->getClipTrack())
            tracks.addIfNotAlreadyThere (clipTrack);
    }

    if (tracks.isEmpty())
        return fail ("No arrangement clips selected.");

    edit.getUndoManager().beginNewTransaction ("Consolidate");

    juce::Array<te::Clip*> created;
    juce::Array<te::Clip*> toDelete;

    for (auto* clipTrack : tracks)
    {
        const auto trackClips = clipsOnTrack (selected, *clipTrack);
        const auto range = boundingRangeForClips (trackClips);
        if (range.getLength() <= 0s)
            continue;

        const auto tempFile = createScopedRenderTempFile (edit, "consolidate");
        ExportManager::RenderScope scope;
        scope.time = range;
        scope.tracks.add (clipTrack);

        const auto rendered = ExportManager::renderScopeToFile (edit, tempFile, scope);
        if (! rendered.existsAsFile())
            return fail ("Consolidate render was cancelled or failed.");

        juce::String clipName = "Consolidated";
        if (trackClips.size() == 1 && trackClips.getFirst()->getName().isNotEmpty())
            clipName = trackClips.getFirst()->getName();

        if (auto* newClip = insertWaveClipFromFile (*clipTrack, rendered, range.getStart(), clipName))
        {
            created.add (newClip);
            for (auto* source : trackClips)
                toDelete.addIfNotAlreadyThere (source);
        }
        else
        {
            return fail ("Could not create consolidated audio clip.");
        }
    }

    for (auto* clip : toDelete)
        clip->removeFromParent();

    selection.deselectAll();
    for (auto* clip : created)
        selection.addToSelection (*clip);

    return created;
}

te::WaveAudioClip* EngineHelpers::flattenTrackToAudioClip (te::AudioTrack& track, te::TimeRange range,
                                                           bool deleteCoveredMidiClips,
                                                           juce::String* errorMessage)
{
    auto fail = [errorMessage] (const juce::String& message) -> te::WaveAudioClip*
    {
        if (errorMessage != nullptr)
            *errorMessage = message;
        return nullptr;
    };

    if (range.getLength() <= 0s)
        return fail ("Nothing to flatten in the selected range.");

    auto* clipTrack = dynamic_cast<te::ClipTrack*> (&track);
    if (clipTrack == nullptr)
        return fail ("This track cannot host audio clips.");

    const auto tempFile = createScopedRenderTempFile (track.edit, "flatten");
    ExportManager::RenderScope scope;
    scope.time = range;
    scope.tracks.add (&track);

    const auto rendered = ExportManager::renderScopeToFile (track.edit, tempFile, scope);
    if (! rendered.existsAsFile())
        return fail ("Flatten render was cancelled or failed.");

    track.edit.getUndoManager().beginNewTransaction ("Flatten Track");

    const juce::String clipName = track.getName().isNotEmpty() ? track.getName() + " (flat)"
                                                                 : juce::String ("Flattened");
    auto* newClip = insertWaveClipFromFile (*clipTrack, rendered, range.getStart(), clipName);
    if (newClip == nullptr)
        return fail ("Could not create flattened audio clip.");

    if (deleteCoveredMidiClips)
    {
        juce::Array<te::Clip*> toRemove;

        for (auto* clip : clipTrack->getClips())
        {
            if (clip == newClip)
                continue;

            if (dynamic_cast<te::MidiClip*> (clip) == nullptr)
                continue;

            const auto pos = clip->getPosition();
            if (pos.getStart() >= range.getStart() && pos.getEnd() <= range.getEnd())
                toRemove.add (clip);
        }

        for (auto* clip : toRemove)
            clip->removeFromParent();
    }

    TrackPluginChainModel model (track);
    const auto userPlugins = model.getUserChainPlugins();

    for (int i = userPlugins.size(); --i >= 0;)
    {
        if (auto* plugin = userPlugins[i])
        {
            if (isPluginSoloed (track, *plugin))
                clearSoloedPlugin (track);

            plugin->deleteFromParent();
        }
    }

    return newClip;
}

te::TimeRange EngineHelpers::resolveProductionRange (te::Edit& edit, te::ClipTrack& track,
                                                     te::SelectionManager& selection)
{
    juce::Array<te::Clip*> trackClips;

    for (auto* clip : selection.getItemsOfType<te::Clip>())
        if (clip->getClipTrack() == &track)
            trackClips.add (clip);

    if (! trackClips.isEmpty())
    {
        const auto range = boundingRangeForClips (trackClips);
        if (range.getLength() > 0s)
            return range;
    }

    const auto loopRange = edit.getTransport().getLoopRange();
    if (loopRange.getLength() > 0s)
        return loopRange;

    if (edit.getLength() > 0s)
        return te::TimeRange (0s, edit.getLength());

    return {};
}

int EngineHelpers::applyGrooveToSelection (te::Edit& edit, te::SelectionManager& selection,
                                           const GrooveTemplate& groove, juce::String* errorMessage)
{
    auto fail = [errorMessage] (const juce::String& message)
    {
        if (errorMessage != nullptr)
            *errorMessage = message;
        return 0;
    };

    const auto selected = selection.getItemsOfType<te::Clip>();
    juce::Array<te::MidiClip*> midiClips;

    for (auto* clip : selected)
        if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip))
            midiClips.addIfNotAlreadyThere (midiClip);

    if (midiClips.isEmpty())
        return fail ("Select one or more MIDI clips to apply a groove.");

    edit.getUndoManager().beginNewTransaction ("Apply Groove");

    int noteCount = 0;

    for (auto* midiClip : midiClips)
    {
        auto notes = midiClip->getSequence().getNotes();

        if (notes.isEmpty())
            continue;

        if (groove.isRandom)
            GrooveEngine::applyRandomHumanize (notes, 0.25, &edit.getUndoManager());
        else
            GrooveEngine::applyGroove (notes, groove, midiClip->getOffsetInBeats().inBeats(), &edit.getUndoManager());

        noteCount += notes.size();
    }

    if (noteCount == 0)
        return fail ("Selected MIDI clips contain no notes.");

    return noteCount;
}

juce::String EngineHelpers::makeSessionSlotId (te::EditItemID trackId, int sceneIndex)
{
    return juce::String ((juce::int64) trackId.getRawID()) + "_" + juce::String (sceneIndex);
}

bool EngineHelpers::isSessionClip (const te::Clip& clip)
{
    return clip.state.getProperty (sessionSlotIdProperty).toString().isNotEmpty();
}

juce::String EngineHelpers::getSessionSlotId (const te::Clip& clip)
{
    return clip.state.getProperty (sessionSlotIdProperty).toString();
}

void EngineHelpers::setSessionSlotId (te::Clip& clip, const juce::String& slotId)
{
    if (slotId.isEmpty())
        clip.state.removeProperty (sessionSlotIdProperty, &clip.edit.getUndoManager());
    else
        clip.state.setProperty (sessionSlotIdProperty, slotId, &clip.edit.getUndoManager());
}

void EngineHelpers::clearSessionClipTag (te::Clip& clip)
{
    setSessionSlotId (clip, {});
}

te::TimePosition EngineHelpers::sessionClipParkingPosition (const te::Edit& edit)
{
    return edit.tempoSequence.toTime (te::BeatPosition::fromBeats (1000000.0));
}

void EngineHelpers::parkSessionClip (te::Clip& clip)
{
    clip.setStart (sessionClipParkingPosition (clip.edit), false, true);
}

void EngineHelpers::activateSessionClipAtStart (te::Clip& clip)
{
    clip.setStart (0s, false, true);
}

void EngineHelpers::enableSessionClipLoop (te::Clip& clip)
{
    if (auto* audio = dynamic_cast<te::AudioClipBase*> (&clip))
    {
        if (auto* wave = dynamic_cast<te::WaveAudioClip*> (audio))
            wave->setLoopDefaults();

        const auto beatRange = clip.edit.tempoSequence.toBeats (clip.getPosition().time);
        const auto lengthBeats = beatRange.getLength().inBeats();
        const auto loopLength = te::BeatDuration::fromBeats (juce::jmax (0.25, lengthBeats));
        audio->setLoopRangeBeats ({ te::BeatPosition(), loopLength });
    }
}

te::Clip* EngineHelpers::findClipById (te::Edit& edit, te::EditItemID clipId)
{
    if (clipId == te::EditItemID())
        return nullptr;

    for (auto track : te::getAllTracks (edit))
    {
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track))
        {
            for (auto* clip : clipTrack->getClips())
            {
                if (clip->itemID == clipId)
                    return clip;
            }
        }
    }

    return nullptr;
}

int EngineHelpers::getClipScaleRoot (const te::Clip& clip)
{
    return juce::jlimit (0, 11, (int) clip.state.getProperty (clipScaleRootProperty, 0));
}

void EngineHelpers::setClipScaleRoot (te::Clip& clip, int root)
{
    clip.state.setProperty (clipScaleRootProperty, juce::jlimit (0, 11, root), &clip.edit.getUndoManager());
}

ScaleMode EngineHelpers::getClipScaleMode (const te::Clip& clip)
{
    return (ScaleMode) juce::jlimit (0, 2, (int) clip.state.getProperty (clipScaleModeProperty, 0));
}

void EngineHelpers::setClipScaleMode (te::Clip& clip, ScaleMode mode)
{
    clip.state.setProperty (clipScaleModeProperty, (int) mode, &clip.edit.getUndoManager());
}

bool EngineHelpers::getClipScaleLock (const te::Clip& clip)
{
    return (bool) clip.state.getProperty (clipScaleLockProperty, false);
}

void EngineHelpers::setClipScaleLock (te::Clip& clip, bool locked)
{
    clip.state.setProperty (clipScaleLockProperty, locked, &clip.edit.getUndoManager());
}

int EngineHelpers::getNoteProbability (juce::ValueTree noteState)
{
    return juce::jlimit (0, 100, (int) noteState.getProperty (noteProbabilityProperty, 100));
}

void EngineHelpers::setNoteProbability (juce::ValueTree noteState, int probability, juce::UndoManager* um)
{
    noteState.setProperty (noteProbabilityProperty, juce::jlimit (0, 100, probability), um);
}

int EngineHelpers::getNoteIteration (juce::ValueTree noteState)
{
    return juce::jmax (0, (int) noteState.getProperty (noteIterationProperty, 0));
}

void EngineHelpers::setNoteIteration (juce::ValueTree noteState, int iteration, juce::UndoManager* um)
{
    noteState.setProperty (noteIterationProperty, juce::jmax (0, iteration), um);
}

#if JUCE_DEBUG
void EngineHelpers::createStressTestTracks (te::Edit& edit, int trackCount, int sceneCount)
{
    juce::ignoreUnused (sceneCount);

    for (int i = 0; i < trackCount; ++i)
    {
        if (auto* track = getOrInsertTrackForMidi (edit, edit.getTrackList().size()))
        {
            track->setName ("Stress " + juce::String (i + 1));

            if (auto clip = createMidiClipOnTrack (*track, { 0s, edit.tempoSequence.toTime (te::BeatPosition::fromBeats (4.0)) },
                                                   "Slot Clip"))
            {
                clip->getSequence().addNote (60 + (i % 12),
                                             te::BeatPosition(),
                                             te::BeatDuration::fromBeats (1.0),
                                             100, 0, &edit.getUndoManager());
                setSessionSlotId (*clip, makeSessionSlotId (track->itemID, i % juce::jmax (1, sceneCount)));
            }
        }
    }
}
#endif

} // namespace skeletonhive
