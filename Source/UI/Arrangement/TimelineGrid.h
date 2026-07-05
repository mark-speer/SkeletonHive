#pragma once

#include "EditViewState.h"

namespace arrange
{

struct TimelineGrid
{
    static double beatsPerBar (const te::Edit& edit, te::TimePosition atTime);

    /** Grid interval in beats for the current division, evaluated at a given
        timeline position so bar-relative divisions follow time-signature changes. */
    static double gridIntervalBeats (const te::Edit& edit, const EditViewState& viewState,
                                     te::TimePosition atTime = {});

    /** Snaps a time to the grid, anchored to the start of the bar it falls in
        so snapping stays consistent with the drawn grid across tempo/time-sig changes. */
    static te::TimePosition snapTime (const te::Edit& edit, const EditViewState& viewState,
                                      te::TimePosition time, bool bypassSnap = false);

    /** area is in canvas coordinates (x == 0 is the timeline origin). Only the
        given area is painted, so callers should pass the clip/dirty region. */
    static void drawBarBackground (juce::Graphics& g, const te::Edit& edit, const EditViewState& viewState,
                                   juce::Rectangle<int> area);
    static void drawGridLines (juce::Graphics& g, const te::Edit& edit, const EditViewState& viewState,
                               juce::Rectangle<int> area);

    static void drawRuler (juce::Graphics& g, const te::Edit& edit, const EditViewState& viewState,
                           juce::Rectangle<int> area);

    static juce::String gridDivisionLabel (GridDivision division);
};

} // namespace arrange
