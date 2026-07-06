#include "SidechainRouting.h"
#include "TrackPluginChainModel.h"

namespace skeletonhive
{

SidechainRouting::OpenMatrixCallback SidechainRouting::openMatrixForPlugin;

te::Plugin* SidechainRouting::findPluginById (te::Edit& edit, te::EditItemID pluginId)
{
    if (! pluginId.isValid())
        return nullptr;

    for (auto* t : te::getAllTracks (edit))
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
            for (auto p : at->pluginList)
                if (p->itemID == pluginId)
                    return p;

    return nullptr;
}

juce::Array<SidechainMatrixRow> SidechainRouting::buildMatrix (te::Edit& edit)
{
    juce::Array<SidechainMatrixRow> rows;

    for (auto* t : te::getAudioTracks (edit))
    {
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
        {
            TrackPluginChainModel model (*at);

            for (auto* plugin : model.getUserChainPlugins())
            {
                if (plugin == nullptr || ! plugin->canSidechain())
                    continue;

                SidechainMatrixRow row;
                row.pluginId = plugin->itemID;
                row.pluginName = plugin->getName();
                row.hostTrackName = at->getName();
                row.hostTrackId = at->itemID;
                row.currentSourceTrackId = plugin->getSidechainSourceID();
                rows.add (row);
            }
        }
    }

    return rows;
}

juce::Array<te::AudioTrack*> SidechainRouting::getAllSourceTracks (te::Edit& edit)
{
    juce::Array<te::AudioTrack*> tracks;

    for (auto* t : te::getAudioTracks (edit))
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
            tracks.add (at);

    return tracks;
}

juce::Array<te::AudioTrack*> SidechainRouting::getCandidateSources (te::Edit& edit, const te::Plugin& plugin)
{
    juce::Array<te::AudioTrack*> sources;
    const auto* hostTrack = te::getTrackContainingPlugin (edit, &plugin);
    const te::EditItemID hostId = hostTrack != nullptr ? hostTrack->itemID : te::EditItemID {};

    for (auto* at : getAllSourceTracks (edit))
        if (at->itemID != hostId)
            sources.add (at);

    return sources;
}

bool SidechainRouting::hasActiveSidechain (const te::Plugin& plugin)
{
    return plugin.getSidechainSourceID().isValid();
}

bool SidechainRouting::isSourceSelected (const te::Plugin& plugin, te::EditItemID sourceTrackId)
{
    if (! sourceTrackId.isValid())
        return ! plugin.getSidechainSourceID().isValid();

    return plugin.getSidechainSourceID() == sourceTrackId;
}

juce::String SidechainRouting::formatSourceMenuName (int oneBasedIndex, const juce::String& trackName)
{
    return juce::String (oneBasedIndex) + ". " + trackName;
}

void SidechainRouting::setSidechainSource (te::Plugin& plugin, te::AudioTrack* sourceOrNull)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (sourceOrNull == nullptr)
    {
        plugin.setSidechainSourceByName ({});
        return;
    }

    const auto candidates = getCandidateSources (plugin.edit, plugin);

    for (int i = 0; i < candidates.size(); ++i)
    {
        if (candidates[i] == sourceOrNull)
        {
            plugin.setSidechainSourceByName (formatSourceMenuName (i + 1, sourceOrNull->getName()));

            if (plugin.getNumWires() == 0)
                plugin.guessSidechainRouting();

            return;
        }
    }
}

void SidechainRouting::applyMatrix (te::Edit& edit, const juce::Array<SidechainMatrixRow>& rows)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    const auto current = buildMatrix (edit);
    bool changed = false;

    for (const auto& row : rows)
    {
        if (auto* plugin = findPluginById (edit, row.pluginId))
        {
            const te::EditItemID existing = plugin->getSidechainSourceID();

            if (existing == row.currentSourceTrackId)
                continue;

            te::AudioTrack* source = te::findAudioTrackForID (edit, row.currentSourceTrackId);
            setSidechainSource (*plugin, source);
            changed = true;
        }
    }

    if (changed)
        edit.restartPlayback();
}

bool SidechainRouting::isTrackUsedAsSidechainSource (te::Edit& edit, const te::AudioTrack& track)
{
    for (const auto& row : buildMatrix (edit))
        if (row.currentSourceTrackId == track.itemID)
            return true;

    return false;
}

} // namespace skeletonhive
