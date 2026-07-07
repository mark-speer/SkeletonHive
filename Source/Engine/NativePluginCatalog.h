#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct NativePluginEntry
{
    const char* displayName = nullptr;
    const char* xmlTypeName = nullptr;
    const char* category = nullptr;
    bool isInstrument = false;
};

/** Registry of Tracktion Engine built-in plugins exposed in the SkeletonHive browser. */
class NativePluginCatalog
{
public:
    static constexpr const char* identifierPrefix = "skeletonhive.native:";

    static const juce::Array<NativePluginEntry>& getEntries();

    static juce::Array<juce::PluginDescription> getAllDescriptions();
    static juce::PluginDescription makeDescription (const NativePluginEntry& entry);

    static bool isNativeDescription (const juce::PluginDescription& desc);
    static juce::String xmlTypeNameFromDescription (const juce::PluginDescription& desc);
    static juce::PluginDescription lookupDescription (const juce::String& identifierString);
    static juce::PluginDescription descriptionForPlugin (const te::Plugin& plugin);

    static te::Plugin::Ptr createPlugin (te::Edit& edit, const juce::PluginDescription& desc);
    static bool isNativeInstrumentPlugin (const te::Plugin& plugin);
    static bool isNativePlugin (const te::Plugin& plugin);
};

} // namespace skeletonhive
