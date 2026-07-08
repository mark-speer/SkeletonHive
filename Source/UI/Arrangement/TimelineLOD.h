#pragma once

namespace skeletonhive
{

/** Below this zoom, clips are painted in the lane background instead of ClipComponents. */
constexpr double laneLevelPixelsPerBeatThreshold = 4.0;

inline bool useLaneLevelRendering (double pixelsPerBeat)
{
    return pixelsPerBeat <= laneLevelPixelsPerBeatThreshold;
}

/** Zoom-driven clip detail for the arrangement timeline. */
enum class TimelineClipDetailLevel
{
    Summary,  // solid blocks only — no waveforms, note previews, or fade curves
    Overview, // clip name + simplified content hints
    Detail    // full waveforms, MIDI previews, fade curves
};

/** Pick the coarsest level allowed by zoom and on-screen clip width. */
inline TimelineClipDetailLevel getClipDetailLevel (double pixelsPerBeat, int clipWidthPx)
{
    if (pixelsPerBeat >= 20.0 || clipWidthPx >= 48)
        return TimelineClipDetailLevel::Detail;

    if (pixelsPerBeat >= 8.0 || clipWidthPx >= 16)
        return TimelineClipDetailLevel::Overview;

    return TimelineClipDetailLevel::Summary;
}

inline bool shouldShowWaveforms (TimelineClipDetailLevel level, bool drawWaveformsPref)
{
    return drawWaveformsPref && level == TimelineClipDetailLevel::Detail;
}

inline bool shouldShowMidiPreview (TimelineClipDetailLevel level)
{
    return level == TimelineClipDetailLevel::Detail;
}

inline bool shouldShowMidiDensity (TimelineClipDetailLevel level)
{
    return level == TimelineClipDetailLevel::Overview;
}

inline bool shouldShowFadeCurves (TimelineClipDetailLevel level)
{
    return level == TimelineClipDetailLevel::Detail;
}

inline bool shouldShowFadeHandles (TimelineClipDetailLevel level)
{
    return level == TimelineClipDetailLevel::Detail;
}

inline bool shouldShowWarpMarkers (TimelineClipDetailLevel level)
{
    return level == TimelineClipDetailLevel::Detail;
}

inline bool shouldShowClipLabel (TimelineClipDetailLevel level, int clipWidthPx)
{
    if (level == TimelineClipDetailLevel::Summary)
        return clipWidthPx >= 28;

    return clipWidthPx >= 12;
}

inline bool shouldHoldWaveformThumbnail (TimelineClipDetailLevel level, bool drawWaveformsPref)
{
    return shouldShowWaveforms (level, drawWaveformsPref);
}

} // namespace skeletonhive
