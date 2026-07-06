#include "ContentLibraryManager.h"

namespace skeletonhive
{

namespace
{
constexpr int maxRecent = 32;

juce::PropertiesFile::Options makeContentOptions()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "SkeletonHive";
    opts.filenameSuffix = ".content";
    opts.osxLibrarySubFolder = "Application Support";
    opts.commonToAllUsers = false;
    opts.storageFormat = juce::PropertiesFile::storeAsXML;
    opts.millisecondsBeforeSaving = 500;
    return opts;
}

bool pathStartsWithRoot (const juce::File& file, const juce::File& root)
{
    if (! root.isDirectory())
        return false;

    return file.getFullPathName().startsWithIgnoreCase (root.getFullPathName());
}
} // namespace

class ContentLibraryManager::ScanJob : public juce::ThreadPoolJob
{
public:
    ScanJob (te::Engine& eng, juce::Array<juce::File> roots, std::function<void (juce::Array<ContentEntry>)> onDone)
        : juce::ThreadPoolJob ("ContentLibraryScan"),
          engine (eng),
          scanRoots (std::move (roots)),
          completion (std::move (onDone))
    {
    }

    juce::ThreadPoolJob::JobStatus runJob() override
    {
        juce::Array<ContentEntry> found;
        juce::HashMap<juce::String, bool> seen;

        for (const auto& root : scanRoots)
        {
            if (shouldExit())
                break;

            if (! root.isDirectory())
                continue;

            for (const auto& iter : juce::RangedDirectoryIterator (root, true, "*", juce::File::findFiles))
            {
                if (shouldExit())
                    break;

                const auto file = iter.getFile();

                if (! ContentLibraryManager::isAudioFile (engine, file))
                    continue;

                const auto key = file.getFullPathName();
                if (seen.contains (key))
                    continue;

                seen.set (key, true);

                ContentEntry entry;
                entry.file = file;
                entry.displayName = file.getFileNameWithoutExtension();
                entry.modifiedTimeMs = file.getLastModificationTime().toMilliseconds();

                te::AudioFile audioFile (engine, file);
                if (audioFile.isValid())
                    entry.lengthSeconds = audioFile.getLength();

                found.add (std::move (entry));
            }
        }

        if (completion != nullptr)
            juce::MessageManager::callAsync ([cb = std::move (completion), found = std::move (found)]() mutable
            {
                cb (std::move (found));
            });

        return jobHasFinished;
    }

private:
    te::Engine& engine;
    juce::Array<juce::File> scanRoots;
    std::function<void (juce::Array<ContentEntry>)> completion;
};

ContentLibraryManager::ContentLibraryManager (te::Engine& eng, AppSettings& settings)
    : engine (eng),
      appSettings (settings),
      storageFile (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("SkeletonHive")
                       .getChildFile ("content-state.content")),
      properties (storageFile, makeContentOptions())
{
    loadState();
    appSettings.ensureDefaultSampleLibraryPaths();
}

void ContentLibraryManager::setProjectFolder (const juce::File& folder)
{
    projectFolder = folder.getParentDirectory();
}

void ContentLibraryManager::loadState()
{
    juce::ignoreUnused (properties);
}

void ContentLibraryManager::saveState()
{
    properties.saveIfNeeded();
}

bool ContentLibraryManager::isAudioFile (te::Engine& eng, const juce::File& file)
{
    return eng.getAudioFileFormatManager().readFormatManager.findFormatForFileExtension (file.getFileExtension()) != nullptr;
}

void ContentLibraryManager::rescanAll()
{
    if (scanning.exchange (true))
        return;

    auto roots = getLibraryRoots();

    threadPool.addJob (new ScanJob (engine, std::move (roots), [this] (juce::Array<ContentEntry> scanned)
    {
        finishScan (std::move (scanned));
    }), true);
}

void ContentLibraryManager::finishScan (juce::Array<ContentEntry> newEntries)
{
    entries = std::move (newEntries);
    scanning = false;
    sendChangeMessage();
}

