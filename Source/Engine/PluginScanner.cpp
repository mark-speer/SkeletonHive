#include "PluginScanner.h"

namespace skeletonhive
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

juce::StringArray PluginScanner::getBlacklistedFiles() const
{
    return engine.getPluginManager().knownPluginList.getBlacklistedFiles();
}

juce::StringArray PluginScanner::readDeadMansPedal (const juce::File& pedalFile)
{
    juce::StringArray lines;
    if (pedalFile.existsAsFile())
        pedalFile.readLines (lines);

    lines.removeEmptyStrings();
    return lines;
}

void PluginScanner::blacklistRemainingPedalEntries (juce::KnownPluginList& list, const juce::File& pedalFile)
{
    for (const auto& crashed : readDeadMansPedal (pedalFile))
        if (! list.getBlacklistedFiles().contains (crashed))
            list.addToBlacklist (crashed);
}

bool PluginScanner::scanDefaultLocations (std::function<void (const PluginScanReport&)> onComplete)
{
    return performScan ({}, true, false, std::move (onComplete));
}

bool PluginScanner::scanPath (const juce::File& path, std::function<void (const PluginScanReport&)> onComplete)
{
    return performScan (juce::FileSearchPath (path.getFullPathName()), false, false, std::move (onComplete));
}

bool PluginScanner::rescanFailedPlugins (std::function<void (const PluginScanReport&)> onComplete)
{
    return performScan ({}, true, true, std::move (onComplete));
}

bool PluginScanner::rescanBlacklistedFile (const juce::String& fileOrIdentifier,
                                           std::function<void (const PluginScanReport&)> onComplete)
{
    if (scanning.exchange (true))
        return false;

    getKnownPluginList().removeFromBlacklist (fileOrIdentifier);

    const int typesBefore = getKnownPluginList().getNumTypes();
    const int blacklistedBefore = getKnownPluginList().getBlacklistedFiles().size();
    const auto deadMansPedal = engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal");

    {
        const juce::ScopedLock sl (scanStatusLock);
        currentScanTarget = fileOrIdentifier;
        scanProgress = 0.0f;
    }

    scanPool.addJob ([this, fileOrIdentifier, onComplete = std::move (onComplete),
                      deadMansPedal, typesBefore, blacklistedBefore]
    {
        PluginScanReport report;
        report.skippedBlacklisted = blacklistedBefore;

        const int numFormats = getFormatManager().getNumFormats();

        for (int i = 0; i < numFormats; ++i)
        {
            auto* format = getFormatManager().getFormat (i);
            juce::OwnedArray<juce::PluginDescription> typesFound;

            if (getKnownPluginList().scanAndAddFile (fileOrIdentifier, false, typesFound, *format))
                report.newPluginsFound += typesFound.size();
        }

        blacklistRemainingPedalEntries (getKnownPluginList(), deadMansPedal);

        for (const auto& failed : readDeadMansPedal (deadMansPedal))
        {
            if (! report.newlyFailedFiles.contains (failed))
            {
                report.newlyFailedFiles.add (failed);
                ++report.newlyFailed;
            }
        }

        report.newPluginsFound = juce::jmax (report.newPluginsFound,
                                             getKnownPluginList().getNumTypes() - typesBefore);
        report.totalPlugins = getKnownPluginList().getNumTypes();

        juce::MessageManager::callAsync ([this, onComplete = std::move (onComplete), report]
        {
            scanning = false;
            getKnownPluginList().scanFinished();

            {
                const juce::ScopedLock sl (scanStatusLock);
                scanProgress = 1.0f;
            }

            if (onComplete)
                onComplete (report);
        });
    });

    return true;
}

bool PluginScanner::performScan (const juce::FileSearchPath& explicitPath,
                                 bool useDefaultLocations,
                                 bool clearBlacklistFirst,
                                 std::function<void (const PluginScanReport&)> onComplete)
{
    if (scanning.exchange (true))
        return false;

    if (clearBlacklistFirst)
        getKnownPluginList().clearBlacklistedFiles();

    const int typesBefore = getKnownPluginList().getNumTypes();
    const int blacklistedBefore = getKnownPluginList().getBlacklistedFiles().size();
    const auto deadMansPedal = engine.getTemporaryFileManager().getTempFile ("PluginScanDeadMansPedal");

    {
        const juce::ScopedLock sl (scanStatusLock);
        currentScanTarget = "Scanning plugins...";
        scanProgress = 0.0f;
    }

    scanPool.addJob ([this, explicitPath, useDefaultLocations, clearBlacklistFirst,
                      onComplete = std::move (onComplete), deadMansPedal,
                      typesBefore, blacklistedBefore]
    {
        const int numFormats = getFormatManager().getNumFormats();
        juce::StringArray failedDuringScan;

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

            for (const auto& failed : scanner.getFailedFiles())
                if (! failedDuringScan.contains (failed))
                    failedDuringScan.add (failed);
        }

        blacklistRemainingPedalEntries (getKnownPluginList(), deadMansPedal);

        PluginScanReport report;
        report.newPluginsFound = getKnownPluginList().getNumTypes() - typesBefore;
        report.skippedBlacklisted = clearBlacklistFirst ? 0 : blacklistedBefore;
        report.totalPlugins = getKnownPluginList().getNumTypes();

        for (const auto& failed : readDeadMansPedal (deadMansPedal))
        {
            if (! report.newlyFailedFiles.contains (failed))
            {
                report.newlyFailedFiles.add (failed);
                ++report.newlyFailed;
            }
        }

        for (const auto& failed : failedDuringScan)
        {
            if (! report.newlyFailedFiles.contains (failed))
            {
                report.newlyFailedFiles.add (failed);
                ++report.newlyFailed;
            }
        }

        juce::MessageManager::callAsync ([this, onComplete = std::move (onComplete), report]
        {
            scanning = false;
            getKnownPluginList().scanFinished();

            {
                const juce::ScopedLock sl (scanStatusLock);
                scanProgress = 1.0f;
            }

            if (onComplete)
                onComplete (report);
        });
    });

    return true;
}

te::Plugin::Ptr PluginScanner::createPlugin (const juce::PluginDescription& desc, te::Edit& edit)
{
    return edit.getPluginCache().createNewPlugin (te::ExternalPlugin::xmlTypeName, desc);
}

} // namespace skeletonhive
