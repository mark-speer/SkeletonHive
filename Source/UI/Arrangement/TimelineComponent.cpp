#include "TimelineComponent.h"
#include "Engine/EngineHelpers.h"

namespace arrange
{

//==============================================================================
// TimelineRulerComponent

int TimelineRulerComponent::xForTime (te::TimePosition time) const
{
    return editViewState.timeToXInView (time, getWidth());
}

te::TimePosition TimelineRulerComponent::timeForX (int x) const
{
    return editViewState.xToTimeInView (x, getWidth());
}

void TimelineRulerComponent::paint (juce::Graphics& g)
{
    TimelineGrid::drawRuler (g, editRef, editViewState, getLocalBounds());

    // Loop brace across the top of the ruler
    const auto loopRange = editRef.getTransport().getLoopRange();
    if (loopRange.getLength() > 0s)
    {
        const int x1 = xForTime (loopRange.getStart());
        const int x2 = xForTime (loopRange.getEnd());

        if (x2 >= 0 && x1 <= getWidth())
        {
            const bool looping = editRef.getTransport().looping;
            const auto braceColour = looping ? juce::Colour (0xffffd166) : juce::Colours::grey;

            g.setColour (braceColour.withAlpha (0.35f));
            g.fillRect (x1, 0, juce::jmax (2, x2 - x1), loopBraceHeight);
            g.setColour (braceColour);
            g.drawRect (x1, 0, juce::jmax (2, x2 - x1), loopBraceHeight);
            g.fillRect (x1 - 2, 0, 4, loopBraceHeight);
            g.fillRect (x2 - 2, 0, 4, loopBraceHeight);
        }
    }
}

TimelineRulerComponent::DragTarget TimelineRulerComponent::targetForPosition (juce::Point<int> pos) const
{
    const auto loopRange = editRef.getTransport().getLoopRange();
    const int x1 = xForTime (loopRange.getStart());
    const int x2 = xForTime (loopRange.getEnd());

    if (pos.y <= loopBraceHeight + 2 && loopRange.getLength() > 0s)
    {
        if (std::abs (pos.x - x1) <= handleTolerancePx)
            return DragTarget::loopStart;
        if (std::abs (pos.x - x2) <= handleTolerancePx)
            return DragTarget::loopEnd;
        if (pos.x > x1 && pos.x < x2)
            return DragTarget::loopMove;
    }

    return DragTarget::scrub;
}

void TimelineRulerComponent::mouseDown (const juce::MouseEvent& e)
{
    dragTarget = targetForPosition (e.getPosition());
    dragStartLoopRange = editRef.getTransport().getLoopRange();
    dragAnchorTime = timeForX (e.x);

    if (dragTarget == DragTarget::scrub)
        mouseDrag (e);
}

void TimelineRulerComponent::mouseDrag (const juce::MouseEvent& e)
{
    const auto time = timeForX (e.x);
    const auto snapped = TimelineGrid::snapTime (editRef, editViewState, time);
    auto& transport = editRef.getTransport();

    switch (dragTarget)
    {
        case DragTarget::scrub:
            transport.setPosition (juce::jmax (te::TimePosition(), snapped));
            break;

        case DragTarget::loopStart:
        {
            const auto newStart = juce::jlimit (te::TimePosition(), dragStartLoopRange.getEnd(), snapped);
            transport.setLoopRange ({ newStart, dragStartLoopRange.getEnd() });
            break;
        }

        case DragTarget::loopEnd:
        {
            const auto newEnd = juce::jmax (dragStartLoopRange.getStart(), snapped);
            transport.setLoopRange ({ dragStartLoopRange.getStart(), newEnd });
            break;
        }

        case DragTarget::loopMove:
        {
            const auto rawDelta = time - dragAnchorTime;
            auto newStart = TimelineGrid::snapTime (editRef, editViewState,
                                                    dragStartLoopRange.getStart() + rawDelta);
            newStart = juce::jmax (te::TimePosition(), newStart);
            transport.setLoopRange ({ newStart, newStart + dragStartLoopRange.getLength() });
            break;
        }

        case DragTarget::none:
            break;
    }

    repaint();
}

void TimelineRulerComponent::mouseUp (const juce::MouseEvent&)
{
    dragTarget = DragTarget::none;
    repaint();
}

void TimelineRulerComponent::mouseMove (const juce::MouseEvent& e)
{
    switch (targetForPosition (e.getPosition()))
    {
        case DragTarget::loopStart:
        case DragTarget::loopEnd:
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            break;
        case DragTarget::loopMove:
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            break;
        default:
            setMouseCursor (juce::MouseCursor::NormalCursor);
            break;
    }
}

//==============================================================================
// TimelineComponent

TimelineComponent::TimelineComponent (te::Edit& e, te::SelectionManager& sm, te::EditInsertPoint* ip)
    : edit (e),
      editViewState (edit, sm, ip),
      playhead (edit, editViewState),
      ruler (edit, editViewState)
{
    addAndMakeVisible (headerViewport);
    addAndMakeVisible (timelineViewport);
    addAndMakeVisible (ruler);
    addAndMakeVisible (gridButton);
    addAndMakeVisible (snapButton);
    addAndMakeVisible (rippleButton);
    addAndMakeVisible (gridDivisionBox);

    headerViewport.setViewedComponent (&headerContent, false);
    headerViewport.setScrollBarsShown (false, false);

    timelineViewport.setViewedComponent (&timelineContent, false);
    timelineViewport.setScrollBarsShown (true, true);
    timelineViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::all);
    timelineViewport.getHorizontalScrollBar().addListener (this);
    timelineViewport.getVerticalScrollBar().addListener (this);

