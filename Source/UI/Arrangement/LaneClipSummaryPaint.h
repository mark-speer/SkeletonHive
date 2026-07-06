#pragma once

#include "EditViewState.h"

namespace skeletonhive
{

/** Paints clip summaries directly into a lane at extreme zoom (lane-level LOD). */
void paintLaneClipSummaries (juce::Graphics& g,
                             EditViewState& editViewState,
                             te::ClipTrack& clipTrack,
                             juce::Rectangle<int> laneBounds);

/** Returns the clip under canvas X, or nullptr. Message thread only. */
te::Clip* findClipAtX (EditViewState& editViewState, te::ClipTrack& clipTrack, int x);

} // namespace skeletonhive
