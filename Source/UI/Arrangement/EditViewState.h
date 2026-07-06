#pragma once

#include "TracktionCommon.h"
#include "Engine/WaveformCache.h"
#include "LaneBackgroundCache.h"

namespace skeletonhive
{

enum class MainView
{
    arrangement = 0,
    session
};

enum class SceneLaunchMode
{
    stopOthers = 0,
    additive
};

enum class LaunchQuantization
{
    none = 0,
    bar,
    halfBar,
    beat,
    halfBeat,
    eighthBeat
};

enum class GridDivision
{
    Auto = 0,
    Bar,
    HalfBar,
    Beat,
    HalfBeat,
    QuarterBeat,
    EighthBeat,
    SixteenthBeat
};

namespace IDs
{
    #define DECLARE_ID(name) const juce::Identifier name (#name);
    DECLARE_ID (EDITVIEWSTATE)
    DECLARE_ID (viewX1)
    DECLARE_ID (viewX2)
    DECLARE_ID (viewY)
    DECLARE_ID (drawWaveforms)
    DECLARE_ID (showFooters)
    DECLARE_ID (trackHeight)
    DECLARE_ID (showGrid)
    DECLARE_ID (snapToGrid)
    DECLARE_ID (gridDivision)
    DECLARE_ID (pixelsPerBeat)
    DECLARE_ID (rippleMode)
    DECLARE_ID (expandedTakeClipId)
    DECLARE_ID (createTakesOnLoop)
    DECLARE_ID (SESSIONSTATE)
    DECLARE_ID (activeMainView)
    DECLARE_ID (sceneCount)
    DECLARE_ID (slotsPerTrack)
    DECLARE_ID (launchQuantization)
    DECLARE_ID (sceneLaunchMode)
    DECLARE_ID (arrangementWritePosition)
    DECLARE_ID (recordToArrangementEnabled)
    DECLARE_ID (SLOT)
    DECLARE_ID (trackId)
    DECLARE_ID (sceneIndex)
    DECLARE_ID (clipId)
    #undef DECLARE_ID
}

class EditViewState
{
public:
    EditViewState (te::Edit& e, te::SelectionManager& s, te::EditInsertPoint* ip = nullptr)
        : edit (e), selectionManager (s), insertPoint (ip)
    {
        state = edit.state.getOrCreateChildWithName (IDs::EDITVIEWSTATE, nullptr);
        auto* um = &edit.getUndoManager();

        drawWaveforms.referTo (state, IDs::drawWaveforms, um, true);
        showFooters.referTo (state, IDs::showFooters, um, true);
        showGrid.referTo (state, IDs::showGrid, um, true);
        snapToGrid.referTo (state, IDs::snapToGrid, um, true);
        rippleMode.referTo (state, IDs::rippleMode, um, false);
        gridDivision.referTo (state, IDs::gridDivision, um, (int) GridDivision::Auto);
        pixelsPerBeat.referTo (state, IDs::pixelsPerBeat, um, 24.0);
        viewX1.referTo (state, IDs::viewX1, um, 0s);
        viewX2.referTo (state, IDs::viewX2, um, 60s);
        viewY.referTo (state, IDs::viewY, um, 0);
        trackHeight.referTo (state, IDs::trackHeight, um, 80);
        expandedTakeClipId.referTo (state, IDs::expandedTakeClipId, um, (juce::int64) 0);
        createTakesOnLoop.referTo (state, IDs::createTakesOnLoop, um, true);

        sessionState = state.getOrCreateChildWithName (IDs::SESSIONSTATE, um);
        activeMainView.referTo (sessionState, IDs::activeMainView, um, (int) MainView::arrangement);
        sceneCount.referTo (sessionState, IDs::sceneCount, um, 8);
        slotsPerTrack.referTo (sessionState, IDs::slotsPerTrack, um, 8);
        launchQuantization.referTo (sessionState, IDs::launchQuantization, um, (int) LaunchQuantization::bar);
        sceneLaunchMode.referTo (sessionState, IDs::sceneLaunchMode, um, (int) SceneLaunchMode::stopOthers);
        arrangementWritePosition.referTo (sessionState, IDs::arrangementWritePosition, um, 0s);
        recordToArrangementEnabled.referTo (sessionState, IDs::recordToArrangementEnabled, um, false);
    }

    te::TimePosition getArrangementWritePosition() const
    {
        return arrangementWritePosition.get();
    }

    void setArrangementWritePosition (te::TimePosition pos)
    {
        arrangementWritePosition = pos;
    }

    bool isRecordToArrangementEnabled() const
    {
        return recordToArrangementEnabled.get();
    }

    void setRecordToArrangementEnabled (bool enabled)
    {
        recordToArrangementEnabled = enabled;
    }

    MainView getMainView() const
    {
        return static_cast<MainView> (juce::jlimit (0, 1, activeMainView.get()));
    }

