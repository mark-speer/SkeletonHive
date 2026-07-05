#include "WaveformCache.h"

namespace arrange
{

WaveformCache::WaveformCache (int maxEntriesIn)
    : maxEntries (juce::jmax (8, maxEntriesIn))
{
}

juce::int64 WaveformCache::keyForFile (const te::AudioFile& file) const
{
    return (juce::int64) file.getHash();
}

std::shared_ptr<te::SmartThumbnail> WaveformCache::acquire (te::Engine& engine,
                                                              const te::AudioFile& file,
                                                              juce::Component& repaintTarget,
                                                              te::Edit* edit)
{
    if (! file.isValid())
        return {};

    const auto key = keyForFile (file);
    const auto now = (juce::int64) juce::Time::getMillisecondCounterHiRes();

    if (entries.contains (key))
    {
        entries.getReference (key).lastUsedMs = now;
        return entries.getReference (key).thumbnail;
    }

    auto thumbnail = std::make_shared<te::SmartThumbnail> (engine, file, repaintTarget, edit);
    entries.set (key, { thumbnail, now });
    trimToSize();
    return thumbnail;
}

void WaveformCache::suggestEviction (const te::AudioFile& file)
{
    if (! file.isValid())
        return;

    const auto key = keyForFile (file);

    if (entries.contains (key))
    {
        auto& entry = entries.getReference (key);

        if (entry.thumbnail.use_count() <= 1)
        {
            entry.thumbnail->releaseFile();
            entries.remove (key);
        }
    }
}

void WaveformCache::clear()
{
    for (auto it = entries.begin(); it != entries.end(); ++it)
        if (it.getValue().thumbnail != nullptr)
            it.getValue().thumbnail->releaseFile();

    entries.clear();
}

void WaveformCache::trimToSize()
{
    while (entries.size() > maxEntries)
    {
        juce::int64 oldestKey = 0;
        juce::int64 oldestTime = std::numeric_limits<juce::int64>::max();

        for (auto it = entries.begin(); it != entries.end(); ++it)
        {
            if (it.getValue().lastUsedMs < oldestTime && it.getValue().thumbnail.use_count() <= 1)
            {
                oldestTime = it.getValue().lastUsedMs;
                oldestKey = it.getKey();
            }
        }

        if (oldestTime == std::numeric_limits<juce::int64>::max())
            break;

        if (entries.contains (oldestKey))
        {
            entries.getReference (oldestKey).thumbnail->releaseFile();
            entries.remove (oldestKey);
        }
        else
        {
            break;
        }
    }
}

} // namespace arrange
