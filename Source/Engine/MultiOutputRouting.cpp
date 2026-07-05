#include "MultiOutputRouting.h"
#include "EngineHelpers.h"
#include "UI/Plugins/MultiOutputConfigDialog.h"

namespace arrange
{

const juce::Identifier MultiOutputRouting::routesProperty ("arrangeMultiOutRoutes");
const juce::Identifier MultiOutputRouting::routeTypeProperty ("arrangeMultiOutRoute");
const juce::Identifier MultiOutputRouting::routeBusIndexProperty ("arrangeMultiOutBusIndex");
const juce::Identifier MultiOutputRouting::routeTrackIdProperty ("arrangeMultiOutTrackId");
const juce::Identifier MultiOutputRouting::routeEnabledProperty ("arrangeMultiOutEnabled");
const juce::Identifier MultiOutputRouting::childSourcePluginIdProperty ("arrangeMultiOutSourcePluginId");
const juce::Identifier MultiOutputRouting::childBusIndexProperty ("arrangeMultiOutChildBusIndex");
const juce::Identifier MultiOutputRouting::childParentTrackIdProperty ("arrangeMultiOutParentTrackId");

namespace
{

te::ExternalPlugin* asExternal (te::Plugin& plugin)
{
    return dynamic_cast<te::ExternalPlugin*> (&plugin);
}

const te::ExternalPlugin* asExternal (const te::Plugin& plugin)
{
    return dynamic_cast<const te::ExternalPlugin*> (&plugin);
}

juce::AudioPluginInstance* getInstance (te::Plugin& plugin)
{
    if (auto* ext = asExternal (plugin))
        return ext->getAudioPluginInstance();

    return nullptr;
}

juce::String busDisplayName (juce::AudioProcessor& proc, int busIndex, int numChannels)
{
    if (auto* bus = proc.getBus (false, busIndex))
        if (bus->getName().isNotEmpty())
            return bus->getName();

    if (busIndex == 0)
        return "Main";

    return "Out " + juce::String (busIndex + 1)
         + " (" + juce::String (numChannels) + " ch)";
}

void writeRoutesToPlugin (te::Plugin& instrument, const juce::Array<OutputRoute>& routes, juce::UndoManager& um)
{
    auto routesTree = instrument.state.getOrCreateChildWithName (MultiOutputRouting::routesProperty, &um);
    routesTree.removeAllChildren (&um);

    for (const auto& route : routes)
    {
        juce::ValueTree entry (MultiOutputRouting::routeTypeProperty);
        entry.setProperty (MultiOutputRouting::routeBusIndexProperty, route.outputBusIndex, &um);
        entry.setProperty (MultiOutputRouting::routeEnabledProperty, route.enabled, &um);
        route.childTrackId.setProperty (entry, MultiOutputRouting::routeTrackIdProperty, &um);
        routesTree.addChild (entry, -1, &um);
    }
}

te::InputDeviceInstance* getTrackWaveInstance (te::Edit& edit, te::AudioTrack& sourceTrack)
{
    edit.getTransport().ensureContextAllocated();
    edit.getEditInputDevices().getInstanceStateForInputDevice (sourceTrack.getWaveInputDevice());

    if (auto* epc = edit.getCurrentPlaybackContext())
        return epc->getInputFor (&sourceTrack.getWaveInputDevice());

    return nullptr;
}

void clearTrackInputFromSource (te::AudioTrack& destTrack, te::AudioTrack& sourceTrack, juce::UndoManager& um)
{
    if (auto* instance = getTrackWaveInstance (destTrack.edit, sourceTrack))
        [[maybe_unused]] auto res = instance->removeTarget (destTrack.itemID, &um);
}

bool assignSourceBusToTrack (te::AudioTrack& destTrack, te::AudioTrack& sourceTrack,
                             int outputBusIndex, juce::UndoManager& um)
{
    if (auto* instance = getTrackWaveInstance (destTrack.edit, sourceTrack))
    {
        const auto result = instance->setTarget (destTrack.itemID, false, &um, outputBusIndex);
        return result.has_value();
    }

    return te::assignTrackAsInput (destTrack, sourceTrack, te::InputDevice::trackWaveDevice) != nullptr;
}

void tagChildTrack (te::AudioTrack& child, te::AudioTrack& parent, te::Plugin& instrument, int busIndex,
                    juce::UndoManager& um)
{
    instrument.itemID.setProperty (child.state, MultiOutputRouting::childSourcePluginIdProperty, &um);
    parent.itemID.setProperty (child.state, MultiOutputRouting::childParentTrackIdProperty, &um);
    child.state.setProperty (MultiOutputRouting::childBusIndexProperty, busIndex, &um);
}

} // namespace

bool MultiOutputRouting::isMultiOutputCapable (const te::Plugin& plugin)
{
    if (! EngineHelpers::isInstrumentPlugin (plugin))
        return false;

    if (plugin.state.getChildWithName (routesProperty).isValid())
        return true;

    if (auto* ext = asExternal (plugin))
    {
        if (auto* pi = ext->getAudioPluginInstance())
        {
            if (pi->getBusCount (false) > 1)
                return true;

            if (ext->getNumOutputs() > 2)
                return true;

            for (int i = 0; i < pi->getBusCount (false); ++i)
                if (pi->getChannelLayoutOfBus (false, i).size() > 2)
                    return true;

            return pi->getBus (false, 0) != nullptr && pi->getBusCount (false) == 1 && pi->canAddBus (false);
        }
    }

    return false;
}

juce::Array<OutputBusInfo> MultiOutputRouting::getOutputBuses (te::Plugin& plugin)
{
    juce::Array<OutputBusInfo> buses;

    if (auto* pi = getInstance (plugin))
    {
        const int numBuses = pi->getBusCount (false);

        for (int busIndex = 0; busIndex < numBuses; ++busIndex)
        {
            OutputBusInfo info;
            info.busIndex = busIndex;

            if (auto* bus = pi->getBus (false, busIndex))
            {
                info.numChannels = bus->getNumberOfChannels();
                info.isActive = bus->isEnabled() && info.numChannels > 0;
                info.name = busDisplayName (*pi, busIndex, info.numChannels);
            }
            else
            {
                info.name = busDisplayName (*pi, busIndex, 0);
            }

            buses.add (info);
        }

        if (numBuses == 1 && pi->canAddBus (false))
        {
            OutputBusInfo extra;
            extra.busIndex = 1;
            extra.name = "Additional output";
            extra.numChannels = 0;
            extra.isActive = false;
            buses.add (extra);
        }
    }
    else if (auto* ext = asExternal (plugin))
    {
        OutputBusInfo main;
        main.busIndex = 0;
        main.name = "Main";
        main.numChannels = juce::jmax (2, ext->getNumOutputs());
        main.isActive = main.numChannels > 0;
        buses.add (main);
    }

    return buses;
}

juce::Array<OutputRoute> MultiOutputRouting::getRoutes (const te::Plugin& plugin)
{
    juce::Array<OutputRoute> routes;
    const auto routesTree = plugin.state.getChildWithName (routesProperty);

    if (! routesTree.isValid())
        return routes;

    for (int i = 0; i < routesTree.getNumChildren(); ++i)
    {
        const auto child = routesTree.getChild (i);
        if (! child.hasType (routeTypeProperty))
            continue;

        OutputRoute route;
        route.outputBusIndex = (int) child.getProperty (routeBusIndexProperty, 0);
        route.enabled = (bool) child.getProperty (routeEnabledProperty, false);
        route.childTrackId = te::EditItemID::fromProperty (child, routeTrackIdProperty);
        routes.add (route);
    }

    return routes;
}

bool MultiOutputRouting::applyRoutes (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                                      const juce::Array<OutputRoute>& routes)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    auto& um = instrumentTrack.edit.getUndoManager();
    const auto previousRoutes = getRoutes (instrument);

