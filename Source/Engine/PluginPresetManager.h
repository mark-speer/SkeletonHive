#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct PluginPresetEntry
{
    juce::File file;
    juce::String name;
    juce::String category;
};

/** Thin wrapper for plugin preset / state persistence via TE ValueTree. */
class PluginPresetManager
{
public:
    static juce::ValueTree capturePluginState (const te::Plugin& plugin);
    static bool applyPluginState (te::Plugin& plugin, const juce::ValueTree& state);

    static bool savePresetToFile (const te::Plugin& plugin, const juce::File& file);
    static bool loadPresetFromFile (te::Plugin& plugin, const juce::File& file);

    static juce::File getPresetLibraryRoot();
    static juce::File getPresetFolderForPlugin (const juce::String& pluginIdentifier);
    static juce::Array<PluginPresetEntry> listPresets (const juce::String& pluginIdentifier,
                                                       const juce::String& categoryFilter = {});
    static juce::StringArray listCategories (const juce::String& pluginIdentifier);
    static bool saveNamedPreset (const te::Plugin& plugin, const juce::String& name,
                                 const juce::String& category);
    static bool loadPreset (te::Plugin& plugin, const juce::File& presetFile);
    static bool deletePreset (const juce::File& presetFile);

private:
    static juce::String sanitisePathComponent (const juce::String& name);
};

} // namespace skeletonhive
