#include "TrackPluginChainModel.h"
#include "EngineHelpers.h"

namespace skeletonhive
{

TrackPluginChainModel::TrackPluginChainModel (te::Track& t)
    : track (t)
{
}

juce::Array<te::Plugin*> TrackPluginChainModel::getUserChainPlugins() const
{
    juce::Array<te::Plugin*> chain;
    for (auto p : track.pluginList)
        if (EngineHelpers::isFooterVisiblePlugin (*p))
            chain.add (p);
    return chain;
}

int TrackPluginChainModel::getUserChainSize() const
{
    return getUserChainPlugins().size();
}

int TrackPluginChainModel::pluginListIndexForUserSlot (int userSlot) const
{
    const auto chain = getUserChainPlugins();
    if (! juce::isPositiveAndBelow (userSlot, chain.size()))
        return EngineHelpers::getUserChainInsertIndex (track);

    return track.pluginList.indexOf (chain[userSlot]);
}

int TrackPluginChainModel::userSlotForPlugin (const te::Plugin& plugin) const
{
    const auto chain = getUserChainPlugins();
    for (int i = 0; i < chain.size(); ++i)
        if (chain[i] == &plugin)
            return i;
    return -1;
}

int TrackPluginChainModel::firstEffectSlot() const
{
    const auto chain = getUserChainPlugins();
    for (int i = 0; i < chain.size(); ++i)
        if (! EngineHelpers::isInstrumentPlugin (*chain[i]))
            return i;

    return chain.size();
}

bool TrackPluginChainModel::expectsInstrumentFirst() const
{
    if (track.isMasterTrack())
        return false;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (&track))
        return EngineHelpers::canHostMidiClips (*audioTrack) || EngineHelpers::isMidiTrack (*audioTrack);

    return false;
}

int TrackPluginChainModel::resolveInsertIndex (int userSlot,
                                               bool isInstrument,
                                               const te::Plugin* movingExisting) const
{
    const auto chain = getUserChainPlugins();
    const int clampedSlot = juce::jlimit (0, chain.size(), userSlot);
    int insertIndex = pluginListIndexForUserSlot (clampedSlot);

    if (movingExisting != nullptr)
    {
        const int existingListIndex = track.pluginList.indexOf (movingExisting);
        if (existingListIndex >= 0 && existingListIndex < insertIndex)
            --insertIndex;
    }

    if (isInstrument)
    {
        if (! expectsInstrumentFirst())
            return -1;

        return EngineHelpers::getUserChainInsertIndex (track);
    }

    if (track.isMasterTrack() && movingExisting == nullptr)
    {
        // New plugins on master must be allowed by TE.
        // Placement is still validated at insert time via canContainPlugin.
    }

    if (! expectsInstrumentFirst())
        return insertIndex;

    const int instrumentSlot = EngineHelpers::findInstrumentSlot (track);
    if (instrumentSlot < 0)
        return insertIndex;

    const int minListIndex = pluginListIndexForUserSlot (instrumentSlot + 1);
    return juce::jmax (insertIndex, minListIndex);
}

} // namespace skeletonhive
