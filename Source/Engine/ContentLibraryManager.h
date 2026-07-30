#pragma once

#include "TracktionCommon.h"
#include "AppSettings.h"

namespace skeletonhive
{

enum class ContentSortMode
{
    name,
    dateModified,
    duration
};

enum class ContentFilterMode
{
    all,
    favorites,
    recent
};

struct ContentEntry
{
    juce::File file;
    juce::String displayName;
    juce::int64 modifiedTimeMs = 0;
    double lengthSeconds = 0.0;

    juce::String getKey() const { return file.getFullPathName(); }
};

/** Indexes sample files from configured library paths; persists favorites/recent. */
class ContentLibraryManager : public juce::ChangeBroadcaster
{
public:
    ContentLibraryManager (te::Engine& engine, AppSettings& settings);

    void setProjectFolder (const juce::File& folder);
    juce::File getProjectFolder() const { return projectFolder; }

    void rescanAll();
    bool isScanning() const { return scanning; }

    juce::Array<ContentEntry> getEntries (ContentFilterMode filter = ContentFilterMode::all,
                                          ContentSortMode sort = ContentSortMode::name,
                                          const juce::String& searchQuery = {},
                                          const juce::File& rootFilter = juce::File()) const;

    void addFavorite (const juce::File& file);
    void removeFavorite (const juce::File& file);
    bool isFavorite (const juce::File& file) const;

    void recordRecentUse (const juce::File& file);
    juce::StringArray getRecentlyUsed (int maxCount = 32) const;

    juce::Array<juce::File> getLibraryRoots() const;
    juce::Array<juce::File> getPlaceRoots() const;

private:
    class ScanJob;

    void loadState();
    void saveState();
    static bool isAudioFile (te::Engine& engine, const juce::File& file);
    void finishScan (juce::Array<ContentEntry> newEntries);

    te::Engine& engine;
    AppSettings& appSettings;

    juce::File projectFolder;
    juce::Array<ContentEntry> entries;
    juce::ThreadPool threadPool { 1 };
    std::atomic<bool> scanning { false };

    juce::File storageFile;
    juce::PropertiesFile properties;
};

} // namespace skeletonhive
