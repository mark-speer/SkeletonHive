#include "ClipComponents.h"
#include "TimelineGrid.h"
#include "TrackComponents.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
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

void drawMidiClipDensity (juce::Graphics& g, te::MidiClip& clip, juce::Rectangle<int> area)
{
    if (area.isEmpty())
        return;

    const int numNotes = clip.getSequence().getNumNotes();
    if (numNotes == 0)
        return;

    static constexpr int buckets = 12;
    float density[buckets] {};
    const double clipLength = clip.getPosition().getLength().inSeconds();

    if (clipLength <= 0.0)
        return;

    for (int i = 0; i < numNotes; ++i)
    {
        const auto* note = clip.getSequence().getNote (i);
        const double rel = (note->getEditStartTime (clip) - clip.getPosition().getStart()).inSeconds() / clipLength;
        const int bucket = juce::jlimit (0, buckets - 1, (int) (rel * buckets));
        density[bucket] += 1.0f;
    }

    float maxDensity = 0.0f;
    for (float d : density)
        maxDensity = juce::jmax (maxDensity, d);

    if (maxDensity <= 0.0f)
        return;

    const float bucketWidth = (float) area.getWidth() / (float) buckets;
    g.setColour (juce::Colours::white.withAlpha (0.55f));

    for (int i = 0; i < buckets; ++i)
    {
        const float normalised = density[i] / maxDensity;
        const float barHeight = juce::jmax (2.0f, normalised * (float) area.getHeight() * 0.75f);
        const float x = (float) area.getX() + bucketWidth * (float) i;
        g.fillRect (x + 1.0f,
                    (float) area.getBottom() - barHeight,
                    juce::jmax (1.0f, bucketWidth - 2.0f),
                    barHeight);
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

TimelineClipDetailLevel ClipComponent::getDetailLevel() const
{
    return getClipDetailLevel (editViewState.getPixelsPerBeat(), getWidth());
}

te::TimePosition ClipComponent::snapTime (te::TimePosition time) const
{
    return TimelineGrid::snapTime (editViewState.edit, editViewState, time);
}

ClipComponent::DragMode ClipComponent::dragModeForEvent (const juce::MouseEvent& e) const
{
    if (e.mods.isRightButtonDown())
        return DragMode::none;

    // Fade handles live in the top corners, only on audio clips (TE fades are
    // an AudioClipBase feature; MIDI clips have no equivalent).
    if (shouldShowFadeHandles (getDetailLevel()))
    {
        if (dynamic_cast<te::AudioClipBase*> (clip.get()) != nullptr)
        {
            if (e.y <= fadeHandlePx && e.x <= fadeHandlePx * 2)
                return DragMode::fadeIn;
            if (e.y <= fadeHandlePx && e.x >= getWidth() - fadeHandlePx * 2)
                return DragMode::fadeOut;
        }
    }

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
    else if (mode == DragMode::fadeIn || mode == DragMode::fadeOut)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

void ClipComponent::paint (juce::Graphics& g)
{
    const auto detail = getDetailLevel();
    g.setColour (EngineHelpers::getClipFillColour (*clip, juce::Colours::darkgrey));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), detail == TimelineClipDetailLevel::Summary ? 2.0f : 4.0f);

    if (detail != TimelineClipDetailLevel::Summary)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 1.0f);
    }

    if (shouldShowClipLabel (detail, getWidth()))
    {
        g.setColour (juce::Colours::white);
        g.drawText (clip->getName(), getLocalBounds().reduced (4), juce::Justification::centredLeft, true);
    }

    paintSelectionAndGroupIndicators (g);
}

void ClipComponent::paintSelectionAndGroupIndicators (juce::Graphics& g) const
{
    if (editViewState.selectionManager.isSelected (clip.get()))
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 4.0f, 2.0f);
    }

    if (EngineHelpers::getClipOuterGroup (*clip).isNotEmpty())
    {
        g.setColour (EngineHelpers::colourForGroupId (EngineHelpers::getClipOuterGroup (*clip)));
        g.fillRect (getWidth() - 10, 0, 10, 6);
    }

    if (EngineHelpers::getClipGroup (*clip).isNotEmpty())
    {
        g.setColour (EngineHelpers::getClipGroupColour (*clip));
        g.fillRect (getWidth() - 10, EngineHelpers::getClipOuterGroup (*clip).isNotEmpty() ? 7 : 0, 10, 6);
    }
}

