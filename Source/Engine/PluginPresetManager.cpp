#include "PluginPresetManager.h"
#include "EngineHelpers.h"

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

juce::File PluginPresetManager::getPresetLibraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("SkeletonHive")
               .getChildFile ("Presets");
}

juce::String PluginPresetManager::sanitisePathComponent (const juce::String& name)
{
    return name.replaceCharacters ("\\/:*?\"<>|", "_________").trim();
}

juce::File PluginPresetManager::getPresetFolderForPlugin (const juce::String& pluginIdentifier)
{
    return getPresetLibraryRoot().getChildFile (sanitisePathComponent (pluginIdentifier));
}

juce::StringArray PluginPresetManager::listCategories (const juce::String& pluginIdentifier)
{
    juce::StringArray categories;
    const auto pluginRoot = getPresetFolderForPlugin (pluginIdentifier);

    if (! pluginRoot.isDirectory())
        return categories;

    for (const auto& entry : juce::RangedDirectoryIterator (pluginRoot, false, "*", juce::File::findDirectories))
        categories.add (entry.getFile().getFileName());

    categories.sort (true);
    return categories;
}

juce::Array<PluginPresetEntry> PluginPresetManager::listPresets (const juce::String& pluginIdentifier,
                                                                 const juce::String& categoryFilter)
{
    juce::Array<PluginPresetEntry> result;
    const auto pluginRoot = getPresetFolderForPlugin (pluginIdentifier);

    if (! pluginRoot.isDirectory())
        return result;

    const auto collectFromCategory = [&] (const juce::File& categoryDir, const juce::String& categoryName)
    {
        for (const auto& entry : juce::RangedDirectoryIterator (categoryDir, false, "*.xml"))
        {
            PluginPresetEntry preset;
            preset.file = entry.getFile();
            preset.name = entry.getFile().getFileNameWithoutExtension();
            preset.category = categoryName;
            result.add (preset);
        }
    };

    if (categoryFilter.isNotEmpty())
    {
        const auto categoryDir = pluginRoot.getChildFile (categoryFilter);
        if (categoryDir.isDirectory())
            collectFromCategory (categoryDir, categoryFilter);
    }
    else
    {
        for (const auto& entry : juce::RangedDirectoryIterator (pluginRoot, false, "*", juce::File::findDirectories))
            collectFromCategory (entry.getFile(), entry.getFile().getFileName());
    }

    std::sort (result.begin(), result.end(), [] (const PluginPresetEntry& a, const PluginPresetEntry& b)
    {
        const int categoryCompare = a.category.compareIgnoreCase (b.category);
        return categoryCompare != 0 ? categoryCompare < 0 : a.name.compareIgnoreCase (b.name) < 0;
    });

    return result;
}

bool PluginPresetManager::saveNamedPreset (const te::Plugin& plugin, const juce::String& name,
                                           const juce::String& category)
{
    if (name.trim().isEmpty())
        return false;

    const auto categoryName = category.trim().isEmpty() ? juce::String ("User") : category.trim();
    const auto pluginId = EngineHelpers::getPluginDescription (plugin).createIdentifierString();
    const auto folder = getPresetFolderForPlugin (pluginId)
                            .getChildFile (sanitisePathComponent (categoryName));

    if (! folder.createDirectory())
        return false;

    const auto file = folder.getChildFile (sanitisePathComponent (name.trim()) + ".xml");
    return savePresetToFile (plugin, file);
}

bool PluginPresetManager::loadPreset (te::Plugin& plugin, const juce::File& presetFile)
{
    return loadPresetFromFile (plugin, presetFile);
}

bool PluginPresetManager::deletePreset (const juce::File& presetFile)
{
    if (presetFile.existsAsFile())
        return presetFile.deleteFile();

    return false;
}

} // namespace skeletonhive
