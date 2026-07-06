#include "PluginPresetManager.h"

namespace skeletonhive
{

juce::ValueTree PluginPresetManager::capturePluginState (const te::Plugin& plugin)
{
    return plugin.state.createCopy();
}

bool PluginPresetManager::applyPluginState (te::Plugin& plugin, const juce::ValueTree& state)
{
    if (! state.isValid())
        return false;

    plugin.state.copyPropertiesAndChildrenFrom (state, &plugin.edit.getUndoManager());
    return true;
}

bool PluginPresetManager::savePresetToFile (const te::Plugin& plugin, const juce::File& file)
{
    const auto xml = capturePluginState (plugin).createXml();
    if (xml == nullptr)
        return false;

    return xml->writeTo (file);
}

bool PluginPresetManager::loadPresetFromFile (te::Plugin& plugin, const juce::File& file)
{
    const auto xml = juce::parseXML (file);
    if (xml == nullptr)
        return false;

    return applyPluginState (plugin, juce::ValueTree::fromXml (*xml));
}

} // namespace skeletonhive