te::AudioFadeCurve::Type ClipComponent::nextFadeCurveType (te::AudioFadeCurve::Type current)
{
    switch (current)
    {
        case te::AudioFadeCurve::Type::linear:  return te::AudioFadeCurve::Type::convex;
        case te::AudioFadeCurve::Type::convex:  return te::AudioFadeCurve::Type::concave;
        case te::AudioFadeCurve::Type::concave: return te::AudioFadeCurve::Type::sCurve;
        default:                                return te::AudioFadeCurve::Type::linear;
    }
}

bool ClipComponent::isFadeHandleZone (const juce::MouseEvent& e, bool& fadeIn) const
{
    if (! shouldShowFadeHandles (getDetailLevel()))
        return false;

    if (dynamic_cast<te::AudioClipBase*> (clip.get()) == nullptr)
        return false;

    if (e.y <= fadeHandlePx && e.x <= fadeHandlePx * 2)
    {
        fadeIn = true;
        return true;
    }

    if (e.y <= fadeHandlePx && e.x >= getWidth() - fadeHandlePx * 2)
    {
        fadeIn = false;
        return true;
    }

    return false;
}

void ClipComponent::cycleFadeCurveType (bool fadeIn)
{
    if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get()))
    {
        if (fadeIn)
            audioClip->setFadeInType (nextFadeCurveType (audioClip->getFadeInType()));
        else
            audioClip->setFadeOutType (nextFadeCurveType (audioClip->getFadeOutType()));

        audioClip->checkFadeLengthsForOverrun();
        repaint();
    }
}

void ClipComponent::showFadeCurveMenu (bool fadeIn, juce::Point<int> screenPosition)
{
    auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get());
    if (audioClip == nullptr)
        return;

    const auto current = fadeIn ? audioClip->getFadeInType() : audioClip->getFadeOutType();

    juce::PopupMenu menu;
    menu.addItem (1, "Linear",   true, current == te::AudioFadeCurve::Type::linear);
    menu.addItem (2, "Convex",   true, current == te::AudioFadeCurve::Type::convex);
    menu.addItem (3, "Concave",  true, current == te::AudioFadeCurve::Type::concave);
    menu.addItem (4, "S-Curve",  true, current == te::AudioFadeCurve::Type::sCurve);

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [safeClip = clip, fadeIn] (int result)
    {
        if (result < 1 || result > 4)
            return;

        if (auto* ac = dynamic_cast<te::AudioClipBase*> (safeClip.get()))
        {
            const auto type = static_cast<te::AudioFadeCurve::Type> (result - 1);

            if (fadeIn)
                ac->setFadeInType (type);
            else
                ac->setFadeOutType (type);

            ac->checkFadeLengthsForOverrun();
        }
    });
}

