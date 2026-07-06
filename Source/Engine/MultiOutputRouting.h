#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Describes one output bus on a multi-out instrument (TE ExternalPlugin / JUCE buses). */
struct OutputBusInfo
{
    int busIndex = 0;
    juce::String name;
    int numChannels = 0;
    bool isActive = false;
};

/** Persisted route from an instrument output bus to a child audio track. */
struct OutputRoute
{
    int outputBusIndex = 0;
    te::EditItemID childTrackId;
    bool enabled = false;
};

/** TE-native helpers for drum-sampler style multi-output routing to child tracks. */
struct MultiOutputRouting
{
    static const juce::Identifier routesProperty;
    static const juce::Identifier routeTypeProperty;
    static const juce::Identifier routeBusIndexProperty;
    static const juce::Identifier routeTrackIdProperty;
    static const juce::Identifier routeEnabledProperty;
    static const juce::Identifier childSourcePluginIdProperty;
    static const juce::Identifier childBusIndexProperty;
    static const juce::Identifier childParentTrackIdProperty;

    static bool isMultiOutputCapable (const te::Plugin& plugin);
    static juce::Array<OutputBusInfo> getOutputBuses (te::Plugin& plugin);
    static juce::Array<OutputRoute> getRoutes (const te::Plugin& plugin);

    /** Creates/updates child tracks and TE input routing for the given routes. Message thread only. */
    static bool applyRoutes (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                             const juce::Array<OutputRoute>& routes);

    static bool isMultiOutChildTrack (const te::Track& track);
    static te::EditItemID getSourcePluginIdForChildTrack (const te::Track& track);

    static void showConfigureOutputsDialog (te::AudioTrack& instrumentTrack, te::Plugin& instrument,
                                            juce::Component* centreAround);
};

} // namespace skeletonhive
