#include "ClipComponents.h"
#include "TimelineGrid.h"
#include "TrackComponents.h"
#include "Engine/EngineHelpers.h"

namespace arrange
{

void drawMidiClipPreview (juce::Graphics& g, te::MidiClip& clip, juce::Rectangle<int> area, te::TimeRange viewRange)
{
    if (area.isEmpty())
        return;

    const auto clipRange = clip.getPosition().time;
    const auto visibleStart = juce::jmax (clipRange.getStart(), viewRange.getStart());
    const auto visibleEnd = juce::jmin (clipRange.getEnd(), viewRange.getEnd());

    if (visibleEnd <= visibleStart)
        return;

    const float pixelsPerSecond = (float) area.getWidth() / (float) viewRange.getLength().inSeconds();
    const int lowestNote = 36, highestNote = 96, noteRange = highestNote - lowestNote;

    g.setColour (juce::Colours::white.withAlpha (0.8f));

    for (int i = 0; i < clip.getSequence().getNumNotes(); ++i)
    {
        const auto* note = clip.getSequence().getNote (i);
        const auto noteStart = note->getEditStartTime (clip);
        const float x = (float) ((noteStart - viewRange.getStart()).inSeconds()) * pixelsPerSecond;
        const float w = juce::jmax (2.0f, (float) note->getLengthSeconds (clip).inSeconds() * pixelsPerSecond);
        const float y = area.getHeight() * (1.0f - (float) (note->getNoteNumber() - lowestNote) / (float) noteRange);
        const float h = juce::jmax (2.0f, area.getHeight() / (float) noteRange);

        g.fillRect (x, y - h, w, h);
    }
}

ClipComponent::ClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : editViewState (evs), clip (std::move (c))
{
}

te::TimePosition ClipComponent::timeAtLaneX (int laneX) const
{
    return editViewState.xToTime (laneX);
}

te::TimePosition ClipComponent::snapTime (te::TimePosition time) const
{
    return TimelineGrid::snapTime (editViewState.edit, editViewState, time);
}

ClipComponent::DragMode ClipComponent::dragModeForEvent (const juce::MouseEvent& e) const
{
    if (e.mods.isRightButtonDown())
        return DragMode::none;

    if (getWidth() <= resizeHandleWidth * 2)
        return DragMode::move;

    if (e.x <= resizeHandleWidth)
        return DragMode::resizeStart;

    if (e.x >= getWidth() - resizeHandleWidth)
        return DragMode::resizeEnd;

    return DragMode::move;
}

void ClipComponent::updateCursorForMode (DragMode mode)
{
    if (mode == DragMode::resizeStart || mode == DragMode::resizeEnd)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ClipComponent::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::darkgrey);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.setColour (juce::Colours::white);
    g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 1.0f);
    g.drawText (clip->getName(), getLocalBounds().reduced (4), juce::Justification::centredLeft, true);
    paintSelectionAndGroupIndicators (g);
}

void ClipComponent::paintSelectionAndGroupIndicators (juce::Graphics& g) const
{
    if (editViewState.selectionManager.isSelected (clip.get()))
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 2.0f);
    }

    if (EngineHelpers::getClipGroup (*clip).isNotEmpty())
    {
        // Small corner tag marking grouped clips, coloured per-group
        g.setColour (EngineHelpers::getClipGroupColour (*clip));
        g.fillRect (getWidth() - 10, 0, 10, 6);
    }
}

void ClipComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        editViewState.selectionManager.selectOnly (clip.get());

        if (auto* lane = findParentComponentOfClass<TrackLaneComponent>())
            lane->clearRangeSelection();

        if (editViewState.insertPoint != nullptr)
        {
            if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (clip->getTrack()))
                editViewState.insertPoint->setNextInsertPoint (clip->getPosition().getStart(), clipTrack);
        }

        showTimelineContextMenu (*this, e.getScreenPosition(), editViewState, clip->getTrack(), false, nullptr);
        return;
    }

    editViewState.selectionManager.selectOnly (clip.get());

    dragMode = dragModeForEvent (e);
    if (dragMode == DragMode::none)
        return;

    // Ctrl-drag: leave a duplicate at the original position and move this clip.
    // The duplicate left behind is no longer part of the original's group (which
    // continues to move together) — it becomes its own single-member group.
    if (dragMode == DragMode::move && (e.mods.isCtrlDown() || e.mods.isCommandDown()))
    {
        if (auto* leftBehind = EngineHelpers::duplicateClip (*clip, false))
        {
            const auto originalGroupId = EngineHelpers::getClipGroup (*clip);
            if (originalGroupId.isNotEmpty())
            {
                EngineHelpers::setClipGroup (*leftBehind, juce::Uuid().toString());
                EngineHelpers::setClipGroupColour (*leftBehind, EngineHelpers::getClipGroupColour (*clip));
            }
        }
    }

    const auto pos = clip->getPosition();
    originalStart = pos.getStart();
    originalEnd = pos.getEnd();
    dragAnchorTime = timeAtLaneX (getX() + e.x);

    if (dragMode == DragMode::move)
    {
        captureGroupDragItems();
        captureRippleDragItems (originalStart);
    }
    else if (dragMode == DragMode::resizeEnd)
    {
        captureRippleDragItems (originalEnd);
    }

    updateCursorForMode (dragMode);
}

