#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Thin wrapper for plugin preset / state persistence via TE ValueTree. */
class PluginPresetManager
{
public:
    static juce::ValueTree capturePluginState (const te::Plugin& plugin);
    static bool applyPluginState (te::Plugin& plugin, const juce::ValueTree& state);

    static bool savePresetToFile (const te::Plugin& plugin, const juce::File& file);
    static bool loadPresetFromFile (te::Plugin& plugin, const juce::File& file);
};

} // namespace skeletonhive
