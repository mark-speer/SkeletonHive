#include "TimelineGrid.h"

namespace arrange
{

namespace
{
constexpr double minGridSpacingPx = 8.0;
constexpr int maxBarsToIterate = 100000;   // hard stop for degenerate zoom levels

double intervalForDivision (GridDivision division, double barBeats)
{
    switch (division)
    {
        case GridDivision::Bar:          return barBeats;
        case GridDivision::HalfBar:      return barBeats * 0.5;
        case GridDivision::Beat:         return 1.0;
        case GridDivision::HalfBeat:     return 0.5;
        case GridDivision::QuarterBeat:  return 0.25;
        case GridDivision::EighthBeat:   return 0.125;
        case GridDivision::SixteenthBeat: return 0.0625;
        case GridDivision::Auto:
        default:                         return 0.0;
    }
}

double adaptiveGridIntervalBeats (double pixelsPerBeat, double barBeats)
{
    static constexpr double candidates[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 0.0625 };

    for (int i = (int) (std::size (candidates)) - 1; i >= 0; --i)
    {
        const auto interval = juce::jmin (candidates[(size_t) i], barBeats);
        if (interval * pixelsPerBeat >= minGridSpacingPx)
            return interval;
    }

    return juce::jmax (0.0625, barBeats);
}

bool isNearlyInteger (double value, double epsilon = 1.0e-6)
{
    return std::abs (value - std::round (value)) < epsilon;
}
} // namespace

double TimelineGrid::beatsPerBar (const te::Edit& edit, te::TimePosition atTime)
{
    const auto& ts = edit.tempoSequence;
    const auto barsBeats = ts.toBarsAndBeats (atTime);
    const auto barStart = ts.toTime ({ barsBeats.bars, {} });
    const auto nextBar = ts.toTime ({ barsBeats.bars + 1, {} });
    return juce::jmax (1.0, (ts.toBeats (nextBar) - ts.toBeats (barStart)).inBeats());
}

double TimelineGrid::gridIntervalBeats (const te::Edit& edit, const EditViewState& viewState,
                                        te::TimePosition atTime)
{
    const auto barBeats = beatsPerBar (edit, atTime);
    const auto fixed = intervalForDivision (viewState.getGridDivision(), barBeats);
    if (fixed > 0.0)
        return fixed;

    return adaptiveGridIntervalBeats (viewState.getPixelsPerBeat(), barBeats);
}

te::TimePosition TimelineGrid::snapTime (const te::Edit& edit, const EditViewState& viewState,
                                         te::TimePosition time, bool bypassSnap)
{
    if (bypassSnap || juce::ModifierKeys::getCurrentModifiers().isAltDown())
        return time;

    if (! viewState.snapToGrid.get())
        return time;

    const auto& ts = edit.tempoSequence;

    // Anchor the snap grid to the start of the current bar so it matches
    // the drawn grid even across time-signature changes.
    const auto barsBeats = ts.toBarsAndBeats (time);
    const auto barStartBeat = ts.toBeats (ts.toTime ({ barsBeats.bars, {} })).inBeats();
    const auto beat = ts.toBeats (time).inBeats();
    const auto interval = gridIntervalBeats (edit, viewState, time);

    const auto relative = beat - barStartBeat;
    const auto snappedRelative = std::round (relative / interval) * interval;
    return ts.toTime (te::BeatPosition::fromBeats (barStartBeat + snappedRelative));
}

void TimelineGrid::drawBarBackground (juce::Graphics& g, const te::Edit& edit, const EditViewState& viewState,
                                      juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    const auto& ts = edit.tempoSequence;
    const int firstBar = juce::jmax (0, ts.toBarsAndBeats (viewState.xToTime (area.getX())).bars);

    for (int bar = firstBar; bar < firstBar + maxBarsToIterate; ++bar)
    {
        const int x1 = viewState.timeToX (ts.toTime ({ bar, {} }));
        const int x2 = viewState.timeToX (ts.toTime ({ bar + 1, {} }));

        if (x1 > area.getRight())
            break;
        if (x2 < area.getX() || x2 <= x1)
            continue;

        const bool shaded = (bar & 1) != 0;
        g.setColour (shaded ? juce::Colour (0xff141428) : juce::Colour (0xff10101f));
        g.fillRect (juce::Rectangle<int> (juce::jmax (area.getX(), x1), area.getY(),
                                          juce::jmin (area.getRight(), x2) - juce::jmax (area.getX(), x1),
                                          area.getHeight()));
    }
}

void TimelineGrid::drawGridLines (juce::Graphics& g, const te::Edit& edit, const EditViewState& viewState,
                                  juce::Rectangle<int> area)
{
    if (! viewState.showGrid.get() || area.isEmpty())
        return;

    const auto& ts = edit.tempoSequence;
    const int firstBar = juce::jmax (0, ts.toBarsAndBeats (viewState.xToTime (area.getX())).bars);

    for (int bar = firstBar; bar < firstBar + maxBarsToIterate; ++bar)
    {
        const auto barStartTime = ts.toTime ({ bar, {} });
        const int barX = viewState.timeToX (barStartTime);

        if (barX > area.getRight())
            break;

        const double barStartBeat = ts.toBeats (barStartTime).inBeats();
        const double barBeats = beatsPerBar (edit, barStartTime);
        const double interval = gridIntervalBeats (edit, viewState, barStartTime);

        for (double b = 0.0; b < barBeats - 1.0e-9; b += interval)
        {
            const int x = viewState.timeToX (ts.toTime (te::BeatPosition::fromBeats (barStartBeat + b)));
            if (x < area.getX() || x > area.getRight())
                continue;

            if (b == 0.0)
            {
                g.setColour (juce::Colours::white.withAlpha (0.45f));
                g.fillRect (x, area.getY(), 1, area.getHeight());
            }
            else if (isNearlyInteger (b))
            {
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
            }
            else
            {
                g.setColour (juce::Colours::white.withAlpha (0.08f));
                g.drawVerticalLine (x, (float) area.getY(), (float) area.getBottom());
            }
        }
    }
}

void TimelineGrid::drawRuler (juce::Graphics& g, const te::Edit& edit, const EditViewState& viewState,
                              juce::Rectangle<int> area)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine (area.getBottom() - 1, (float) area.getX(), (float) area.getRight());