    timelineContent.addAndMakeVisible (playhead);
    playhead.toFront (false);

    gridButton.setClickingTogglesState (true);
    gridButton.setToggleState (editViewState.showGrid.get(), juce::dontSendNotification);
    gridButton.onClick = [this]
    {
        editViewState.showGrid = gridButton.getToggleState();
        repaintGrid();
    };

    snapButton.setClickingTogglesState (true);
    snapButton.setToggleState (editViewState.snapToGrid.get(), juce::dontSendNotification);
    snapButton.onClick = [this]
    {
        editViewState.snapToGrid = snapButton.getToggleState();
    };

    rippleButton.setClickingTogglesState (true);
    rippleButton.setToggleState (editViewState.rippleMode.get(), juce::dontSendNotification);
    rippleButton.setTooltip ("Ripple edit: moving/resizing/duplicating/deleting a clip "
                             "shifts every later clip on the same track (Ctrl+R)");
    rippleButton.onClick = [this]
    {
        editViewState.rippleMode = rippleButton.getToggleState();
    };

    gridDivisionBox.addItem ("Auto", (int) GridDivision::Auto + 1);
    gridDivisionBox.addItem ("1 Bar", (int) GridDivision::Bar + 1);
    gridDivisionBox.addItem ("1/2 Bar", (int) GridDivision::HalfBar + 1);
    gridDivisionBox.addItem ("1/4", (int) GridDivision::Beat + 1);
    gridDivisionBox.addItem ("1/8", (int) GridDivision::HalfBeat + 1);
    gridDivisionBox.addItem ("1/16", (int) GridDivision::QuarterBeat + 1);
    gridDivisionBox.addItem ("1/32", (int) GridDivision::EighthBeat + 1);
    gridDivisionBox.addItem ("1/64", (int) GridDivision::SixteenthBeat + 1);
    gridDivisionBox.setSelectedId ((int) editViewState.getGridDivision() + 1, juce::dontSendNotification);
    gridDivisionBox.onChange = [this]
    {
        editViewState.gridDivision = gridDivisionBox.getSelectedId() - 1;
        repaintGrid();
    };

    edit.state.addListener (this);
    buildTracks();
}

TimelineComponent::~TimelineComponent()
{
    timelineViewport.getHorizontalScrollBar().removeListener (this);
    timelineViewport.getVerticalScrollBar().removeListener (this);
    edit.state.removeListener (this);
}

void TimelineComponent::rebuildTracks()
{
    buildTracks();
}

void TimelineComponent::clearRangeSelectionsExcept (TrackLaneComponent* except)
{
    for (auto* lane : trackLanes)
        if (lane != except)
            lane->clearRangeSelection();
}

//==============================================================================
// Keyboard commands

