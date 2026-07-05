#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class PluginScanner
{
public:
    explicit PluginScanner (te::Engine& engine);
    ~PluginScanner();

    juce::KnownPluginList& getKnownPluginList();
    juce::AudioPluginFormatManager& getFormatManager();

    /** Scans every available plugin format in its default install locations. */
    bool scanDefaultLocations (std::function<void (int numFound)> onComplete);

    /** Scans a specific directory with every available plugin format. */
    bool scanPath (const juce::File& path, std::function<void (int numFound)> onComplete);

    te::Plugin::Ptr createPlugin (const juce::PluginDescription& desc, te::Edit& edit);

    bool isScanning() const { return scanning.load(); }
    juce::String getCurrentScanTarget() const;
    float getScanProgress() const;

private:
    te::Engine& engine;
    juce::ThreadPool scanPool { 1 };

    std::atomic<bool> scanning { false };
    mutable juce::CriticalSection scanStatusLock;
    juce::String currentScanTarget;
    float scanProgress = 0.0f;

    // An invalid (default) FileSearchPath means "use each format's default locations".
    bool performScan (const juce::FileSearchPath& explicitPath, bool useDefaultLocations,
                      std::function<void (int numFound)> onComplete);
};

} // namespace arrange
