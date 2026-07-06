#include "TimelineComponent.h"
#include "Engine/AppCommands.h"
#include "UI/AppLookAndFeel.h"
#include "Engine/EngineHelpers.h"
#include "TimelineLOD.h"

namespace skeletonhive
{

//==============================================================================
// TimelineRulerComponent

TimelineRulerComponent::TimelineRulerComponent (te::Edit& edit, EditViewState& viewState)
    : editRef (edit), editViewState (viewState)
{
    editRef.getMarkerManager().addChangeListener (this);
}

TimelineRulerComponent::~TimelineRulerComponent()
{
    editRef.getMarkerManager().removeChangeListener (this);
}

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
            const auto braceColour = looping ? AppColours::accentLoop (AppLookAndFeel::getCurrentTheme())
                                             : juce::Colours::grey;

            g.setColour (braceColour.withAlpha (0.35f));
            g.fillRect (x1, 0, juce::jmax (2, x2 - x1), loopBraceHeight);
            g.setColour (braceColour);
            g.drawRect (x1, 0, juce::jmax (2, x2 - x1), loopBraceHeight);
            g.fillRect (x1 - 2, 0, 4, loopBraceHeight);
            g.fillRect (x2 - 2, 0, 4, loopBraceHeight);
        }
    }

    paintTempoChanges (g);
    paintMarkers (g);
}

void TimelineRulerComponent::paintMarkers (juce::Graphics& g)
{
    const int flagTop = loopBraceHeight + 1;
    const int flagHeight = getHeight() - flagTop;

    for (auto marker : editRef.getMarkerManager().getMarkers())
    {
        const int x = xForTime (marker->getPosition().getStart());
        if (x < -80 || x > getWidth() + 4)
            continue;

        const auto colour = marker->getColour().withAlpha (1.0f);

        g.setColour (colour);
        g.fillRect (x, flagTop, 2, flagHeight);

        juce::Path flag;
        flag.addTriangle ((float) x + 2.0f, (float) flagTop,
                          (float) x + 10.0f, (float) flagTop + 4.0f,
                          (float) x + 2.0f, (float) flagTop + 8.0f);
        g.fillPath (flag);

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (marker->getName(), x + 12, flagTop, 90, flagHeight,
                    juce::Justification::centredLeft, true);
    }
}

void TimelineRulerComponent::paintTempoChanges (juce::Graphics& g)
{
    auto& sequence = editRef.tempoSequence;
    g.setFont (juce::FontOptions (9.0f));

    // Skip index 0: the initial tempo/time-sig isn't a "change"
    for (int i = 1; i < sequence.getNumTempos(); ++i)
    {
        if (auto* tempo = sequence.getTempo (i))
        {
            const int x = xForTime (sequence.toTime (tempo->getStartBeat()));
            if (x < -40 || x > getWidth() + 4)
                continue;

            g.setColour (AppColours::accentTempo (AppLookAndFeel::getCurrentTheme()));
            g.fillEllipse ((float) x - 2.5f, (float) getHeight() - 6.0f, 5.0f, 5.0f);
            g.drawText (juce::String (tempo->getBpm(), 1), x + 4, getHeight() - 11, 36, 10,
                        juce::Justification::centredLeft, false);
        }
    }

    for (int i = 1; i < sequence.getNumTimeSigs(); ++i)
    {
        if (auto* sig = sequence.getTimeSig (i))
        {
            const int x = xForTime (sequence.toTime (sig->getStartBeat()));
            if (x < -40 || x > getWidth() + 4)
                continue;

            g.setColour (AppColours::accentTimeSig (AppLookAndFeel::getCurrentTheme()));
            g.fillEllipse ((float) x - 2.5f, (float) getHeight() - 6.0f, 5.0f, 5.0f);
            g.drawText (sig->getStringTimeSig(), x + 4, getHeight() - 11, 30, 10,
                        juce::Justification::centredLeft, false);
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
    if (e.mods.isPopupMenu())
    {
        dragTarget = DragTarget::none;
        showRulerContextMenu (e);
        return;
    }

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

te::MarkerClip* TimelineRulerComponent::markerNearX (int x) const
{
    te::MarkerClip* best = nullptr;
    int bestDistance = 8;

    for (auto marker : editRef.getMarkerManager().getMarkers())
    {
        const int distance = std::abs (xForTime (marker->getPosition().getStart()) - x);
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            best = marker;
        }
    }

    return best;
}

void TimelineRulerComponent::notifyTempoMapChanged()
{
    if (onTempoMapChanged)
        onTempoMapChanged();

    repaint();
}

void TimelineRulerComponent::renameMarker (te::MarkerClip& marker)
{
    auto w = std::make_shared<juce::AlertWindow> ("Rename Marker", "Marker name:",
                                                  juce::AlertWindow::QuestionIcon);
    w->addTextEditor ("name", marker.getName());
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    te::Clip::Ptr keepAlive (&marker);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, keepAlive] (int r)
    {
        if (r == 1 && w->getTextEditorContents ("name").isNotEmpty())
            keepAlive->setName (w->getTextEditorContents ("name"));
    }));
}