void ClipComponent::applyAutoCrossfadeForClip (te::Clip& c) const
{
    if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (&c))
    {
        auto enableIfOverlapping = [] (te::AudioClipBase* a, te::AudioClipBase::ClipDirection dir)
        {
            if (auto* neighbour = a->getOverlappingClip (dir))
            {
                a->setAutoCrossfade (true);
                neighbour->setAutoCrossfade (true);
            }
        };

        enableIfOverlapping (audioClip, te::AudioClipBase::ClipDirection::previous);
        enableIfOverlapping (audioClip, te::AudioClipBase::ClipDirection::next);
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

        bool fadeIn = false;
        if (isFadeHandleZone (e, fadeIn))
        {
            showFadeCurveMenu (fadeIn, e.getScreenPosition());
            return;
        }

        showTimelineContextMenu (*this, e.getScreenPosition(), editViewState, clip->getTrack(), false, nullptr, clip.get(),
                                 onShowClipProperties, onTakeLanesChanged,
                                 [this]
                                 {
                                     if (onTakeLanesChanged)
                                         onTakeLanesChanged();
                                     if (onSelectionChanged)
                                         onSelectionChanged();
                                 });
        if (onSelectionChanged)
            onSelectionChanged();
        return;
    }

    bool fadeIn = false;
    if (e.mods.isAltDown() && isFadeHandleZone (e, fadeIn))
    {
        cycleFadeCurveType (fadeIn);
        return;
    }

    editViewState.selectionManager.selectOnly (clip.get());

    if (onSelectionChanged)
        onSelectionChanged();

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
            const auto originalOuterId = EngineHelpers::getClipOuterGroup (*clip);
            if (originalGroupId.isNotEmpty())
            {
                EngineHelpers::setClipGroup (*leftBehind, juce::Uuid().toString());
                EngineHelpers::setClipGroupColour (*leftBehind, EngineHelpers::getClipGroupColour (*clip));
            }
            if (originalOuterId.isNotEmpty())
                EngineHelpers::setClipOuterGroup (*leftBehind, juce::Uuid().toString());
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
    else if (dragMode == DragMode::resizeStart || dragMode == DragMode::resizeEnd)
    {
        captureGroupResizeItems();
        if (dragMode == DragMode::resizeEnd)
            captureRippleDragItems (originalEnd);
    }
    else if (dragMode == DragMode::fadeIn || dragMode == DragMode::fadeOut)
    {
        if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get()))
        {
            originalFadeIn = audioClip->getFadeIn();
            originalFadeOut = audioClip->getFadeOut();
        }
    }

    updateCursorForMode (dragMode);
}

void ClipComponent::captureGroupDragItems()
{
    groupDragItems.clear();

    for (auto* member : EngineHelpers::getGroupedPeers (*clip))
        groupDragItems.add ({ member, member->getPosition().getStart() });
}

void ClipComponent::captureGroupResizeItems()
{
    groupResizeItems.clear();

    for (auto* member : EngineHelpers::getGroupedPeers (*clip))
    {
        const auto pos = member->getPosition();
        groupResizeItems.add ({ member, pos.getStart(), pos.getEnd() });
    }
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
    const auto outerId = EngineHelpers::getClipOuterGroup (*clip);

    for (auto* c : EngineHelpers::getClipsStartingAfter (*track, anchor))
    {
        if (c == clip.get())
            continue;
        if (groupId.isNotEmpty() && EngineHelpers::getClipGroup (*c) == groupId)
            continue;
        if (outerId.isNotEmpty() && EngineHelpers::getClipOuterGroup (*c) == outerId)
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

        if (onCrossTrackDragMove)
            onCrossTrackDragMove (*clip, e);
    }
    else if (dragMode == DragMode::resizeStart)
    {
        const auto newStart = snapTime (currentTime);
        if (originalEnd - newStart >= minLength)
        {
            const auto effectiveDelta = newStart - originalStart;
            clip->setStart (newStart, false, false);
            clip->setEnd (originalEnd, false);

            for (auto& item : groupResizeItems)
            {
                const auto memberStart = juce::jmax (te::TimePosition(), item.originalStart + effectiveDelta);
                if (item.originalEnd - memberStart >= minLength)
                {
                    item.clip->setStart (memberStart, false, false);
                    item.clip->setEnd (item.originalEnd, false);
                }
            }
        }
    }
    else if (dragMode == DragMode::resizeEnd)
    {
        const auto newEnd = snapTime (currentTime);
        if (newEnd - originalStart >= minLength)
        {
            const auto effectiveDelta = newEnd - originalEnd;
            clip->setEnd (newEnd, false);

            for (auto& item : groupResizeItems)
            {
                const auto memberEnd = item.originalEnd + effectiveDelta;
                if (memberEnd - item.originalStart >= minLength)
                    item.clip->setEnd (memberEnd, false);
            }

            for (auto& item : rippleDragItems)
            {
                const auto memberStart = juce::jmax (te::TimePosition(), item.originalStart + effectiveDelta);
                item.clip->setStart (memberStart, false, true);
            }
        }
    }
    else if (dragMode == DragMode::fadeIn)
    {
        if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get()))
            audioClip->setFadeIn (juce::jmax (te::TimeDuration(), currentTime - originalStart));
    }
    else if (dragMode == DragMode::fadeOut)
    {
        if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get()))
            audioClip->setFadeOut (juce::jmax (te::TimeDuration(), originalEnd - currentTime));
    }
}

