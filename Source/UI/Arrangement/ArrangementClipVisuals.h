#pragma once

#include "EditViewState.h"
#include "TimelineLOD.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

void paintClipStateOverlay (juce::Graphics& g, EditViewState& editViewState, te::Clip& clip,
                            juce::Rectangle<int> bounds, float cornerRadius);

float clipCornerRadius (TimelineClipDetailLevel detail, int clipWidthPx);

} // namespace skeletonhive