void TimelineRulerComponent::promptForTempoChange (te::TimePosition time)
{
    auto w = std::make_shared<juce::AlertWindow> ("Insert Tempo Change",
                                                  "BPM from this point onwards:",
                                                  juce::AlertWindow::QuestionIcon);
    w->addTextEditor ("bpm", juce::String (editRef.tempoSequence.getBpmAt (time), 1));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create (
        [w, this, time] (int r)
        {
            if (r != 1)
                return;

            const double bpm = w->getTextEditorContents ("bpm").getDoubleValue();
            if (bpm < 20.0 || bpm > 300.0)
                return;

            if (auto tempo = editRef.tempoSequence.insertTempo (time))
            {
                tempo->setBpm (bpm);
                notifyTempoMapChanged();
            }
        }));
}

void TimelineRulerComponent::showRulerContextMenu (const juce::MouseEvent& e)
{
    enum MenuIds
    {
        addMarkerId = 1,
        renameMarkerId,
        deleteMarkerId,
        insertTempoId,
        removeTempoId,
        removeTimeSigId,
        timeSigBaseId = 100
    };

    const auto clickTime = juce::jmax (te::TimePosition(),
                                       TimelineGrid::snapTime (editRef, editViewState, timeForX (e.x)));
    auto* nearbyMarker = markerNearX (e.x);
    auto& sequence = editRef.tempoSequence;

    // A tempo/time-sig change is "here" if it sits within a small pixel radius
    int nearbyTempoIndex = -1;
    for (int i = 1; i < sequence.getNumTempos(); ++i)
        if (auto* tempo = sequence.getTempo (i))
            if (std::abs (xForTime (sequence.toTime (tempo->getStartBeat())) - e.x) <= 8)
                nearbyTempoIndex = i;

    int nearbyTimeSigIndex = -1;
    for (int i = 1; i < sequence.getNumTimeSigs(); ++i)
        if (auto* sig = sequence.getTimeSig (i))
            if (std::abs (xForTime (sequence.toTime (sig->getStartBeat())) - e.x) <= 8)
                nearbyTimeSigIndex = i;

    juce::PopupMenu menu;
    menu.addItem (addMarkerId, "Add Marker Here");

    if (nearbyMarker != nullptr)
    {
        menu.addItem (renameMarkerId, "Rename \"" + nearbyMarker->getName() + "\"...");
        menu.addItem (deleteMarkerId, "Delete \"" + nearbyMarker->getName() + "\"");
    }

    menu.addSeparator();
    menu.addItem (insertTempoId, "Insert Tempo Change Here...");

    juce::PopupMenu timeSigMenu;
    const juce::StringArray timeSigs { "4/4", "3/4", "6/8", "2/4", "5/4", "7/8", "12/8" };
    for (int i = 0; i < timeSigs.size(); ++i)
        timeSigMenu.addItem (timeSigBaseId + i, timeSigs[i]);
    menu.addSubMenu ("Insert Time Signature Here", timeSigMenu);

    if (nearbyTempoIndex > 0)
        menu.addItem (removeTempoId, "Remove Tempo Change");

    if (nearbyTimeSigIndex > 0)
        menu.addItem (removeTimeSigId, "Remove Time Signature Change");

    const te::Clip::Ptr markerRef (nearbyMarker);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                            .withTargetScreenArea ({ e.getScreenX(), e.getScreenY(), 1, 1 }),
                        [this, clickTime, markerRef, nearbyTempoIndex, nearbyTimeSigIndex, timeSigs] (int result)
    {
        auto& seq = editRef.tempoSequence;

        if (result >= timeSigBaseId)
        {
            const auto sigText = timeSigs[result - timeSigBaseId];
            if (auto sig = seq.insertTimeSig (clickTime))
            {
                sig->setStringTimeSig (sigText);
                notifyTempoMapChanged();
            }
            return;
        }

        switch (result)
        {
            case addMarkerId:
                editRef.getMarkerManager().createMarker (-1, clickTime, {}, &editViewState.selectionManager);
                repaint();
                break;

            case renameMarkerId:
                if (auto* marker = dynamic_cast<te::MarkerClip*> (markerRef.get()))
                    renameMarker (*marker);
                break;

            case deleteMarkerId:
                if (markerRef != nullptr)
                    markerRef->removeFromParent();
                repaint();
                break;

            case insertTempoId:
                promptForTempoChange (clickTime);
                break;

            case removeTempoId:
                if (nearbyTempoIndex > 0 && nearbyTempoIndex < seq.getNumTempos())
                {
                    seq.removeTempo (nearbyTempoIndex, false);
                    notifyTempoMapChanged();
                }
                break;

            case removeTimeSigId:
                if (nearbyTimeSigIndex > 0 && nearbyTimeSigIndex < seq.getNumTimeSigs())
                {
                    seq.removeTimeSig (nearbyTimeSigIndex);
                    notifyTempoMapChanged();
                }
                break;

            default:
                break;
        }
    });
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

