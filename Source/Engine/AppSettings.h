#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

enum class ThemeChoice
{
    dark,
    light
};

enum class DefaultChainKind
{
    audioTrack,
    midiTrack
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

    juce::Array<juce::File> getSampleLibraryPaths() const;
    void setSampleLibraryPaths (const juce::Array<juce::File>& paths);
    void ensureDefaultSampleLibraryPaths();

    juce::StringArray getDefaultDeviceChain (DefaultChainKind kind) const;
    void setDefaultDeviceChain (DefaultChainKind kind, const juce::StringArray& pluginIdentifiers);

    void saveKeyMappings (const juce::ApplicationCommandManager& commandManager);
    void loadKeyMappings (juce::ApplicationCommandManager& commandManager);

    void saveIfNeeded();

private:
    juce::File storageFile;
    juce::PropertiesFile properties;
};

} // namespace skeletonhive