void ContentLibraryManager::addFavorite (const juce::File& file)
{
    if (! file.existsAsFile() || isFavorite (file))
        return;

    auto favs = juce::StringArray::fromTokens (properties.getValue ("favorites"), "|", "");
    favs.add (file.getFullPathName());
    properties.setValue ("favorites", favs.joinIntoString ("|"));
    saveState();
    sendChangeMessage();
}

void ContentLibraryManager::removeFavorite (const juce::File& file)
{
    auto favs = juce::StringArray::fromTokens (properties.getValue ("favorites"), "|", "");
    favs.removeString (file.getFullPathName());
    properties.setValue ("favorites", favs.joinIntoString ("|"));
    saveState();
    sendChangeMessage();
}

bool ContentLibraryManager::isFavorite (const juce::File& file) const
{
    return juce::StringArray::fromTokens (properties.getValue ("favorites"), "|", "")
               .contains (file.getFullPathName());
}

void ContentLibraryManager::recordRecentUse (const juce::File& file)
{
    if (! file.existsAsFile())
        return;

    auto recent = getRecentlyUsed (maxRecent);
    recent.removeString (file.getFullPathName());
    recent.insert (0, file.getFullPathName());

    while (recent.size() > maxRecent)
        recent.remove (recent.size() - 1);

    properties.setValue ("recent", recent.joinIntoString ("|"));
    saveState();
    sendChangeMessage();
}

juce::StringArray ContentLibraryManager::getRecentlyUsed (int maxCount) const
{
    auto recent = juce::StringArray::fromTokens (properties.getValue ("recent"), "|", "");

    while (recent.size() > maxCount)
        recent.remove (recent.size() - 1);

    return recent;
}

juce::Array<juce::File> ContentLibraryManager::getLibraryRoots() const
{
    return appSettings.getSampleLibraryPaths();
}

juce::Array<juce::File> ContentLibraryManager::getPlaceRoots() const
{
    juce::Array<juce::File> roots;

    if (projectFolder.isDirectory())
        roots.add (projectFolder);

    for (const auto& path : appSettings.getSampleLibraryPaths())
        roots.addIfNotAlreadyThere (path);

    return roots;
}

juce::Array<ContentEntry> ContentLibraryManager::getEntries (ContentFilterMode filter,
                                                             ContentSortMode sort,
                                                             const juce::String& searchQuery,
                                                             const juce::File& rootFilter) const
{
    juce::Array<ContentEntry> filtered;

    const auto recent = getRecentlyUsed (maxRecent);
    const auto favorites = juce::StringArray::fromTokens (properties.getValue ("favorites"), "|", "");
    const auto query = searchQuery.trim().toLowerCase();

    for (const auto& entry : entries)
    {
        if (rootFilter.isDirectory() && ! pathStartsWithRoot (entry.file, rootFilter))
            continue;

        if (query.isNotEmpty() && ! entry.displayName.toLowerCase().contains (query))
            continue;

        switch (filter)
        {
            case ContentFilterMode::favorites:
                if (! favorites.contains (entry.getKey()))
                    continue;
                break;

            case ContentFilterMode::recent:
            {
                bool inRecent = false;

                for (const auto& r : recent)
                {
                    if (entry.getKey().equalsIgnoreCase (r))
                    {
                        inRecent = true;
                        break;
                    }
                }

                if (! inRecent)
                    continue;
                break;
            }

            case ContentFilterMode::all:
            default:
                break;
        }

        filtered.add (entry);
    }

    struct Sorter
    {
        ContentSortMode mode;

        int compareElements (const ContentEntry& a, const ContentEntry& b) const
        {
            switch (mode)
            {
                case ContentSortMode::dateModified:
                    if (a.modifiedTimeMs == b.modifiedTimeMs) return 0;
                    return a.modifiedTimeMs > b.modifiedTimeMs ? -1 : 1;
                case ContentSortMode::duration:
                    if (a.lengthSeconds == b.lengthSeconds) return 0;
                    return a.lengthSeconds < b.lengthSeconds ? -1 : 1;
                case ContentSortMode::name:
                default:
                    return a.displayName.compareIgnoreCase (b.displayName);
            }
        }
    };

    Sorter sorter { sort };
    filtered.sort (sorter);
    return filtered;
}

} // namespace skeletonhive