    const auto& ts = edit.tempoSequence;
    const int width = area.getWidth();
    const auto viewStart = viewState.viewX1.get();
    const auto viewEnd = viewState.viewX2.get();
    const auto pixelsPerBeat = viewState.getPixelsPerBeat();

    const int firstBar = juce::jmax (0, ts.toBarsAndBeats (viewStart).bars);
    const int lastBar = ts.toBarsAndBeats (viewEnd).bars + 1;

    g.setFont (juce::FontOptions (11.0f));

    for (int bar = firstBar; bar <= lastBar; ++bar)
    {
        const auto barTime = ts.toTime ({ bar, {} });
        if (barTime < viewStart || barTime > viewEnd)
            continue;

        const int x = area.getX() + viewState.timeToXInView (barTime, width);

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.drawVerticalLine (x, (float) area.getY() + 2.0f, (float) area.getBottom() - 2.0f);

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (juce::String (bar + 1),
                    juce::Rectangle<int> (x + 4, area.getY(), 36, area.getHeight()),
                    juce::Justification::centredLeft, true);

        // Beat ticks within the bar when zoomed in enough (Live-style).
        const double barBeats = beatsPerBar (edit, barTime);
        const double barStartBeat = ts.toBeats (barTime).inBeats();

        if (barBeats * pixelsPerBeat >= 48.0)
        {
            const int beatsToDraw = (int) juce::jmin (barBeats, 16.0);
            for (int beat = 1; beat < beatsToDraw; ++beat)
            {
                const auto beatTime = ts.toTime (te::BeatPosition::fromBeats (barStartBeat + beat));
                if (beatTime < viewStart || beatTime > viewEnd)
                    continue;

                const int bx = area.getX() + viewState.timeToXInView (beatTime, width);
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.drawVerticalLine (bx, (float) area.getBottom() - 8.0f, (float) area.getBottom() - 2.0f);
            }
        }
    }
}

juce::String TimelineGrid::gridDivisionLabel (GridDivision division)
{
    switch (division)
    {
        case GridDivision::Auto:          return "Auto";
        case GridDivision::Bar:           return "1 Bar";
        case GridDivision::HalfBar:       return "1/2 Bar";
        case GridDivision::Beat:          return "1/4";
        case GridDivision::HalfBeat:      return "1/8";
        case GridDivision::QuarterBeat:   return "1/16";
        case GridDivision::EighthBeat:    return "1/32";
        case GridDivision::SixteenthBeat: return "1/64";
        default:                          return "Auto";
    }
}

} // namespace arrange