void ClipComponent::mouseUp (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::move || dragMode == DragMode::resizeStart || dragMode == DragMode::resizeEnd)
    {
        applyAutoCrossfadeForClip (*clip);

        for (auto& item : groupDragItems)
            applyAutoCrossfadeForClip (*item.clip);

        for (auto& item : groupResizeItems)
            applyAutoCrossfadeForClip (*item.clip);

        if (dragMode == DragMode::move && onCrossTrackDragEnd)
            onCrossTrackDragEnd (*clip, e);
    }

    dragMode = DragMode::none;
    groupDragItems.clear();
    groupResizeItems.clear();
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
    else if (mode == DragMode::fadeIn || mode == DragMode::fadeOut)
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
}

void AudioClipComponent::ensureThumbnail()
{
    if (thumbnailHeld)
        return;

    refreshThumbnailSource();
    thumbnailHeld = true;
}

void AudioClipComponent::releaseThumbnail()
{
    if (! thumbnailHeld)
        return;

    if (auto* waveClip = dynamic_cast<te::WaveAudioClip*> (clip.get()))
        editViewState.waveformCache.suggestEviction (waveClip->getAudioFile());

    thumbnail.reset();
    thumbnailHeld = false;
}

void AudioClipComponent::refreshThumbnailSource()
{
    if (auto* waveClip = dynamic_cast<te::WaveAudioClip*> (clip.get()))
    {
        const auto file = waveClip->getAudioFile();
        const auto fileKey = (juce::int64) file.getHash();

        if (thumbnail != nullptr && cachedFileKey == fileKey)
            return;

        if (thumbnail != nullptr && cachedFileKey != fileKey)
            thumbnail->setNewFile (file);

        cachedFileKey = fileKey;
        thumbnail = editViewState.waveformCache.acquire (editViewState.edit.engine,
                                                         file,
                                                         *this,
                                                         &editViewState.edit);
    }
    else
    {
        thumbnail.reset();
        cachedFileKey = 0;
    }
}

void AudioClipComponent::paint (juce::Graphics& g)
{
    const auto detail = getDetailLevel();
    auto bounds = getLocalBounds();
    const float cornerRadius = detail == TimelineClipDetailLevel::Summary ? 2.0f : 4.0f;

    g.setColour (EngineHelpers::getClipFillColour (*clip, juce::Colour (0xff2d6a4f)));
    g.fillRoundedRectangle (bounds.toFloat(), cornerRadius);

    if (shouldShowWaveforms (detail, editViewState.drawWaveforms.get()) && isVisible())
    {
        ensureThumbnail();

        if (thumbnail != nullptr)
        {
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            const te::TimeRange viewRange { editViewState.viewX1, editViewState.viewX2 };
            thumbnail->drawChannels (g, bounds, viewRange, 1.0f);
        }
    }

    if (shouldShowFadeCurves (detail))
        paintFadeOverlay (g);

    if (shouldShowClipLabel (detail, bounds.getWidth()))
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (clip->getName(), bounds.reduced (4), juce::Justification::centredLeft, true);
    }

    paintSelectionAndGroupIndicators (g);
}

