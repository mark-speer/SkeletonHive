#pragma once

#include "EditViewState.h"

namespace skeletonhive
{

class TrackLaneComponent;

/** Clip selection gestures for the arrangement view (Tracktion SelectionManager). */
struct ArrangementSelectionHelpers
{
    static void handleClipClick (EditViewState& editViewState, te::Clip& clip, const juce::ModifierKeys& mods);

    static void selectClipsInRect (EditViewState& editViewState,
                                   const juce::Rectangle<int>& rectInTimelineContent,
                                   const juce::OwnedArray<TrackLaneComponent>& lanes);
};

} // namespace skeletonhive
