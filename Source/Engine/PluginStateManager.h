#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Persists favorites, recently-used plugins, and clipboard outside the Edit. */
class PluginStateManager
{
public:
    PluginStateManager();

    void addFavorite (const juce::String& identifier);
    void removeFavorite (const juce::String& identifier);
    bool isFavorite (const juce::String& identifier) const;
    juce::StringArray getFavorites() const;

    void recordRecentUse (const juce::String& identifier);
    juce::StringArray getRecentlyUsed (int maxCount = 16) const;

    void setClipboard (juce::ValueTree pluginState, juce::PluginDescription desc);
    bool hasClipboard() const;
    juce::ValueTree getClipboardState() const;
    juce::PluginDescription getClipboardDescription() const;
    void clearClipboard();

private:
    void save();
    void load();

    juce::File storageFile;
    juce::PropertiesFile properties;
    juce::ValueTree clipboardState { "PluginClipboard" };
    juce::PluginDescription clipboardDescription;
};

} // namespace skeletonhive
