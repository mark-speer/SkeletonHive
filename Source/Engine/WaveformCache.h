#pragma once

#include "TracktionCommon.h"

namespace arrange
{

/** Shared LRU cache of te::SmartThumbnail instances keyed by audio file identity. */
class WaveformCache
{
public:
    static constexpr int defaultMaxEntries = 96;

    explicit WaveformCache (int maxEntries = defaultMaxEntries);

    std::shared_ptr<te::SmartThumbnail> acquire (te::Engine& engine,
                                                 const te::AudioFile& file,
                                                 juce::Component& repaintTarget,
                                                 te::Edit* edit);

    void suggestEviction (const te::AudioFile& file);
    void clear();

private:
    struct Entry
    {
        std::shared_ptr<te::SmartThumbnail> thumbnail;
        juce::int64 lastUsedMs = 0;
    };

    juce::int64 keyForFile (const te::AudioFile& file) const;
    void trimToSize();

    int maxEntries;
    juce::HashMap<juce::int64, Entry> entries;
};

} // namespace arrange
