#include "PluginScanner.h"

namespace arrange
{

PluginScanner::PluginScanner (te::Engine& e)
    : engine (e)
{
}

PluginScanner::~PluginScanner()
{
    engine.getPluginManager().abortCurrentPluginScan();
    scanPool.removeAllJobs (true, 5000);
    scanning = false;
}

juce::KnownPluginList& PluginScanner::getKnownPluginList()
{
    return engine.getPluginManager().knownPluginList;
}

juce::AudioPluginFormatManager& PluginScanner::getFormatManager()
{
    return engine.getPluginManager().pluginFormatManager;
}

juce::String PluginScanner::getCurrentScanTarget() const
{
    const juce::ScopedLock sl (scanStatusLock);
    return currentScanTarget;
}

float PluginScanner::getScanProgress() const
{
    const juce::ScopedLock sl (scanStatusLock);
    return scanProgress;
}

bool PluginScanner::scanDefaultLocations (std::function<void (int numFound)> onComplete)
{
    return performScan ({}, true, std::move (onComplete));
}

bool PluginScanner::scanPath (const juce::File& path, std::function<void (int numFound)> onComplete)
{
    return performScan (juce::FileSearchPath (path.getFullPathName()), false, std::move (onComplete));
}

bool PluginScanner::performScan (const juce::FileSearchPath& explicitPath, bool useDefaultLocations,
                                 std::function<void (int numFound)> onComplete)
{
    if (scanning.exchange (true))
        return false;

    const int typesBefore = getKnownPluginList().getNumTypes();

    // Retry plugins that earlier failed scans blacklisted, and drop stale
    // dead-man's-pedal entries so they aren't re-blacklisted immediately.
    getKnownPluginList().clearBlacklistedFiles();
    const auto deadMansPedal = engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal");
    deadMansPedal.deleteFile();

    {
        const juce::ScopedLock sl (scanStatusLock);
        currentScanTarget = "Scanning plugins...";
        scanProgress = 0.0f;
    }

    scanPool.addJob ([this, explicitPath, useDefaultLocations,
                      onComplete = std::move (onComplete), deadMansPedal, typesBefore]
    {
        const int numFormats = getFormatManager().getNumFormats();

        for (int i = 0; i < numFormats; ++i)
        {
            auto* format = getFormatManager().getFormat (i);

            const auto searchPath = useDefaultLocations ? format->getDefaultLocationsToSearch()
                                                        : explicitPath;
            if (searchPath.getNumPaths() == 0)
                continue;

            juce::PluginDirectoryScanner scanner (getKnownPluginList(),
                                                  *format,
                                                  searchPath,
                                                  true,
                                                  deadMansPedal,
                                                  true);

            juce::String name;
            while (scanner.scanNextFile (true, name))
            {
                const juce::ScopedLock sl (scanStatusLock);
                currentScanTarget = name.isNotEmpty() ? format->getName() + ": " + name
                                                      : scanner.getNextPluginFileThatWillBeScanned();
                scanProgress = ((float) i + scanner.getProgress()) / (float) juce::jmax (1, numFormats);
            }
        }

        const int result = numFormats > 0 ? getKnownPluginList().getNumTypes() - typesBefore : -1;

        juce::MessageManager::callAsync ([this, onComplete = std::move (onComplete), result]
        {
            scanning = false;
            getKnownPluginList().scanFinished();

            {
                const juce::ScopedLock sl (scanStatusLock);
                scanProgress = 1.0f;
            }

            if (onComplete)
                onComplete (result);
        });
    });

    return true;
}

te::Plugin::Ptr PluginScanner::createPlugin (const juce::PluginDescription& desc, te::Edit& edit)
{
    return edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);
}

} // namespace arrange
