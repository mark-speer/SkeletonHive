#include "PluginStateManager.h"

namespace arrange
{

namespace
{
constexpr int maxRecent = 32;

juce::PropertiesFile::Options makeOptions (const juce::File& file)
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "ArrangeDAW";
    opts.filenameSuffix = ".plugins";
    opts.osxLibrarySubFolder = "Application Support";
    opts.commonToAllUsers = false;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    opts.millisecondsBeforeSaving = 500;
    juce::ignoreUnused (file);
    return opts;
}
} // namespace

PluginStateManager::PluginStateManager()
    : storageFile (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("ArrangeDAW")
                       .getChildFile ("plugin-state.plugins")),
      properties (storageFile, makeOptions (storageFile))
{
    load();
}

void PluginStateManager::load()
{
    juce::ignoreUnused (properties);
}

void PluginStateManager::save()
{
    properties.saveIfNeeded();
}

void PluginStateManager::addFavorite (const juce::String& identifier)
{
    if (identifier.isEmpty() || isFavorite (identifier))
        return;

    auto favs = getFavorites();
    favs.add (identifier);
    properties.setValue ("favorites", favs.joinIntoString ("|"));
    save();
}

void PluginStateManager::removeFavorite (const juce::String& identifier)
{
    auto favs = getFavorites();
    favs.removeString (identifier);
    properties.setValue ("favorites", favs.joinIntoString ("|"));
    save();
}

bool PluginStateManager::isFavorite (const juce::String& identifier) const
{
    return getFavorites().contains (identifier);
}

juce::StringArray PluginStateManager::getFavorites() const
{
    return juce::StringArray::fromTokens (properties.getValue ("favorites"), "|", "");
}

void PluginStateManager::recordRecentUse (const juce::String& identifier)
{
    if (identifier.isEmpty())
        return;

    auto recent = getRecentlyUsed (maxRecent);
    recent.removeString (identifier);
    recent.insert (0, identifier);

    while (recent.size() > maxRecent)
        recent.remove (recent.size() - 1);

    properties.setValue ("recent", recent.joinIntoString ("|"));
    save();
}

juce::StringArray PluginStateManager::getRecentlyUsed (int maxCount) const
{
    auto recent = juce::StringArray::fromTokens (properties.getValue ("recent"), "|", "");
    while (recent.size() > maxCount)
        recent.remove (recent.size() - 1);
    return recent;
}

void PluginStateManager::setClipboard (juce::ValueTree pluginState, juce::PluginDescription desc)
{
    clipboardState = pluginState.createCopy();
    clipboardDescription = std::move (desc);
}

bool PluginStateManager::hasClipboard() const
{
    return clipboardState.isValid() && clipboardDescription.name.isNotEmpty();
}

juce::ValueTree PluginStateManager::getClipboardState() const
{
    return clipboardState.createCopy();
}

juce::PluginDescription PluginStateManager::getClipboardDescription() const
{
    return clipboardDescription;
}

void PluginStateManager::clearClipboard()
{
    clipboardState = juce::ValueTree { "PluginClipboard" };
    clipboardDescription = {};
}

} // namespace arrange
