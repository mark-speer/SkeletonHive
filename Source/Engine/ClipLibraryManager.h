#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

enum class ClipLibrarySortMode
{
    name,
    dateModified
};

struct ClipLibraryEntry
{
    juce::File presetFile;
    juce::String name;
    juce::String category;
    juce::String clipType;
    juce::int64 modifiedTimeMs = 0;
    double lengthSeconds = 0.0;

    juce::String getKey() const { return presetFile.getFullPathName(); }
};

/** Persists arrangement clip presets to the user clip library folder. */
class ClipLibraryManager : public juce::ChangeBroadcaster
{
public:
    explicit ClipLibraryManager (te::Engine& engine);

    juce::File getLibraryRoot() const;

    void refresh();
    juce::Array<ClipLibraryEntry> getEntries (const juce::String& searchQuery = {},
                                              ClipLibrarySortMode sort = ClipLibrarySortMode::name) const;

    /** Saves clip state (and referenced audio) to the library. Returns preset file on success. */
    juce::File saveClip (te::Clip& clip, const juce::String& name, const juce::String& category = "User");

    /** Instantiates a library preset on the given track at start time. */
    te::Clip* instantiateClip (te::ClipTrack& track, te::TimePosition start, const juce::File& presetFile);

    te::Clip* findClipById (te::Edit& edit, te::EditItemID clipId) const;

    bool deletePreset (const juce::File& presetFile);

private:
    static juce::ValueTree loadPresetTree (const juce::File& presetFile);
    static bool writePresetTree (const juce::File& presetFile, const juce::ValueTree& preset);
    static void normalizeClipStateForExport (juce::ValueTree& clipState, te::TimePosition clipStart);
    static void copyReferencedMedia (te::Clip& clip, const juce::File& presetFolder, juce::ValueTree& clipState);
    static void resolveMediaPaths (const juce::File& presetFolder, juce::ValueTree& clipState);
    static void replacePathInState (juce::ValueTree& state, const juce::String& oldPath, const juce::String& newPath);
    static juce::String clipTypeFor (const te::Clip& clip);

    te::Engine& engineRef;
    juce::Array<ClipLibraryEntry> entries;
};

} // namespace skeletonhive