bool TimelineComponent::handleKeyPress (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('4', juce::ModifierKeys::ctrlModifier, 0)
        || key == juce::KeyPress ('4', juce::ModifierKeys::commandModifier, 0))
    {
        toggleShowGrid();
        return true;
    }
    if (key == juce::KeyPress ('d', juce::ModifierKeys::ctrlModifier, 0)
        || key == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0))
    {
        duplicateSelectedClips();
        return true;
    }
    if (key == juce::KeyPress ('g', juce::ModifierKeys::ctrlModifier, 0)
        || key == juce::KeyPress ('g', juce::ModifierKeys::commandModifier, 0))
    {
        groupSelectedClips (true);
        return true;
    }
    if (key == juce::KeyPress ('g', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('g', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        groupSelectedClips (false);
        return true;
    }
    if (key == juce::KeyPress ('r', juce::ModifierKeys::ctrlModifier, 0)
        || key == juce::KeyPress ('r', juce::ModifierKeys::commandModifier, 0))
    {
        toggleRippleMode();
        return true;
    }
    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
        return deleteSelectedClips();

    return false;
}

void TimelineComponent::toggleRippleMode()
{
    editViewState.rippleMode = ! editViewState.rippleMode.get();
    rippleButton.setToggleState (editViewState.rippleMode.get(), juce::dontSendNotification);
}

void TimelineComponent::duplicateSelectedClips()
{
    const auto clips = editViewState.selectionManager.getItemsOfType<te::Clip>();
    if (clips.isEmpty())
        return;

    // Copies of a grouped selection form their own new group(s) rather than
    // merging into the source group; every source group id seen is remapped
    // once and reused for every copy of that group's members.
    juce::StringArray oldGroupIds, newGroupIds;
    auto remapGroupId = [&oldGroupIds, &newGroupIds] (const juce::String& oldId)
    {
        const int idx = oldGroupIds.indexOf (oldId);
        if (idx >= 0)
            return newGroupIds[idx];

        const auto newId = juce::Uuid().toString();
        oldGroupIds.add (oldId);
        newGroupIds.add (newId);
        return newId;
    };

    juce::Array<te::Clip*> newClips;

    for (auto* clip : clips)
    {
        if (auto* copy = EngineHelpers::duplicateClip (*clip, true))
        {
            rippleAfterInsert (*clip, *copy);

            const auto originalGroupId = EngineHelpers::getClipGroup (*clip);
            if (originalGroupId.isNotEmpty())
            {
                EngineHelpers::setClipGroup (*copy, remapGroupId (originalGroupId));
                EngineHelpers::setClipGroupColour (*copy, EngineHelpers::getClipGroupColour (*clip));
            }

            newClips.add (copy);
        }
    }

    if (newClips.isEmpty())
        return;

    editViewState.selectionManager.selectOnly (newClips.getFirst());
    for (int i = 1; i < newClips.size(); ++i)
        editViewState.selectionManager.addToSelection (newClips[i]);
}

bool TimelineComponent::deleteSelectedClips()
{
    const auto clips = editViewState.selectionManager.getItemsOfType<te::Clip>();
    if (clips.isEmpty())
        return false;

    editViewState.selectionManager.deselectAll();

    juce::StringArray affectedGroups;
    for (auto* clip : clips)
    {
        const auto groupId = EngineHelpers::getClipGroup (*clip);
        if (groupId.isNotEmpty())
            affectedGroups.addIfNotAlreadyThere (groupId);
    }

    for (auto* clip : clips)
    {
        auto* track = clip->getClipTrack();
        const auto position = clip->getPosition();

        clip->removeFromParent();

        if (track != nullptr)
            rippleAfterDelete (*track, position.getStart(), position.getLength());
    }

    // A group left with exactly one member is no longer a group.
    for (const auto& groupId : affectedGroups)
    {
        const auto remaining = EngineHelpers::getClipsInGroup (editViewState.edit, groupId);
        if (remaining.size() == 1)
            EngineHelpers::setClipGroup (*remaining.getFirst(), {});
    }

    return true;
}

void TimelineComponent::rippleAfterInsert (te::Clip& originalClip, te::Clip& insertedCopy)
{
    if (! editViewState.rippleMode.get())
        return;

    auto* track = originalClip.getClipTrack();
    if (track == nullptr)
        return;

    const auto shiftAmount = insertedCopy.getPosition().getLength();
    if (shiftAmount <= 0s)
        return;

    for (auto* c : EngineHelpers::getClipsStartingAfter (*track, originalClip.getPosition().getStart()))
    {
        if (c == &originalClip || c == &insertedCopy)
            continue;

        c->setStart (c->getPosition().getStart() + shiftAmount, false, true);
    }
}

void TimelineComponent::rippleAfterDelete (te::ClipTrack& track, te::TimePosition removedStart, te::TimeDuration removedLength)
{
    if (! editViewState.rippleMode.get() || removedLength <= 0s)
        return;

    for (auto* c : EngineHelpers::getClipsStartingAfter (track, removedStart))
    {
        const auto newStart = juce::jmax (te::TimePosition(), c->getPosition().getStart() - removedLength);
        c->setStart (newStart, false, true);
    }
}

void TimelineComponent::groupSelectedClips (bool group)
{
    const auto clips = editViewState.selectionManager.getItemsOfType<te::Clip>();
    if (clips.isEmpty())
        return;

    const auto groupId = group ? juce::Uuid().toString() : juce::String();
    const auto colour = EngineHelpers::colourForGroupId (groupId);

    for (auto* clip : clips)
    {
        EngineHelpers::setClipGroup (*clip, groupId);
        if (group)
            EngineHelpers::setClipGroupColour (*clip, colour);
    }

    repaintGrid();
}

//==============================================================================

void TimelineComponent::toggleShowGrid()
{
    editViewState.showGrid = ! editViewState.showGrid.get();
    syncGridControls();
    repaintGrid();
}

void TimelineComponent::syncGridControls()
{
    gridButton.setToggleState (editViewState.showGrid.get(), juce::dontSendNotification);
}

void TimelineComponent::scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double)
{
    if (scrollBarThatHasMoved == &timelineViewport.getHorizontalScrollBar())
    {
        syncVisibleRange();
        refreshLaneLayouts();
        ruler.repaint();
    }
    else if (scrollBarThatHasMoved == &timelineViewport.getVerticalScrollBar())
    {
        const int y = timelineViewport.getViewPositionY();
        headerViewport.setViewPosition (0, y);
        editViewState.viewY = y;
    }
}

void TimelineComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateTracks))
        buildTracks();
}

void TimelineComponent::syncVisibleRange()
{
    editViewState.syncVisibleRangeFromScroll (timelineViewport.getViewPositionX(),
                                              timelineViewport.getViewWidth());
}

void TimelineComponent::refreshLaneLayouts()
{
    for (auto* lane : trackLanes)
        lane->refreshLayout();
}

void TimelineComponent::updateTimelineWidth()
{
    const int width = editViewState.getTimelineWidthPx();

    for (auto* lane : trackLanes)
        lane->setSize (width, lane->getHeight());

    timelineContent.setSize (width, timelineContent.getHeight());
    playhead.setSize (width, playhead.getHeight());
    syncVisibleRange();
    refreshLaneLayouts();
}

void TimelineComponent::repaintGrid()
{
    ruler.repaint();
    for (auto* lane : trackLanes)
        lane->repaint();
}

void TimelineComponent::buildTracks()
{
    trackLanes.clear();
    trackHeaders.clear();
    trackFooters.clear();
    timelineContent.removeAllChildren();
    headerContent.removeAllChildren();
    timelineContent.addAndMakeVisible (playhead);

    for (auto track : te::getAllTracks (edit))
    {
        if (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()
            || track->isMasterTrack() || track->isArrangerTrack())
            continue;

        auto lane = std::make_unique<TrackLaneComponent> (editViewState, track);
        lane->onClipDoubleClick = [this] (te::Clip& c)
        {
            if (onClipDoubleClick)
                onClipDoubleClick (c);
        };
        timelineContent.addAndMakeVisible (lane.get());
        trackLanes.add (lane.release());

        auto header = std::make_unique<TrackHeaderComponent> (editViewState, track);
        headerContent.addAndMakeVisible (header.get());
        trackHeaders.add (header.release());

        if (editViewState.showFooters)
        {
            auto footer = std::make_unique<TrackFooterComponent> (editViewState, track);
            footer->onAddPlugin = [this] (te::Track& t)
            {
                if (onAddPlugin)
                    onAddPlugin (t);
            };
            headerContent.addAndMakeVisible (footer.get());
            trackFooters.add (footer.release());
        }
    }

    layoutTracks();
    repaintGrid();

    // Restore vertical scroll position
    timelineViewport.setViewPosition (timelineViewport.getViewPositionX(), editViewState.viewY.get());
    headerViewport.setViewPosition (0, timelineViewport.getViewPositionY());
}

