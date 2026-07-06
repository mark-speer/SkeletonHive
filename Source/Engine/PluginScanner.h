#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct PluginScanReport
{
    int newPluginsFound = 0;
    int skippedBlacklisted = 0;
    int newlyFailed = 0;
    int totalPlugins = 0;
    juce::StringArray newlyFailedFiles;
};

class PluginScanner
{
public:
    explicit PluginScanner (te::Engine& engine);
    ~PluginScanner();

    juce::KnownPluginList& getKnownPluginList();
    juce::AudioPluginFormatManager& getFormatManager();

    /** Scans every available plugin format in its default install locations. */
    bool scanDefaultLocations (std::function<void (const PluginScanReport&)> onComplete);

    /** Scans a specific directory with every available plugin format. */
    bool scanPath (const juce::File& path, std::function<void (const PluginScanReport&)> onComplete);

    /** Clears the blacklist and rescans all default plugin locations. */
    bool rescanFailedPlugins (std::function<void (const PluginScanReport&)> onComplete);

    /** Removes one file from the blacklist and rescans it. */
    bool rescanBlacklistedFile (const juce::String& fileOrIdentifier,
                                std::function<void (const PluginScanReport&)> onComplete);

    te::Plugin::Ptr createPlugin (const juce::PluginDescription& desc, te::Edit& edit);

    bool isScanning() const { return scanning.load(); }
    juce::String getCurrentScanTarget() const;
    float getScanProgress() const;

    juce::StringArray getBlacklistedFiles() const;

private:
    te::Engine& engine;
    juce::ThreadPool scanPool { 1 };

    std::atomic<bool> scanning { false };
    mutable juce::CriticalSection scanStatusLock;
    juce::String currentScanTarget;
    float scanProgress = 0.0f;

    bool performScan (const juce::FileSearchPath& explicitPath,
                      bool useDefaultLocations,
                      bool clearBlacklistFirst,
                      std::function<void (const PluginScanReport&)> onComplete);

    static juce::StringArray readDeadMansPedal (const juce::File& pedalFile);
    static void blacklistRemainingPedalEntries (juce::KnownPluginList& list, const juce::File& pedalFile);
};

} // namespace skeletonhive