    void setMainView (MainView view)
    {
        activeMainView = (int) view;
    }

    LaunchQuantization getLaunchQuantization() const
    {
        return static_cast<LaunchQuantization> (juce::jlimit (0, (int) LaunchQuantization::eighthBeat,
                                                              launchQuantization.get()));
    }

    SceneLaunchMode getSceneLaunchMode() const
    {
        return static_cast<SceneLaunchMode> (juce::jlimit (0, 1, sceneLaunchMode.get()));
    }

    GridDivision getGridDivision() const
    {
        return static_cast<GridDivision> (juce::jlimit (0, (int) GridDivision::SixteenthBeat,
                                                        gridDivision.get()));
    }

    double getPixelsPerBeat() const
    {
        return juce::jmax (4.0, pixelsPerBeat.get());
    }

    /** Absolute timeline X from song start (scrollable canvas). */
    int timeToX (te::TimePosition time) const
    {
        const auto& ts = edit.tempoSequence;
        return juce::roundToInt (ts.toBeats (time).inBeats() * getPixelsPerBeat());
    }

    /** Absolute timeline time from canvas X. */
    te::TimePosition xToTime (int x) const
    {
        const auto& ts = edit.tempoSequence;
        return ts.toTime (te::BeatPosition::fromBeats ((double) x / getPixelsPerBeat()));
    }

    /** Map time into the currently visible viewport width (ruler overlay). */
    int timeToXInView (te::TimePosition time, int viewWidth) const
    {
        const auto range = viewX2.get() - viewX1.get();
        if (range <= 0s)
            return 0;
        return juce::roundToInt (((time - viewX1.get()) * viewWidth) / range);
    }

    te::TimePosition xToTimeInView (int x, int viewWidth) const
    {
        const auto range = viewX2.get() - viewX1.get();
        return toPosition (range * (double (x) / juce::jmax (1, viewWidth))) + toDuration (viewX1.get());
    }

    void syncVisibleRangeFromScroll (int scrollX, int viewWidth)
    {
        viewX1 = xToTime (scrollX);
        viewX2 = xToTime (scrollX + juce::jmax (1, viewWidth));
    }

    te::TimePosition getTimelineEndTime() const
    {
        const auto& ts = edit.tempoSequence;
        const auto barBeats = ts.toBeats (ts.toTime ({ 1, {} })).inBeats() - ts.toBeats (0s).inBeats();
        double endBeats = ts.toBeats (ts.toTime ({ 128, {} })).inBeats();

        for (auto track : te::getAllTracks (edit))
        {
            if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track))
            {
                for (auto* clip : clipTrack->getClips())
                {
                    const double clipEndBeats = ts.toBeats (clip->getPosition().getEnd()).inBeats();
                    endBeats = juce::jmax (endBeats, clipEndBeats);
                }
            }
        }

        const double loopEndBeats = ts.toBeats (edit.getTransport().getLoopRange().getEnd()).inBeats();
        endBeats = juce::jmax (endBeats, loopEndBeats);
        endBeats += 4.0 * barBeats;

        return ts.toTime (te::BeatPosition::fromBeats (endBeats));
    }

    int getTimelineWidthPx() const
    {
        return timeToX (getTimelineEndTime()) + 400;
    }

    int zoomHorizontalAndGetScroll (double factor, int anchorXInView, int currentScrollX)
    {
        const auto anchorTime = xToTime (currentScrollX + anchorXInView);
        pixelsPerBeat = juce::jlimit (4.0, 256.0, getPixelsPerBeat() * factor);
        return juce::jmax (0, timeToX (anchorTime) - anchorXInView);
    }

    te::Edit& edit;
    te::SelectionManager& selectionManager;
    te::EditInsertPoint* insertPoint = nullptr;

    juce::CachedValue<bool> drawWaveforms, showFooters, showGrid, snapToGrid, rippleMode;
    juce::CachedValue<int> gridDivision;
    juce::CachedValue<double> pixelsPerBeat;
    juce::CachedValue<te::TimePosition> viewX1, viewX2;
    juce::CachedValue<int> viewY, trackHeight;
    juce::CachedValue<juce::int64> expandedTakeClipId;
    juce::CachedValue<bool> createTakesOnLoop;
    juce::ValueTree state;
    juce::ValueTree sessionState;
    juce::CachedValue<int> activeMainView;
    juce::CachedValue<int> sceneCount;
    juce::CachedValue<int> slotsPerTrack;
    juce::CachedValue<int> launchQuantization;
    juce::CachedValue<int> sceneLaunchMode;
    juce::CachedValue<te::TimePosition> arrangementWritePosition;
    juce::CachedValue<bool> recordToArrangementEnabled;

    WaveformCache waveformCache;
    LaneBackgroundCache laneBackgroundCache;
};

} // namespace skeletonhive
