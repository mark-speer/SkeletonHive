#include "LaneClipSummaryPaint.h"
#include "TimelineGrid.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

namespace
{
constexpr int laneContentInsetY = 2;
constexpr int minPerClipWidthPx = 2;
constexpr int barStripHeightPx = 3;
constexpr int maxBarsToIterate = 100000;

enum class BarBucketKind
{
    empty,
    audio,
    midi,
    mixed
};

juce::Colour fillColourForClip (const te::Clip& clip)
{
    if (dynamic_cast<const te::WaveAudioClip*> (&clip) != nullptr)
        return juce::Colour (0xff2d6a4f);

    if (dynamic_cast<const te::MidiClip*> (&clip) != nullptr)
        return juce::Colour (0xff4361ee);

    return juce::Colours::darkgrey;
}

bool isAudioClip (const te::Clip& clip)
{
    return dynamic_cast<const te::WaveAudioClip*> (&clip) != nullptr;
}

bool isMidiClip (const te::Clip& clip)
{
    return dynamic_cast<const te::MidiClip*> (&clip) != nullptr;
}

void mergeBarBucket (BarBucketKind& bucket, const te::Clip& clip)
{
    const bool audio = isAudioClip (clip);
    const bool midi = isMidiClip (clip);

    if (bucket == BarBucketKind::empty)
    {
        if (audio)
            bucket = BarBucketKind::audio;
        else if (midi)
            bucket = BarBucketKind::midi;
        return;
    }

    if (bucket == BarBucketKind::audio && midi)
        bucket = BarBucketKind::mixed;
    else if (bucket == BarBucketKind::midi && audio)
        bucket = BarBucketKind::mixed;
}

juce::Colour colourForBarBucket (BarBucketKind kind)
{
    switch (kind)
    {
        case BarBucketKind::audio: return juce::Colour (0xff2d6a4f);
        case BarBucketKind::midi:  return juce::Colour (0xff4361ee);
        case BarBucketKind::mixed: return juce::Colour (0xff888888);
        default:                   return juce::Colours::transparentBlack;
    }
}

void paintClipBlock (juce::Graphics& g, EditViewState& editViewState, te::Clip& clip,
                     juce::Rectangle<int> bounds)
{
    if (bounds.isEmpty())
        return;

    constexpr float corner = 2.0f;
    g.setColour (fillColourForClip (clip));
    g.fillRoundedRectangle (bounds.toFloat(), corner);

    if (editViewState.selectionManager.isSelected (&clip))
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (bounds.toFloat().reduced (0.5f), corner, 2.0f);
    }

    if (EngineHelpers::getClipGroup (clip).isNotEmpty())
    {
        g.setColour (EngineHelpers::getClipGroupColour (clip));
        g.fillRect (bounds.getRight() - 10, bounds.getY(), 10, 6);
    }
}

te::TimeRange visibleTimeRange (const EditViewState& editViewState, int marginPx)
{
    const int visibleStartX = editViewState.timeToX (editViewState.viewX1.get()) - marginPx;
    const int visibleEndX = editViewState.timeToX (editViewState.viewX2.get()) + marginPx;
    return { editViewState.xToTime (visibleStartX), editViewState.xToTime (visibleEndX) };
}

bool clipIntersectsRange (const te::Clip& clip, const te::TimeRange& range)
{
    const auto pos = clip.getPosition();
    return pos.getEnd() > range.getStart() && pos.getStart() < range.getEnd();
}
} // namespace

