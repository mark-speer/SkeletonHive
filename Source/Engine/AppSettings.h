#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

enum class ThemeChoice
{
    dark,
    light
};

/** Application-wide preferences persisted outside any Edit. */
class AppSettings : public juce::ChangeBroadcaster
{
public:
    AppSettings();

    ThemeChoice getTheme() const;
    void setTheme (ThemeChoice theme);

    int getAutosaveIntervalSeconds() const;
    void setAutosaveIntervalSeconds (int seconds);

    juce::File getDefaultProjectFolder() const;
    void setDefaultProjectFolder (const juce::File& folder);

    void saveKeyMappings (const juce::ApplicationCommandManager& commandManager);
    void loadKeyMappings (juce::ApplicationCommandManager& commandManager);

    void saveIfNeeded();

private:
    juce::File storageFile;
    juce::PropertiesFile properties;
};

} // namespace skeletonhive
