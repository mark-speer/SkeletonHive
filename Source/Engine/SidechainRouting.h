#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct SidechainMatrixRow
{
    te::EditItemID pluginId;
    juce::String pluginName;
    juce::String hostTrackName;
    te::EditItemID hostTrackId;
    te::EditItemID currentSourceTrackId;
};

/** TE-native helpers for plugin sidechain routing (one source track per plugin). */
struct SidechainRouting
{
    using OpenMatrixCallback = std::function<void (te::Plugin*)>;

    /** Set from MainContentComponent to open the matrix panel for a plugin. */
    static OpenMatrixCallback openMatrixForPlugin;

    static juce::Array<SidechainMatrixRow> buildMatrix (te::Edit& edit);
    static juce::Array<te::AudioTrack*> getAllSourceTracks (te::Edit& edit);
    static juce::Array<te::AudioTrack*> getCandidateSources (te::Edit& edit, const te::Plugin& plugin);

    static bool hasActiveSidechain (const te::Plugin& plugin);
    static bool isSourceSelected (const te::Plugin& plugin, te::EditItemID sourceTrackId);
    static juce::String formatSourceMenuName (int oneBasedIndex, const juce::String& trackName);

    static void setSidechainSource (te::Plugin& plugin, te::AudioTrack* sourceOrNull);
    static void applyMatrix (te::Edit& edit, const juce::Array<SidechainMatrixRow>& rows);

    static te::Plugin* findPluginById (te::Edit& edit, te::EditItemID pluginId);
    static bool isTrackUsedAsSidechainSource (te::Edit& edit, const te::AudioTrack& track);
};

} // namespace skeletonhive
