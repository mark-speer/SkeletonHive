#pragma once

#include "TracktionCommon.h"

namespace arrange
{

/** Read-only view of a track's user-visible plugin chain (TE PluginList wrapper). */
class TrackPluginChainModel
{
public:
    explicit TrackPluginChainModel (te::AudioTrack& track);

    te::AudioTrack& getTrack() { return track; }

    juce::Array<te::Plugin*> getUserChainPlugins() const;
    int getUserChainSize() const;

    /** Maps a user-chain slot index to pluginList index. */
    int pluginListIndexForUserSlot (int userSlot) const;

    /** User-chain slot for a plugin, or -1. */
    int userSlotForPlugin (const te::Plugin& plugin) const;

    /** First user-chain slot that accepts an effect (after any instrument). */
    int firstEffectSlot() const;

    /** True if this track should host an instrument before effects. */
    bool expectsInstrumentFirst() const;

    /** Validates placement; returns pluginList insert index or -1 if rejected. */
    int resolveInsertIndex (int userSlot,
                            bool isInstrument,
                            const te::Plugin* movingExisting = nullptr) const;

private:
    te::AudioTrack& track;
};

} // namespace arrange
