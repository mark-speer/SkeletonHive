#pragma once

#include "TrackComponents.h"
#include "Engine/AudioToMidiTypes.h"
#include "TimelineGrid.h"
#include "TimelineTypes.h"
#include "Engine/GroovePoolManager.h"

namespace skeletonhive
{

class UiTelemetryHub;

/** Bar/beat ruler with a draggable loop brace bound to the transport's loop
    range. Also shows arrangement markers and tempo/time-sig change flags;
    right-click for marker and tempo-map editing. */
class TimelineRulerComponent : public juce::Component,
                               private juce::ChangeListener
{
public:
    TimelineRulerComponent (te::Edit& edit, EditViewState& viewState);
    ~TimelineRulerComponent() override;

    /** Called after tempo/time-sig edits so the owner can re-layout clips. */
    std::function<void()> onTempoMapChanged;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    enum class DragTarget { none, loopStart, loopEnd, loopMove, scrub };

    static constexpr int loopBraceHeight = 9;
    static constexpr int handleTolerancePx = 5;

    void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

    int xForTime (te::TimePosition time) const;
    te::TimePosition timeForX (int x) const;
    DragTarget targetForPosition (juce::Point<int> pos) const;

    void showRulerContextMenu (const juce::MouseEvent& e);
    void promptForTempoChange (te::TimePosition time);
    void renameMarker (te::MarkerClip& marker);
    te::MarkerClip* markerNearX (int x) const;
    void notifyTempoMapChanged();

    void paintMarkers (juce::Graphics& g);
    void paintTempoChanges (juce::Graphics& g);

    te::Edit& editRef;
    EditViewState& editViewState;

    DragTarget dragTarget = DragTarget::none;
    te::TimeRange dragStartLoopRange;
    te::TimePosition dragAnchorTime;
};

class TimelineComponent : public juce::Component,
                          private te::ValueTreeAllEventListener,
                          private FlaggedAsyncUpdater,
                          private juce::ScrollBar::Listener
{
public:
    TimelineComponent (te::Edit& edit, te::SelectionManager& selectionManager,
                       te::EditInsertPoint* insertPoint,
                       UiTelemetryHub* telemetryHub = nullptr);
    ~TimelineComponent() override;

    EditViewState& getEditViewState() { return editViewState; }

    std::function<void (te::Clip&)> onClipDoubleClick;
    std::function<void (te::Track&)> onAddPlugin;
    std::function<void (te::Track&)> onTrackSelected;
    std::function<void()> onClipSelectionChanged;
    std::function<void()> onShowClipProperties;
    std::function<void (te::Clip&)> onEditWarpMarkers;
    std::function<void (te::Clip&, AudioToMidiMode)> onAudioToMidi;
    std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> createPlugin;
    std::function<void (const juce::PluginDescription&)> onPluginInserted;
    std::function<void (const juce::File&, te::Clip*)> onSampleInserted;
    std::function<void (te::Clip&)> onExportClipToLibrary;
    std::function<te::Clip* (te::ClipTrack&, te::TimePosition, const juce::File&)> instantiateClipPreset;

    void setGroovePool (GroovePoolManager* pool) { groovePool = pool; }

    void rebuildTracks();
    void clearRangeSelectionsExcept (TrackLaneComponent* except);
    void repaintLoopBrace();
    bool performCommand (int commandID);
    bool handleKeyPress (const juce::KeyPress& key);
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void paintOverChildren (juce::Graphics& g) override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markAndUpdate (updateTracks); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markAndUpdate (updateTracks); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { markAndUpdate (relayoutTracks); }
    void handleAsyncUpdate() override;
    void resized() override;

    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void buildTracks();
    void rebuildTrackRowList();
    void refreshVisibleTracks();
    void destroyAllVisibleTracks();
    void createVisibleTrackUI (const struct TrackRowInfo& row);
    void layoutTracks();
    void updateTimelineWidth();
    void updateHorizontalScrollBarOverlay();
    void refreshLaneLayouts();
    void syncVisibleRange();
    void invalidateLaneBackgrounds();
    void repaintGrid();
    void toggleShowGrid();
    void syncGridControls();

    void duplicateSelectedClips();
    bool consolidateSelectedClips();
    bool deleteSelectedClips();
    void jumpToMarker (bool next);
    void groupSelectedClips (bool group);
    void toggleRippleMode();
    void rippleAfterInsert (te::Clip& originalClip, te::Clip& insertedCopy);
    void rippleAfterDelete (te::ClipTrack& track, te::TimePosition removedStart, te::TimeDuration removedLength);

    void handleClipCrossTrackDragMove (te::Clip& clip, const juce::MouseEvent& e);
    void handleClipCrossTrackDragEnd (te::Clip& clip, const juce::MouseEvent& e);
    void handleClipDragOverlayUpdate (te::Clip& clip, ClipComponent::DragMode mode,
                                      te::TimePosition snapTime, te::TimePosition ghostStart,
                                      te::TimePosition ghostEnd);
    void clearClipDragOverlay();

    bool handleEmptyLaneDrag (TrackLaneComponent& lane, const juce::MouseEvent& e);
    bool handleEmptyLaneDragEnd (TrackLaneComponent& lane, const juce::MouseEvent& e);
    void cancelEmptyLaneDrag (TrackLaneComponent& lane);

    int trackRowIndexAtContentY (int contentY) const;
    te::ClipTrack* clipTrackForRowIndex (int rowIndex) const;
    void paintCrossTrackDropOverlay (juce::Graphics& g);
    void paintClipMarqueeOverlay (juce::Graphics& g);
    void paintClipDragOverlay (juce::Graphics& g);
    void clearCrossTrackDragState();
    void clearClipMarqueeState();
    juce::Point<int> contentPointForLaneEvent (TrackLaneComponent& lane, const juce::MouseEvent& e) const;
    bool marqueeIntersectsClips (const juce::Rectangle<int>& rect) const;
    void applyWheelDelta (double scaledDelta, bool horizontal, bool vertical);

    struct CrossTrackDragState
    {
        bool active = false;
        te::EditItemID clipId;
        int sourceRowIndex = -1;
        int targetRowIndex = -1;
        te::TimePosition ghostStart;
        bool validDrop = false;
    };

    CrossTrackDragState crossTrackDrag;

    struct ClipMarqueeState
    {
        bool active = false;
        bool clipSelectMode = false;
        juce::Point<int> startContent;
        juce::Point<int> currentContent;
        TrackLaneComponent* anchorLane = nullptr;
    };

    struct ClipDragOverlayState
    {
        bool active = false;
        te::EditItemID clipId;
        ClipComponent::DragMode mode = ClipComponent::DragMode::none;
        te::TimePosition snapTime;
        te::TimePosition ghostStart;
        te::TimePosition ghostEnd;
        int sourceRowIndex = -1;
        juce::Colour clipColour;
    };

    ClipMarqueeState clipMarquee;
    ClipDragOverlayState clipDragOverlay;

    double horizontalScrollAccumulator = 0.0;
    double verticalScrollAccumulator = 0.0;

    te::Edit& edit;
    EditViewState editViewState;
    UiTelemetryHub* telemetryHub = nullptr;
    PlayheadOverlay playhead;
    TimelineRulerComponent ruler;
    juce::ToggleButton gridButton { "Grid" };
    juce::ToggleButton snapButton { "Snap" };
    juce::ToggleButton rippleButton { "Ripple" };
    juce::ComboBox gridDivisionBox;
    juce::ScrollBar hScrollBarOverlay { false };

    static constexpr int headerWidth = 190;
    static constexpr int rulerHeight = 24;
    static constexpr int footerHeight = 28;
    static constexpr int hScrollBarHeight = 14;
    static constexpr int minTrackHeight = 72;
    static constexpr int maxTrackHeight = 240;

    juce::Viewport timelineViewport;
    juce::Component timelineContent;
    juce::Viewport headerViewport;
    juce::Component headerContent;
    static constexpr int verticalVirtualizationMargin = 200;

    juce::Array<TrackRowInfo> trackRows;
    juce::OwnedArray<TrackLaneComponent> trackLanes;
    juce::OwnedArray<TrackHeaderComponent> trackHeaders;
    juce::OwnedArray<TrackFooterComponent> trackFooters;

    bool updateTracks = false;
    bool relayoutTracks = false;
    bool laneLevelRenderingActive = false;

    GroovePoolManager* groovePool = nullptr;
};

} // namespace skeletonhive