void AudioClipComponent::paintFadeOverlay (juce::Graphics& g) const
{
    auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip.get());
    if (audioClip == nullptr)
        return;

    const auto bounds = getLocalBounds().toFloat();
    const double lengthSeconds = clip->getPosition().getLength().inSeconds();
    if (lengthSeconds <= 0.0 || bounds.getWidth() <= 0.0f)
        return;

    const float pxPerSecond = bounds.getWidth() / (float) lengthSeconds;

    auto drawFade = [&] (float startX, float endX, bool isFadeIn, te::AudioFadeCurve::Type type)
    {
        if (endX - startX < 1.0f)
            return;

        const int steps = juce::jlimit (2, 64, (int) (endX - startX));
        juce::Path shadow, curve;

        for (int i = 0; i <= steps; ++i)
        {
            const float alpha = (float) i / (float) steps;
            const float gain = te::AudioFadeCurve::alphaToGainForType (type, isFadeIn ? alpha : 1.0f - alpha);
            const float x = startX + alpha * (endX - startX);
            const float y = bounds.getY() + (1.0f - gain) * bounds.getHeight();

            if (i == 0)
            {
                shadow.startNewSubPath (x, bounds.getY());
                curve.startNewSubPath (x, y);
            }
            else
            {
                curve.lineTo (x, y);
            }

            shadow.lineTo (x, y);
        }

        shadow.lineTo (endX, bounds.getY());
        shadow.closeSubPath();

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (shadow);
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.strokePath (curve, juce::PathStrokeType (1.5f));
    };

    const float fadeInPx = (float) audioClip->getFadeIn().inSeconds() * pxPerSecond;
    const float fadeOutPx = (float) audioClip->getFadeOut().inSeconds() * pxPerSecond;

    drawFade (bounds.getX(), bounds.getX() + fadeInPx, true, audioClip->getFadeInType());
    drawFade (bounds.getRight() - fadeOutPx, bounds.getRight(), false, audioClip->getFadeOutType());

    // Small grab-handle indicators in the top corners
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.fillRect (bounds.getX(), bounds.getY(), (float) fadeHandlePx, 3.0f);
    g.fillRect (bounds.getRight() - (float) fadeHandlePx, bounds.getY(), (float) fadeHandlePx, 3.0f);
}

MidiClipComponent::MidiClipComponent (EditViewState& evs, te::Clip::Ptr c)
    : ClipComponent (evs, std::move (c))
{
    clip->state.addListener (this);
}

MidiClipComponent::~MidiClipComponent()
{
    clip->state.removeListener (this);
}

void MidiClipComponent::releasePreview()
{
    previewImage = {};
    previewDirty = true;
}

void MidiClipComponent::rebuildPreviewIfNeeded()
{
    if (! previewDirty && previewImage.isValid())
        return;

    auto bounds = getLocalBounds().reduced (2);
    if (bounds.isEmpty())
        return;

    previewImage = juce::Image (juce::Image::ARGB, bounds.getWidth(), bounds.getHeight(), true);
    juce::Graphics g (previewImage);
    g.fillAll (juce::Colours::transparentBlack);

    if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip.get()))
    {
        const te::TimeRange viewRange { editViewState.viewX1, editViewState.viewX2 };
        drawMidiClipPreview (g, *midiClip, bounds.withPosition (0, 0), viewRange);
    }

    previewDirty = false;
}

void MidiClipComponent::paint (juce::Graphics& g)
{
    const auto detail = getDetailLevel();
    auto bounds = getLocalBounds();
    const float cornerRadius = detail == TimelineClipDetailLevel::Summary ? 2.0f : 4.0f;

    g.setColour (EngineHelpers::getClipFillColour (*clip, juce::Colour (0xff4361ee)));
    g.fillRoundedRectangle (bounds.toFloat(), cornerRadius);

    if (isVisible())
    {
        auto previewArea = bounds.reduced (2);

        if (shouldShowMidiPreview (detail))
        {
            rebuildPreviewIfNeeded();

            if (previewImage.isValid())
            {
                g.drawImage (previewImage,
                             previewArea.getX(), previewArea.getY(),
                             previewArea.getWidth(), previewArea.getHeight(),
                             0, 0, previewImage.getWidth(), previewImage.getHeight());
            }
        }
        else if (shouldShowMidiDensity (detail))
        {
            if (auto* midiClip = dynamic_cast<te::MidiClip*> (clip.get()))
                drawMidiClipDensity (g, *midiClip, previewArea);
        }
    }

    if (shouldShowClipLabel (detail, bounds.getWidth()))
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawText (clip->getName(), bounds.reduced (4), juce::Justification::centredLeft, true);
    }

    paintSelectionAndGroupIndicators (g);
}

} // namespace skeletonhive