void TimelineComponent::layoutTracks()
{
    const int trackH = juce::jlimit (minTrackHeight, maxTrackHeight, editViewState.trackHeight.get());
    const int width = editViewState.getTimelineWidthPx();
    int y = 0;

    for (int i = 0; i < trackLanes.size(); ++i)
    {
        trackLanes[i]->setBounds (0, y, width, trackH);

        if (auto* header = trackHeaders[i])
            header->setBounds (0, y, headerWidth, trackH);

        if (i < trackFooters.size())
            if (auto* footer = trackFooters[i])
                footer->setBounds (0, y + trackH - footerHeight, headerWidth, footerHeight);

        y += trackH;
    }

    timelineContent.setSize (width, y);
    headerContent.setSize (headerWidth, y);
    playhead.setBounds (0, 0, width, y);
    playhead.toFront (false);
    resized();
}

void TimelineComponent::resized()
{
    auto r = getLocalBounds();
    auto topRow = r.removeFromTop (rulerHeight);

    auto gridControls = topRow.removeFromLeft (headerWidth).reduced (2);
    gridButton.setBounds (gridControls.removeFromLeft (40));
    snapButton.setBounds (gridControls.removeFromLeft (40));
    rippleButton.setBounds (gridControls.removeFromLeft (48));
    gridDivisionBox.setBounds (gridControls);

    ruler.setBounds (topRow);

    auto headerArea = r.removeFromLeft (headerWidth);
    headerViewport.setBounds (headerArea);
    timelineViewport.setBounds (r);

    updateTimelineWidth();
    ruler.repaint();
}

void TimelineComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto local = e.getEventRelativeTo (this);
    const bool inTimeline = local.x >= headerWidth;

    if (! inTimeline)
    {
        juce::Component::mouseWheelMove (e, wheel);
        return;
    }

    const bool ctrl = e.mods.isCtrlDown() || e.mods.isCommandDown();

    if (ctrl && e.mods.isShiftDown())
    {
        // Vertical zoom: adjust track height around the current scroll position
        const int oldHeight = juce::jlimit (minTrackHeight, maxTrackHeight, editViewState.trackHeight.get());
        const int newHeight = juce::jlimit (minTrackHeight, maxTrackHeight,
                                            oldHeight + (wheel.deltaY > 0 ? 8 : -8));
        if (newHeight != oldHeight)
        {
            editViewState.trackHeight = newHeight;
            layoutTracks();
        }
    }
    else if (ctrl)
    {
        // Horizontal zoom anchored at the mouse position. Components are kept
        // alive and just re-laid-out: no track rebuild.
        const int anchorX = local.x - headerWidth;
        const int scrollX = timelineViewport.getViewPositionX();
        const int newScroll = editViewState.zoomHorizontalAndGetScroll (wheel.deltaY > 0 ? 1.2 : 0.833,
                                                                        anchorX, scrollX);
        updateTimelineWidth();
        timelineViewport.setViewPosition (newScroll, timelineViewport.getViewPositionY());
        syncVisibleRange();
        refreshLaneLayouts();
        repaintGrid();
    }
    else if (e.mods.isShiftDown() || timelineContent.getHeight() <= timelineViewport.getHeight())
    {
        // Horizontal scroll
        const int delta = (int) std::round ((wheel.deltaX + wheel.deltaY) * 120.0);
        if (delta != 0)
        {
            timelineViewport.setViewPosition (juce::jmax (0, timelineViewport.getViewPositionX() - delta),
                                              timelineViewport.getViewPositionY());
            syncVisibleRange();
            refreshLaneLayouts();
            ruler.repaint();
        }
    }
    else
    {
        // Vertical scroll when the track list is taller than the viewport
        const int delta = (int) std::round (wheel.deltaY * 120.0);
        if (delta != 0)
        {
            const int maxY = juce::jmax (0, timelineContent.getHeight() - timelineViewport.getHeight());
            const int newY = juce::jlimit (0, maxY, timelineViewport.getViewPositionY() - delta);
            timelineViewport.setViewPosition (timelineViewport.getViewPositionX(), newY);
            headerViewport.setViewPosition (0, newY);
            editViewState.viewY = newY;
        }
    }
}

} // namespace arrange
