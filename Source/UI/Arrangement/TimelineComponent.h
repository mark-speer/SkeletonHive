#pragma once

#include "TrackComponents.h"
#include "TimelineGrid.h"

namespace arrange
{

/** Bar/beat ruler with a draggable loop brace bound to the transport's loop range. */
class TimelineRulerComponent : public juce::Component
{
public:
    TimelineRulerComponent (te::Edit& edit, EditViewState& viewState)
        : editRef (edit), editViewState (viewState) {}

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    enum class DragTarget { none, loopStart, loopEnd, loopMove, scrub };

    static constexpr int loopBraceHeight = 9;
    static constexpr int handleTolerancePx = 5;

    int xForTime (te::TimePosition time) const;
    te::TimePosition timeForX (int x) const;
    DragTarget targetForPosition (juce::Point<int> pos) const;

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
                       te::EditInsertPoint* insertPoint);
    ~TimelineComponent() override;

    EditViewState& getEditViewState() { return editViewState; }

    std::function<void (te::Clip&)> onClipDoubleClick;
    std::function<void (te::Track&)> onAddPlugin;
    std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> createPlugin;

    void rebuildTracks();
    void clearRangeSelectionsExcept (TrackLaneComponent* except);
    bool handleKeyPress (const juce::KeyPress& key);
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markAndUpdate (updateTracks); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markAndUpdate (updateTracks); }
    void handleAsyncUpdate() override;
    void resized() override;

    void scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart) override;

    void buildTracks();
    void layoutTracks();
    void updateTimelineWidth();
    void refreshLaneLayouts();
    void syncVisibleRange();
    void repaintGrid();
    void toggleShowGrid();
    void syncGridControls();

    void duplicateSelectedClips();
    bool deleteSelectedClips();
    void groupSelectedClips (bool group);
    void toggleRippleMode();
    void rippleAfterInsert (te::Clip& originalClip, te::Clip& insertedCopy);
    void rippleAfterDelete (te::ClipTrack& track, te::TimePosition removedStart, te::TimeDuration removedLength);

    te::Edit& edit;
    EditViewState editViewState;
    PlayheadOverlay playhead;
    TimelineRulerComponent ruler;
    juce::ToggleButton gridButton { "Grid" };
    juce::ToggleButton snapButton { "Snap" };
    juce::ToggleButton rippleButton { "Ripple" };
    juce::ComboBox gridDivisionBox;

    static constexpr int headerWidth = 190;
    static constexpr int rulerHeight = 24;
    static constexpr int footerHeight = 28;
    static constexpr int minTrackHeight = 36;
    static constexpr int maxTrackHeight = 240;

    juce::Viewport timelineViewport;
    juce::Component timelineContent;
    juce::Viewport headerViewport;
    juce::Component headerContent;
    juce::OwnedArray<TrackLaneComponent> trackLanes;
    juce::OwnedArray<TrackHeaderComponent> trackHeaders;
    juce::OwnedArray<TrackFooterComponent> trackFooters;

    bool updateTracks = false;
};

} // namespace arrange
