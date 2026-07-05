#include "LaneBackgroundCache.h"
#include "EditViewState.h"
#include "TimelineGrid.h"

namespace arrange
{

namespace
{
juce::String makeCacheKey (te::EditItemID trackId, EditViewState& viewState, juce::Rectangle<int> bounds)
{
    return juce::String (trackId.getRawID()) + "|"
         + juce::String (juce::roundToInt (viewState.getPixelsPerBeat() * 4.0)) + "|"
         + juce::String (juce::roundToInt (viewState.viewX1.get().inSeconds() * 4.0)) + "|"
         + juce::String (juce::roundToInt (viewState.viewX2.get().inSeconds() * 4.0)) + "|"
         + juce::String (bounds.getHeight()) + "|"
         + (viewState.showGrid.get() ? "1" : "0");
}
} // namespace

void LaneBackgroundCache::invalidateAll()
{
    images.clear();
}

void LaneBackgroundCache::invalidateTrack (te::EditItemID trackId)
{
    const auto prefix = juce::String (trackId.getRawID()) + "|";
    juce::StringArray keysToRemove;

    for (auto it = images.begin(); it != images.end(); ++it)
        if (it.getKey().startsWith (prefix))
            keysToRemove.add (it.getKey());

    for (const auto& key : keysToRemove)
        images.remove (key);
}

juce::Image LaneBackgroundCache::getCachedImage (te::EditItemID trackId,
                                                 double pixelsPerBeat,
                                                 te::TimePosition viewX1,
                                                 te::TimePosition viewX2,
                                                 int trackHeight,
                                                 bool showGrid)
{
    const juce::String key = juce::String (trackId.getRawID()) + "|"
                           + juce::String (juce::roundToInt (pixelsPerBeat * 4.0)) + "|"
                           + juce::String (juce::roundToInt (viewX1.inSeconds() * 4.0)) + "|"
                           + juce::String (juce::roundToInt (viewX2.inSeconds() * 4.0)) + "|"
                           + juce::String (trackHeight) + "|"
                           + (showGrid ? "1" : "0");

    if (images.contains (key))
        return images[key];

    return {};
}

void LaneBackgroundCache::ensureImage (te::Edit& edit,
                                       EditViewState& viewState,
                                       te::EditItemID trackId,
                                       juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    const auto key = makeCacheKey (trackId, viewState, bounds);
    if (images.contains (key))
        return;

    juce::Image image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics imgG (image);

    imgG.fillAll (juce::Colour (0xff0f0f23));
    imgG.setColour (juce::Colours::white.withAlpha (0.08f));
    imgG.drawHorizontalLine (bounds.getHeight() - 1, 0.0f, (float) bounds.getWidth());

    if (viewState.showGrid.get())
    {
        const auto fullArea = bounds.withPosition (0, 0);
        TimelineGrid::drawBarBackground (imgG, edit, viewState, fullArea);
        TimelineGrid::drawGridLines (imgG, edit, viewState, fullArea);
    }

    images.set (key, image);
}

void LaneBackgroundCache::renderOrFetch (juce::Graphics& g,
                                         te::Edit& edit,
                                         EditViewState& viewState,
                                         te::EditItemID trackId,
                                         juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    const auto key = makeCacheKey (trackId, viewState, bounds);

    if (images.contains (key))
    {
        g.drawImageAt (images.getReference (key), bounds.getX(), bounds.getY());
        return;
    }

    ensureImage (edit, viewState, trackId, bounds);

    if (images.contains (key))
        g.drawImageAt (images.getReference (key), bounds.getX(), bounds.getY());
}

} // namespace arrange