void paintLaneClipSummaries (juce::Graphics& g, EditViewState& editViewState,
                             te::ClipTrack& clipTrack, juce::Rectangle<int> laneBounds)
{
    if (laneBounds.isEmpty())
        return;

    const int height = laneBounds.getHeight();
    const int contentHeight = juce::jmax (1, height - laneContentInsetY * 2);
    const int visibleStartX = editViewState.timeToX (editViewState.viewX1.get());
    const int visibleEndX = editViewState.timeToX (editViewState.viewX2.get());
    const int margin = juce::jmax (200, (visibleEndX - visibleStartX) / 2);
    const auto visibleRange = visibleTimeRange (editViewState, margin);

    const auto& ts = editViewState.edit.tempoSequence;
    const int firstBar = juce::jmax (0, ts.toBarsAndBeats (visibleRange.getStart()).bars);
    const int lastBar = ts.toBarsAndBeats (visibleRange.getEnd()).bars;

    for (auto* clipPtr : clipTrack.getClips())
    {
        if (clipPtr == nullptr || ! clipIntersectsRange (*clipPtr, visibleRange))
            continue;

        const auto pos = clipPtr->getPosition();
        const int x = editViewState.timeToX (pos.getStart());
        const int x2 = editViewState.timeToX (pos.getEnd());
        const int clipWidth = juce::jmax (1, x2 - x);

        if (clipWidth >= minPerClipWidthPx)
        {
            auto bounds = juce::Rectangle<int> (x, laneBounds.getY() + laneContentInsetY,
                                                clipWidth, contentHeight);
            bounds = bounds.getIntersection (laneBounds);
            paintClipBlock (g, editViewState, *clipPtr, bounds);
        }
    }

    const int stripY = laneBounds.getBottom() - laneContentInsetY - barStripHeightPx;

    for (int bar = firstBar; bar < firstBar + maxBarsToIterate; ++bar)
    {
        if (bar > lastBar)
            break;

        const int x1 = editViewState.timeToX (ts.toTime ({ bar, {} }));
        const int x2 = editViewState.timeToX (ts.toTime ({ bar + 1, {} }));

        if (x1 > laneBounds.getRight())
            break;
        if (x2 < laneBounds.getX() || x2 <= x1)
            continue;

        const auto barStart = ts.toTime ({ bar, {} });
        const auto barEnd = ts.toTime ({ bar + 1, {} });
        BarBucketKind kind = BarBucketKind::empty;

        for (auto* clipPtr : clipTrack.getClips())
        {
            if (clipPtr == nullptr)
                continue;

            const auto pos = clipPtr->getPosition();
            if (pos.getEnd() <= barStart || pos.getStart() >= barEnd)
                continue;

            const int clipWidth = juce::jmax (1, editViewState.timeToX (pos.getEnd())
                                                 - editViewState.timeToX (pos.getStart()));
            if (clipWidth >= minPerClipWidthPx)
                continue;

            mergeBarBucket (kind, *clipPtr);
        }

        if (kind == BarBucketKind::empty)
            continue;

        auto barRect = juce::Rectangle<int> (juce::jmax (laneBounds.getX(), x1), stripY,
                                              juce::jmax (1, juce::jmin (laneBounds.getRight(), x2)
                                                              - juce::jmax (laneBounds.getX(), x1)),
                                              barStripHeightPx);
        g.setColour (colourForBarBucket (kind));
        g.fillRect (barRect);
    }
}

te::Clip* findClipAtX (EditViewState& editViewState, te::ClipTrack& clipTrack, int x)
{
    const int visibleStartX = editViewState.timeToX (editViewState.viewX1.get());
    const int visibleEndX = editViewState.timeToX (editViewState.viewX2.get());
    const int margin = juce::jmax (200, (visibleEndX - visibleStartX) / 2);
    const auto visibleRange = visibleTimeRange (editViewState, margin);
    const auto time = editViewState.xToTime (x);

    te::Clip* bestMatch = nullptr;

    for (auto* clipPtr : clipTrack.getClips())
    {
        if (clipPtr == nullptr || ! clipIntersectsRange (*clipPtr, visibleRange))
            continue;

        const auto pos = clipPtr->getPosition();
        if (time < pos.getStart() || time >= pos.getEnd())
            continue;

        bestMatch = clipPtr;
    }

    return bestMatch;
}

} // namespace skeletonhive