TimelineComponent::TimelineComponent (te::Edit& e, te::SelectionManager& sm, te::EditInsertPoint* ip,
                                      UiTelemetryHub* hub)
    : edit (e),
      editViewState (edit, sm, ip),
      telemetryHub (hub),
      playhead (edit, editViewState, hub),
      ruler (edit, editViewState)
{
    addAndMakeVisible (headerViewport);
    addAndMakeVisible (timelineViewport);
    addAndMakeVisible (ruler);
    addAndMakeVisible (gridButton);
    addAndMakeVisible (snapButton);
    addAndMakeVisible (rippleButton);
    addAndMakeVisible (gridDivisionBox);

    addAndMakeVisible (gridDivisionBox);
    addAndMakeVisible (hScrollBarOverlay);

    headerViewport.setViewedComponent (&headerContent, false);
    headerViewport.setScrollBarsShown (false, false);

    timelineViewport.setViewedComponent (&timelineContent, false);
    timelineViewport.setScrollBarsShown (true, false);
    timelineViewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    timelineViewport.getVerticalScrollBar().addListener (this);
    hScrollBarOverlay.addListener (this);

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

    ruler.onTempoMapChanged = [this]
    {
        // Tempo edits change the beat<->time mapping, so clip bounds, lane
        // backgrounds and the timeline width must all be recalculated.
        updateTimelineWidth();
        repaintGrid();
    };

    edit.state.addListener (this);

    buildTracks();
}