    if (auto* ext = asExternal (instrument))
    {
        if (auto* pi = ext->getAudioPluginInstance())
        {
            for (const auto& route : routes)
            {
                if (! route.enabled || route.outputBusIndex <= 0)
                    continue;

                while (pi->getBusCount (false) <= route.outputBusIndex)
                    if (! pi->addBus (false))
                        break;

                if (auto* bus = pi->getBus (false, route.outputBusIndex))
                    if (! bus->isEnabled() || bus->getNumberOfChannels() == 0)
                        ext->setBusLayout (juce::AudioChannelSet::stereo(), false, route.outputBusIndex);
            }
        }
    }

    for (const auto& prev : previousRoutes)
    {
        const bool stillEnabled = [&]
        {
            for (const auto& r : routes)
                if (r.outputBusIndex == prev.outputBusIndex && r.enabled)
                    return true;
            return false;
        }();

        if (! stillEnabled && prev.childTrackId.isValid())
            if (auto* existing = te::findAudioTrackForID (instrumentTrack.edit, prev.childTrackId))
                clearTrackInputFromSource (*existing, instrumentTrack, um);
    }

    juce::Array<OutputRoute> persisted;

    for (const auto& route : routes)
    {
        OutputRoute stored = route;

        if (route.enabled && route.outputBusIndex > 0)
        {
            te::AudioTrack* childTrack = te::findAudioTrackForID (instrumentTrack.edit, route.childTrackId);

            if (childTrack == nullptr)
            {
                juce::String busName = "Out " + juce::String (route.outputBusIndex + 1);
                for (const auto& bus : getOutputBuses (instrument))
                    if (bus.busIndex == route.outputBusIndex)
                        busName = bus.name;

                const juce::String trackName = instrument.getName() + " \u2014 " + busName;
                const te::TrackInsertPoint insertPoint (instrumentTrack, false);
                auto newChild = instrumentTrack.edit.insertNewAudioTrack (insertPoint, nullptr, false);
                childTrack = newChild.get();

                if (childTrack != nullptr)
                    childTrack->setName (trackName);
            }

            if (childTrack != nullptr)
            {
                stored.childTrackId = childTrack->itemID;
                tagChildTrack (*childTrack, instrumentTrack, instrument, route.outputBusIndex, um);
                assignSourceBusToTrack (*childTrack, instrumentTrack, route.outputBusIndex, um);
            }
        }

        persisted.add (stored);
    }

    writeRoutesToPlugin (instrument, persisted, um);
    instrumentTrack.edit.restartPlayback();
    return true;
}

bool MultiOutputRouting::isMultiOutChildTrack (const te::Track& track)
{
    return track.state.hasProperty (childSourcePluginIdProperty);
}

te::EditItemID MultiOutputRouting::getSourcePluginIdForChildTrack (const te::Track& track)
{
    return te::EditItemID::fromProperty (track.state, childSourcePluginIdProperty);
}

void MultiOutputRouting::showConfigureOutputsDialog (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                                                     juce::Component* centreAround)
{
    MultiOutputConfigDialog::show (instrumentTrack, instrument, centreAround);
}

} // namespace arrange