void ClipComponent::captureGroupDragItems()
{
    groupDragItems.clear();

    const auto groupId = EngineHelpers::getClipGroup (*clip);
    if (groupId.isEmpty())
        return;

    for (auto* member : EngineHelpers::getClipsInGroup (editViewState.edit, groupId))
        if (member != clip.get())
            groupDragItems.add ({ member, member->getPosition().getStart() });
}

void ClipComponent::captureRippleDragItems (te::TimePosition anchor)
{
    rippleDragItems.clear();

    if (! editViewState.rippleMode.get())
        return;

    auto* track = clip->getClipTrack();
    if (track == nullptr)
        return;

    // Clips already moving together as part of this clip's group are excluded
    // here so they aren't shifted twice.
    const auto groupId = EngineHelpers::getClipGroup (*clip);

    for (auto* c : EngineHelpers::getClipsStartingAfter (*track, anchor))
    {
        if (c == clip.get())
            continue;
        if (groupId.isNotEmpty() && EngineHelpers::getClipGroup (*c) == groupId)
            continue;
        rippleDragItems.add ({ c, c->getPosition().getStart() });
    }
}

void ClipComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none)
        return;

    const auto currentTime = timeAtLaneX (getX() + e.x);
    const auto minLength = te::TimeDuration::fromSeconds (minClipLengthSeconds);

    if (dragMode == DragMode::move)
    {
        const auto delta = currentTime - dragAnchorTime;
        const auto newStart = snapTime (originalStart + delta);
        clip->setStart (newStart, false, true);

        // Move grouped clips by the same (snapped) amount
        const auto effectiveDelta = newStart - originalStart;
        for (auto& item : groupDragItems)
        {
            const auto memberStart = juce::jmax (te::TimePosition(), item.originalStart + effectiveDelta);
            item.clip->setStart (memberStart, false, true);
        }

        // Ripple mode: shift every later clip on this track by the same amount
        for (auto& item : rippleDragItems)
        {
            const auto memberStart = juce::jmax (te::TimePosition(), item.originalStart + effectiveDelta);
            item.clip->setStart (memberStart, false, true);
        }
    }
    else if (dragMode == DragMode::resizeStart)
    {
        const auto newStart = snapTime (currentTime);
        if (originalEnd - newStart >= minLength)
        {
            clip->setStart (newStart, false, false);
            clip->setEnd (originalEnd, false);
        }
    }
    else if (dragMode == DragMode::resizeEnd)
    {
        const auto newEnd = snapTime (currentTime);
        if (newEnd - originalStart >= minLength)
        {
            clip->setEnd (newEnd, false);

            // Ripple mode: resizing changes the clip's length, so shift
            // everything after it by the same change in length.
            const auto effectiveDelta = newEnd - originalEnd;
            for (auto& item : rippleDragItems)
            {
                const auto memberStart = juce::jmax (te::TimePosition(), item.originalStart + effectiveDelta);
                item.clip->setStart (memberStart, false, true);
            }
        }
    }
}

void ClipComponent::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    dragMode = DragMode::none;
    groupDragItems.clear();
    rippleDragItems.clear();
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ClipComponent::mouseMove (const juce::MouseEvent& e)
{
    if (dragMode != DragMode::none)
        return;

    const auto mode = dragModeForEvent (e);
    if (mode == DragMode::resizeStart || mode == DragMode::resizeEnd)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
}

void ClipComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (onDoubleClick)
        onDoubleClick (*clip);
    juce::ignoreUnused (e);
}

AudioClipComponent::AudioClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent (evs, std::move (c))
{
    updateThumbnail();
}

void AudioClipComponent::updateThumbnail()
{
    if (auto* waveClip = dynamic_cast<te::WaveAudioClip*> (clip.get()))
    {
        thumbnail = std::make_unique<te::SmartThumbnail> (editViewState.edit.engine,
                                                            waveClip->getAudioFile(),
                                                            *this, nullptr);
    }
}

void AudioClipComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour (juce::Colour (0xff2d6a4f));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    if (editViewState.drawWaveforms && thumbnail != nullptr)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        const te::TimeRange viewRange { editViewState.viewX1, editViewState.viewX2 };
        thumbnail->drawChannels (g, bounds, viewRange, 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawText (clip->getName(), bounds.reduced (4), juce::Justification::centredLeft, true);
    paintSelectionAndGroupIndicators (g);
}

MidiClipComponent::MidiClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent (evs, std::move (c))
{
}

void MidiClipComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.setColour (juce::Colour (0xff4361ee));
    g.fillRoundedRectangle (bounds.toFloat(), 4.0f);

    if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip.get()))
    {
        const te::TimeRange viewRange { editViewState.viewX1, editViewState.viewX2 };
        drawMidiClipPreview (g, *midiClip, bounds.reduced (2), viewRange);
    }

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.drawText (clip->getName(), bounds.reduced (4), juce::Justification::centredLeft, true);
    paintSelectionAndGroupIndicators (g);
}

} // namespace arrange
