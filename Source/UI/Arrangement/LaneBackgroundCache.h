#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class EditViewState;

/** CPU cache of lane grid/bar backgrounds; invalidated on zoom or tempo/grid changes. */
class LaneBackgroundCache
{
public:
    void invalidateAll();
    void invalidateTrack (te::EditItemID trackId);

    juce::Image getCachedImage (te::EditItemID trackId,
                                double pixelsPerBeat,
                                te::TimePosition viewX1,
                                te::TimePosition viewX2,
                                int trackHeight,
                                bool showGrid);

    void renderOrFetch (juce::Graphics& g,
                        te::Edit& edit,
                        EditViewState& viewState,
                        te::EditItemID trackId,
                        juce::Rectangle<int> bounds);

    void ensureImage (te::Edit& edit,
                      EditViewState& viewState,
                      te::EditItemID trackId,
                      juce::Rectangle<int> bounds);

private:
    juce::HashMap<juce::String, juce::Image> images;
};

} // namespace arrange