TimelineComponent::~TimelineComponent()
{
    timelineViewport.getVerticalScrollBar().removeListener (this);
    hScrollBarOverlay.removeListener (this);
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

bool TimelineComponent::performCommand (int commandID)
{
    using namespace AppCommandIDs;

    switch (commandID)
    {
        case toggleGrid:
            toggleShowGrid();
            return true;
        case duplicateClips:
            duplicateSelectedClips();
            return true;
        case groupClips:
            groupSelectedClips (true);
            return true;
        case ungroupClips:
            groupSelectedClips (false);
            return true;
        case toggleRipple:
            toggleRippleMode();
            return true;
        case deleteTimelineSelection:
            return deleteSelectedClips();
        case addMarker:
            edit.getMarkerManager().createMarker (-1, edit.getTransport().getPosition(), {},
                                                  &editViewState.selectionManager);
            return true;
        case prevMarker:
            jumpToMarker (false);
            return true;
        case nextMarker:
            jumpToMarker (true);
            return true;
        case toggleTakeLanes:
        {
            const auto clips = editViewState.selectionManager.getItemsOfType<te::Clip>();
            if (clips.size() == 1 && EngineHelpers::hasMultipleTakes (*clips.getFirst()))
            {
                EngineHelpers::toggleTakeLanesExpanded (editViewState, *clips.getFirst());
                layoutTracks();
            }
            return true;
        }
        case consolidateClips:
            return consolidateSelectedClips();
        default:
            break;
    }

    return false;
}

bool TimelineComponent::handleKeyPress (const juce::KeyPress& key)
{
    juce::ignoreUnused (key);
    return false;
}

void TimelineComponent::jumpToMarker (bool next)
{
    auto& markers = edit.getMarkerManager();
    const auto pos = edit.getTransport().getPosition();

    if (auto* marker = next ? markers.getNextMarker (pos) : markers.getPrevMarker (pos))
        edit.getTransport().setPosition (marker->getPosition().getStart());
    else if (! next)
        edit.getTransport().setPosition (0s);
}

void TimelineComponent::toggleRippleMode()
{
    editViewState.rippleMode = ! editViewState.rippleMode.get();
    rippleButton.setToggleState (editViewState.rippleMode.get(), juce::dontSendNotification);
}

bool TimelineComponent::consolidateSelectedClips()
{
    juce::String error;
    const auto created = EngineHelpers::consolidateClips (edit, editViewState.selectionManager, &error);

    if (created.isEmpty() && error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Consolidate", error);
        return false;
    }

    if (! created.isEmpty())
    {
        layoutTracks();
        if (onClipSelectionChanged)
            onClipSelectionChanged();
    }

    return ! created.isEmpty();
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
    juce::StringArray oldOuterGroupIds, newOuterGroupIds;
    auto remapGroupId = [&oldGroupIds, &newGroupIds] (const juce::String& oldId)
    {
        if (oldId.isEmpty())
            return juce::String();

        const int idx = oldGroupIds.indexOf (oldId);
        if (idx >= 0)
            return newGroupIds[idx];

        const auto newId = juce::Uuid().toString();
        oldGroupIds.add (oldId);
        newGroupIds.add (newId);
        return newId;
    };
    auto remapOuterGroupId = [&oldOuterGroupIds, &newOuterGroupIds] (const juce::String& oldId)
    {
        if (oldId.isEmpty())
            return juce::String();

        const int idx = oldOuterGroupIds.indexOf (oldId);
        if (idx >= 0)
            return newOuterGroupIds[idx];

        const auto newId = juce::Uuid().toString();
        oldOuterGroupIds.add (oldId);
        newOuterGroupIds.add (newId);
        return newId;
    };

    juce::Array<te::Clip*> newClips;

    for (auto* clip : clips)
    {
        if (auto* copy = EngineHelpers::duplicateClip (*clip, true))
        {
            rippleAfterInsert (*clip, *copy);

            const auto originalGroupId = EngineHelpers::getClipGroup (*clip);
            const auto originalOuterId = EngineHelpers::getClipOuterGroup (*clip);
            if (originalGroupId.isNotEmpty())
            {
                EngineHelpers::setClipGroup (*copy, remapGroupId (originalGroupId));
                EngineHelpers::setClipGroupColour (*copy, EngineHelpers::getClipGroupColour (*clip));
            }
            if (originalOuterId.isNotEmpty())
                EngineHelpers::setClipOuterGroup (*copy, remapOuterGroupId (originalOuterId));

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

    juce::StringArray affectedGroups, affectedOuterGroups;
    for (auto* clip : clips)
    {
        const auto groupId = EngineHelpers::getClipGroup (*clip);
        if (groupId.isNotEmpty())
            affectedGroups.addIfNotAlreadyThere (groupId);

        const auto outerId = EngineHelpers::getClipOuterGroup (*clip);
        if (outerId.isNotEmpty())
            affectedOuterGroups.addIfNotAlreadyThere (outerId);
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

    for (const auto& outerId : affectedOuterGroups)
    {
        const auto remaining = EngineHelpers::getClipsSharingOuterGroup (editViewState.edit, outerId);
        if (remaining.size() == 1)
            EngineHelpers::setClipOuterGroup (*remaining.getFirst(), {});
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

    if (! group)
    {
        bool anyHadOuter = false;
        for (auto* clip : clips)
            if (EngineHelpers::getClipOuterGroup (*clip).isNotEmpty())
                anyHadOuter = true;

        for (auto* clip : clips)
        {
            if (anyHadOuter)
                EngineHelpers::setClipOuterGroup (*clip, {});
            else
                EngineHelpers::setClipGroup (*clip, {});
        }

        repaintGrid();
        return;
    }

    juce::StringArray innerGroups;
    bool hasUngrouped = false;

    for (auto* clip : clips)
    {
        const auto inner = EngineHelpers::getClipGroup (*clip);
        if (inner.isEmpty())
            hasUngrouped = true;
        else
            innerGroups.addIfNotAlreadyThere (inner);
    }

    if (innerGroups.size() > 1 || (innerGroups.size() == 1 && hasUngrouped))
    {
        juce::String sharedOuter;
        for (auto* clip : clips)
        {
            const auto outer = EngineHelpers::getClipOuterGroup (*clip);
            if (outer.isEmpty())
                continue;

            if (sharedOuter.isEmpty())
                sharedOuter = outer;
            else if (sharedOuter != outer)
                sharedOuter = juce::String();
        }

        if (sharedOuter.isNotEmpty())
            return;

        const auto outerId = juce::Uuid().toString();
        for (auto* clip : clips)
            EngineHelpers::setClipOuterGroup (*clip, outerId);
    }
    else
    {
        juce::String sharedInner;
        for (auto* clip : clips)
        {
            const auto inner = EngineHelpers::getClipGroup (*clip);
            if (inner.isEmpty())
                continue;

            if (sharedInner.isEmpty())
                sharedInner = inner;
            else if (sharedInner != inner)
                sharedInner = juce::String();
        }

        if (sharedInner.isNotEmpty())
            return;

        const auto groupId = juce::Uuid().toString();
        const auto colour = EngineHelpers::colourForGroupId (groupId);

        for (auto* clip : clips)
        {
            EngineHelpers::setClipGroup (*clip, groupId);
            EngineHelpers::setClipGroupColour (*clip, colour);
        }
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

int TimelineComponent::trackRowIndexAtContentY (int contentY) const
{
    for (int i = 0; i < trackRows.size(); ++i)
    {
        const auto& row = trackRows.getReference (i);
        if (contentY >= row.y && contentY < row.y + row.height)
            return i;
    }

    return -1;
}

te::ClipTrack* TimelineComponent::clipTrackForRowIndex (int rowIndex) const
{
    if (! juce::isPositiveAndBelow (rowIndex, trackRows.size()))
        return nullptr;

    return dynamic_cast<te::ClipTrack*> (trackRows.getReference (rowIndex).track.get());
}

void TimelineComponent::clearCrossTrackDragState()
{
    crossTrackDrag = {};
    repaint();
}

void TimelineComponent::handleClipCrossTrackDragMove (te::Clip& clip, const juce::MouseEvent& e)
{
    auto* lane = e.eventComponent != nullptr
                     ? e.eventComponent->findParentComponentOfClass<TrackLaneComponent>()
                     : nullptr;

    if (lane == nullptr)
        return;

    const int contentY = lane->getY() + e.getEventRelativeTo (lane).y;

    crossTrackDrag.active = true;
    crossTrackDrag.clipId = clip.itemID;
    crossTrackDrag.ghostStart = clip.getPosition().getStart();

    if (auto* sourceTrack = clip.getClipTrack())
        crossTrackDrag.sourceRowIndex = EngineHelpers::getArrangementTrackIndex (edit, *sourceTrack);

    crossTrackDrag.targetRowIndex = trackRowIndexAtContentY (contentY);

    if (crossTrackDrag.targetRowIndex >= 0)
    {
        if (auto* dest = clipTrackForRowIndex (crossTrackDrag.targetRowIndex))
            crossTrackDrag.validDrop = EngineHelpers::canMoveClipToTrack (clip, *dest);
        else
            crossTrackDrag.validDrop = false;
    }
    else
    {
        crossTrackDrag.validDrop = false;
    }

    repaint();
}

void TimelineComponent::handleClipCrossTrackDragEnd (te::Clip& clip, const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);

    if (crossTrackDrag.active
        && crossTrackDrag.validDrop
        && crossTrackDrag.targetRowIndex >= 0
        && crossTrackDrag.targetRowIndex != crossTrackDrag.sourceRowIndex)
    {
        if (auto* dest = clipTrackForRowIndex (crossTrackDrag.targetRowIndex))
            EngineHelpers::moveClipGroupToTrack (clip, *dest, clip.getPosition().getStart());
    }

    clearCrossTrackDragState();
}

void TimelineComponent::paintCrossTrackDropOverlay (juce::Graphics& g)
{
    if (! crossTrackDrag.active || ! juce::isPositiveAndBelow (crossTrackDrag.targetRowIndex, trackRows.size()))
        return;

    const auto& row = trackRows.getReference (crossTrackDrag.targetRowIndex);
    const int viewY = timelineViewport.getViewPositionY();
    const auto laneBounds = juce::Rectangle<int> (headerWidth + timelineViewport.getX(),
                                                  timelineViewport.getY() + row.y - viewY,
                                                  timelineViewport.getViewWidth(),
                                                  row.height);

    g.setColour (crossTrackDrag.validDrop ? AppColours::accentValidDrop (AppLookAndFeel::getCurrentTheme()).withAlpha (0.25f)
                                          : AppColours::accentInvalidDrop (AppLookAndFeel::getCurrentTheme()).withAlpha (0.25f));
    g.fillRect (laneBounds);

    const int ghostX = headerWidth + timelineViewport.getX()
                       + editViewState.timeToX (crossTrackDrag.ghostStart)
                       - timelineViewport.getViewPositionX();
    g.setColour (crossTrackDrag.validDrop ? juce::Colours::white.withAlpha (0.8f)
                                          : juce::Colours::red.withAlpha (0.8f));
    g.drawVerticalLine (ghostX, (float) laneBounds.getY(), (float) laneBounds.getBottom());
}

void TimelineComponent::paintOverChildren (juce::Graphics& g)
{
    paintCrossTrackDropOverlay (g);
}

void TimelineComponent::syncGridControls()
{
    gridButton.setToggleState (editViewState.showGrid.get(), juce::dontSendNotification);
}

void TimelineComponent::scrollBarMoved (juce::ScrollBar* scrollBarThatHasMoved, double newRangeStart)
{
    if (scrollBarThatHasMoved == &hScrollBarOverlay)
    {
        timelineViewport.setViewPosition ((int) newRangeStart, timelineViewport.getViewPositionY());
        syncVisibleRange();
        refreshLaneLayouts();
        ruler.repaint();
    }
    else if (scrollBarThatHasMoved == &timelineViewport.getVerticalScrollBar())
    {
        const int y = timelineViewport.getViewPositionY();
        headerViewport.setViewPosition (0, y);
        editViewState.viewY = y;
        refreshVisibleTracks();
    }
}

void TimelineComponent::updateHorizontalScrollBarOverlay()
{
    const int contentW = timelineContent.getWidth();
    const int viewW = juce::jmax (1, timelineViewport.getViewWidth());
    hScrollBarOverlay.setRangeLimits (0.0, (double) contentW);
    hScrollBarOverlay.setCurrentRange ((double) timelineViewport.getViewPositionX(),
                                       (double) viewW,
                                       juce::dontSendNotification);
    hScrollBarOverlay.setVisible (contentW > viewW);
}

void TimelineComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateTracks))
        buildTracks();
    else if (compareAndReset (relayoutTracks))
    {
        rebuildTrackRowList();
        refreshVisibleTracks();
    }
}

void TimelineComponent::invalidateLaneBackgrounds()
{
    editViewState.laneBackgroundCache.invalidateAll();
}

void TimelineComponent::syncVisibleRange()
{
    editViewState.syncVisibleRangeFromScroll (timelineViewport.getViewPositionX(),
                                              timelineViewport.getViewWidth());
}

void TimelineComponent::refreshLaneLayouts()
{
    const bool laneLevel = useLaneLevelRendering (editViewState.getPixelsPerBeat());
    const bool modeChanged = laneLevel != laneLevelRenderingActive;
    laneLevelRenderingActive = laneLevel;

    for (auto* lane : trackLanes)
    {
        lane->refreshLayout();

        if (laneLevel || modeChanged)
            lane->repaint();
    }
}

void TimelineComponent::updateTimelineWidth()
{
    const int width = editViewState.getTimelineWidthPx();

    for (auto* lane : trackLanes)
        lane->setSize (width, lane->getHeight());

    timelineContent.setSize (width, timelineContent.getHeight());
    playhead.setSize (width, playhead.getHeight());
    syncVisibleRange();
    invalidateLaneBackgrounds();
    refreshLaneLayouts();
    updateHorizontalScrollBarOverlay();
}

void TimelineComponent::repaintGrid()
{
    invalidateLaneBackgrounds();
    ruler.repaint();

    for (auto* lane : trackLanes)
        lane->repaint();
}

void TimelineComponent::destroyAllVisibleTracks()
{
    trackLanes.clear();
    trackHeaders.clear();
    trackFooters.clear();
    timelineContent.removeAllChildren();
    headerContent.removeAllChildren();
    timelineContent.addAndMakeVisible (playhead);
}

void TimelineComponent::createVisibleTrackUI (const TrackRowInfo& row)
{
    auto lane = std::make_unique<TrackLaneComponent> (editViewState, row.track);
    lane->onClipDoubleClick = [this] (te::Clip& c)
    {
        if (onClipDoubleClick)
            onClipDoubleClick (c);
    };
    lane->onClipSelectionChanged = [this]
    {
        if (onClipSelectionChanged)
            onClipSelectionChanged();
    };
    lane->onShowClipProperties = [this]
    {
        if (onShowClipProperties)
            onShowClipProperties();
    };
    lane->createPlugin = createPlugin;
    lane->onAddPlugin = onAddPlugin;
    lane->onClipCrossTrackDragMove = [this] (te::Clip& c, const juce::MouseEvent& e)
    {
        handleClipCrossTrackDragMove (c, e);
    };
    lane->onClipCrossTrackDragEnd = [this] (te::Clip& c, const juce::MouseEvent& e)
    {
        handleClipCrossTrackDragEnd (c, e);
    };
    lane->onTakeLanesChanged = [this]
    {
        layoutTracks();
    };
    trackLanes.add (lane.release());

    auto header = std::make_unique<TrackHeaderComponent> (editViewState, row.track);
    header->onTrackSelected = [this] (te::Track& t)
    {
        if (onTrackSelected)
            onTrackSelected (t);
    };
    trackHeaders.add (header.release());

    if (editViewState.showFooters)
    {
        auto footer = std::make_unique<TrackFooterComponent> (editViewState, row.track);
        footer->onAddPlugin = [this] (te::Track& t)
        {
            if (onAddPlugin)
                onAddPlugin (t);
        };
        footer->createPlugin = createPlugin;
        trackFooters.add (footer.release());
    }
}

void TimelineComponent::rebuildTrackRowList()
{
    trackRows.clear();

    const int trackH = juce::jlimit (minTrackHeight, maxTrackHeight, editViewState.trackHeight.get());
    const int width = editViewState.getTimelineWidthPx();
    int y = 0;

    for (auto track : te::getAllTracks (edit))
    {
        if (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()
            || track->isMasterTrack() || track->isArrangerTrack())
            continue;

        const int takeExtra = EngineHelpers::getTakeLaneExtraHeight (editViewState, *track);
        trackRows.add ({ track, y, trackH + takeExtra, takeExtra });
        y += trackH + takeExtra;
    }

    timelineContent.setSize (width, y);
    headerContent.setSize (headerWidth, y);
    playhead.setBounds (0, 0, width, y);
    playhead.toFront (false);
}

void TimelineComponent::refreshVisibleTracks()
{
    if (trackRows.isEmpty())
    {
        destroyAllVisibleTracks();
        return;
    }

    const int viewY = timelineViewport.getViewPositionY();
    const int viewH = juce::jmax (1, timelineViewport.getHeight());
    const int margin = juce::jmax (verticalVirtualizationMargin, viewH / 2);
    const int visibleStartY = viewY - margin;
    const int visibleEndY = viewY + viewH + margin;
    const int width = editViewState.getTimelineWidthPx();

    juce::Array<te::EditItemID> desiredIds;

    for (const auto& row : trackRows)
    {
        if (row.y + row.height >= visibleStartY && row.y <= visibleEndY)
            desiredIds.add (row.track->itemID);
    }

    for (int i = trackLanes.size(); --i >= 0;)
    {
        if (! desiredIds.contains (trackLanes[i]->getTrack().itemID))
        {
            trackLanes.remove (i);
            trackHeaders.remove (i);
            if (i < trackFooters.size())
                trackFooters.remove (i);
        }
    }

    for (const auto& row : trackRows)
    {
        if (! desiredIds.contains (row.track->itemID))
            continue;

        bool found = false;
        for (auto* lane : trackLanes)
        {
            if (lane->getTrack().itemID == row.track->itemID)
            {
                found = true;
                break;
            }
        }

        if (! found)
            createVisibleTrackUI (row);
    }

    timelineContent.removeAllChildren();
    headerContent.removeAllChildren();
    timelineContent.addAndMakeVisible (playhead);

    for (int i = 0; i < trackLanes.size(); ++i)
    {
        const auto trackId = trackLanes[i]->getTrack().itemID;
        const TrackRowInfo* row = nullptr;

        for (const auto& candidate : trackRows)
        {
            if (candidate.track->itemID == trackId)
            {
                row = &candidate;
                break;
            }
        }

        if (row == nullptr)
            continue;

        trackLanes[i]->setBounds (0, row->y, width, row->height);
        timelineContent.addAndMakeVisible (trackLanes[i]);

        if (auto* header = trackHeaders[i])
        {
            header->setBounds (0, row->y, headerWidth, row->height);
            headerContent.addAndMakeVisible (header);
        }

        if (i < trackFooters.size())
        {
            if (auto* footer = trackFooters[i])
            {
                footer->setBounds (0, row->y + row->height - footerHeight, headerWidth, footerHeight);
                headerContent.addAndMakeVisible (footer);
            }
        }
    }

    playhead.toFront (false);
    refreshLaneLayouts();
}

void TimelineComponent::buildTracks()
{
    editViewState.waveformCache.clear();
    invalidateLaneBackgrounds();
    destroyAllVisibleTracks();
    rebuildTrackRowList();
    refreshVisibleTracks();

    timelineViewport.setViewPosition (timelineViewport.getViewPositionX(), editViewState.viewY.get());
    headerViewport.setViewPosition (0, timelineViewport.getViewPositionY());
    updateHorizontalScrollBarOverlay();
}

void TimelineComponent::layoutTracks()
{
    rebuildTrackRowList();
    invalidateLaneBackgrounds();
    refreshVisibleTracks();
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

    hScrollBarOverlay.setBounds (timelineViewport.getBounds().withHeight (hScrollBarHeight));
    hScrollBarOverlay.toFront (false);

    updateTimelineWidth();
    updateHorizontalScrollBarOverlay();
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
        updateHorizontalScrollBarOverlay();
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
            updateHorizontalScrollBarOverlay();
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
            refreshVisibleTracks();
        }
    }
}

} // namespace skeletonhive
