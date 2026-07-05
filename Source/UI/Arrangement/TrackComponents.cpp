#include "TrackComponents.h"
#include "TimelineComponent.h"
#include "Engine/EngineHelpers.h"
#include "TracktionCommon.h"
#include "TimelineGrid.h"

namespace arrange
{

namespace
{
enum class TimelineMenuResult
{
    cut = 1,
    copy,
    paste,
    createMidiClip
};

void setInsertPoint (EditViewState& editViewState, te::Track* track, te::TimePosition time)
{
    if (editViewState.insertPoint != nullptr && track != nullptr)
        editViewState.insertPoint->setNextInsertPoint (time, track);
}

} // namespace

void showTimelineContextMenu (juce::Component& target,
                              juce::Point<int> screenPosition,
                              EditViewState& editViewState,
                              te::Track* track,
                              bool offerCreateMidiClip,
                              std::function<void()> onCreateMidiClip)
{
    juce::ignoreUnused (track);

    juce::PopupMenu menu;
    const bool hasSelection = editViewState.selectionManager.getNumObjectsSelected() > 0;
    const bool canPaste = te::Clipboard::getInstance()->getContent() != nullptr;

    menu.addItem ((int) TimelineMenuResult::cut, "Cut", hasSelection);
    menu.addItem ((int) TimelineMenuResult::copy, "Copy", hasSelection);
    menu.addItem ((int) TimelineMenuResult::paste, "Paste", canPaste);

    if (offerCreateMidiClip)
    {
        menu.addSeparator();
        menu.addItem ((int) TimelineMenuResult::createMidiClip, "Create MIDI Clip");
    }

    // Anchor at the click point, not the target's full screen bounds. Lanes can be
    // tens of thousands of pixels wide inside a scrolled viewport, so using
    // withTargetComponent alone places the menu on the wrong monitor.
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&target)
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [&editViewState, onCreateMidiClip = std::move (onCreateMidiClip)] (int result)
    {
        switch (static_cast<TimelineMenuResult> (result))
        {
            case TimelineMenuResult::cut:
                editViewState.selectionManager.cutSelected();
                break;
            case TimelineMenuResult::copy:
                editViewState.selectionManager.copySelected();
                break;
            case TimelineMenuResult::paste:
                editViewState.selectionManager.pasteSelected();
                break;
            case TimelineMenuResult::createMidiClip:
                if (onCreateMidiClip)
                    onCreateMidiClip();
                break;
            default:
                break;
        }
    });
}

TrackHeaderComponent::TrackHeaderComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (std::move (t))
{
    trackName.setText (track->getName(), juce::dontSendNotification);
    trackName.setJustificationType (juce::Justification::centredLeft);

    kindBadge.setJustificationType (juce::Justification::centred);
    kindBadge.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    kindBadge.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.95f));
    kindBadge.setInterceptsMouseClicks (false, false);
    updateKindBadge();

    armButton.onClick = [this]
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        {
            const bool arm = ! EngineHelpers::isTrackArmed (*audioTrack);
            EngineHelpers::armTrack (*audioTrack, arm);
            armButton.setToggleState (arm, juce::dontSendNotification);
            if (onArmChanged) onArmChanged (*track);
        }
    };

    muteButton.onClick = [this]
    {
        track->setMute (soloButton.getToggleState() ? false : ! track->isMuted (false));
        muteButton.setToggleState (track->isMuted (false), juce::dontSendNotification);
        if (onMuteChanged) onMuteChanged (*track);
    };

    soloButton.onClick = [this]
    {
        track->setSolo (soloButton.getToggleState());
        soloButton.setToggleState (track->isSolo (false), juce::dontSendNotification);
        if (onSoloChanged) onSoloChanged (*track);
    };

    addAndMakeVisible (trackName);
    addAndMakeVisible (kindBadge);
    addAndMakeVisible (armButton);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);

    track->state.addListener (this);
}

TrackHeaderComponent::~TrackHeaderComponent()
{
    track->state.removeListener (this);
}

void TrackHeaderComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2e));
    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TrackHeaderComponent::resized()
{
    auto r = getLocalBounds().reduced (2);
    const int btnW = 22;
    soloButton.setBounds (r.removeFromRight (btnW));
    muteButton.setBounds (r.removeFromRight (btnW));
    armButton.setBounds (r.removeFromRight (btnW));
    kindBadge.setBounds (r.removeFromLeft (42).reduced (0, 3));
    r.removeFromLeft (4);
    trackName.setBounds (r);
}

void TrackHeaderComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::name)
        trackName.setText (track->getName(), juce::dontSendNotification);
    else if (id == EngineHelpers::trackKindProperty)
        updateKindBadge();
}

void TrackHeaderComponent::updateKindBadge()
{
    const bool isMidi = EngineHelpers::getTrackKind (*track) == TrackKind::midi;
    kindBadge.setText (isMidi ? "MIDI" : "AUDIO", juce::dontSendNotification);
    kindBadge.setColour (juce::Label::backgroundColourId,
                        isMidi ? juce::Colour (0xff4361ee) : juce::Colour (0xff2d6a4f));
}

PluginSlotButton::PluginSlotButton (EditViewState& evs, te::Plugin::Ptr p)
    : TextButton (p->getName()), editViewState (evs), plugin (std::move (p))
{
    setTooltip (plugin->getName() + " (right-click for options)");
    updateEnabledLook();

    onClick = [this]
    {
        if (plugin->windowState != nullptr)
            plugin->windowState->showWindowExplicitly();
        editViewState.selectionManager.selectOnly (plugin.get());
    };
}

void PluginSlotButton::updateEnabledLook()
{
    setAlpha (plugin->isEnabled() ? 1.0f : 0.4f);
}

void PluginSlotButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showSlotMenu();
        return;
    }

    TextButton::mouseDown (e);
}

void PluginSlotButton::showSlotMenu()
{
    enum MenuIds { bypass = 1, moveLeft, moveRight, remove };

    juce::PopupMenu menu;
    menu.addItem (bypass, "Bypass", true, ! plugin->isEnabled());
    menu.addSeparator();
    menu.addItem (moveLeft, "Move Earlier");
    menu.addItem (moveRight, "Move Later");
    menu.addSeparator();
    menu.addItem (remove, "Remove");

    const auto safeThis = juce::Component::SafePointer<PluginSlotButton> (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safeThis] (int result)
    {
        if (safeThis == nullptr)
            return;

        auto& p = *safeThis->plugin;

        switch (result)
        {
            case bypass:
                p.setEnabled (! p.isEnabled());
                safeThis->updateEnabledLook();
                break;
            case moveLeft:
                if (safeThis->onMove) safeThis->onMove (p, -1);
                break;
            case moveRight:
                if (safeThis->onMove) safeThis->onMove (p, 1);
                break;
            case remove:
                if (safeThis->onRemove) safeThis->onRemove (p);
                break;
            default:
                break;
        }
    });
}

TrackFooterComponent::TrackFooterComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (std::move (t))
{
    addButton.onClick = [this]
    {
        if (onAddPlugin)
            onAddPlugin (*track);
    };
    addAndMakeVisible (addButton);
    track->pluginList.state.addListener (this);
    buildPlugins();
}

TrackFooterComponent::~TrackFooterComponent()
{
    track->pluginList.state.removeListener (this);
}

void TrackFooterComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16213e));
}

void TrackFooterComponent::resized()
{
    auto r = getLocalBounds().reduced (2);
    addButton.setBounds (r.removeFromLeft (24));
    for (auto* p : plugins)
        p->setBounds (r.removeFromLeft (80).reduced (1));
}

void TrackFooterComponent::handleAsyncUpdate()
{
    if (compareAndReset (updatePlugins))
        buildPlugins();
}

void TrackFooterComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::enabled || id == te::IDs::name)
        markAndUpdate (updatePlugins);
}

void TrackFooterComponent::buildPlugins()
{
    plugins.clear();
    for (auto plugin : track->pluginList.getPlugins())
    {
        if (dynamic_cast<te::VolumeAndPanPlugin*> (plugin) != nullptr
            || dynamic_cast<te::LevelMeterPlugin*> (plugin) != nullptr)
            continue;

        auto* slot = new PluginSlotButton (editViewState, plugin);
        slot->onRemove = [this] (te::Plugin& p) { removePlugin (p); };
        slot->onMove = [this] (te::Plugin& p, int direction) { movePlugin (p, direction); };

        plugins.add (slot);
        addAndMakeVisible (slot);
    }
    resized();
}

void TrackFooterComponent::removePlugin (te::Plugin& plugin)
{
    plugin.deleteFromParent();
}

void TrackFooterComponent::movePlugin (te::Plugin& plugin, int direction)
{
    // Find the plugin's slot position among the user-visible slots so built-in
    // volume/meter plugins are skipped when reordering.
    int slotIndex = -1;
    for (int i = 0; i < plugins.size(); ++i)
        if (plugins[i]->getPlugin().get() == &plugin)
            slotIndex = i;

    const int targetSlot = slotIndex + direction;
    if (slotIndex < 0 || targetSlot < 0 || targetSlot >= plugins.size())
        return;

    auto& listState = track->pluginList.state;
    const int fromIndex = listState.indexOf (plugin.state);
    const int toIndex = listState.indexOf (plugins[targetSlot]->getPlugin()->state);

    if (fromIndex >= 0 && toIndex >= 0)
        listState.moveChild (fromIndex, toIndex, &editViewState.edit.getUndoManager());
}

TrackLaneComponent::TrackLaneComponent (EditViewState& evs, te::Track::Ptr t)
    : editViewState (evs), track (std::move (t))
{
    // Left-drag on MIDI lanes creates a time-range highlight. Without this flag
    // the timeline viewport's scroll-on-drag steals horizontal drags (especially
    // right-to-left) before the lane can extend the selection.
    if (canDragCreateClips())
        setViewportIgnoreDragFlag (true);

    track->state.addListener (this);
    buildClips();
}

TrackLaneComponent::~TrackLaneComponent()
{
    track->state.removeListener (this);
}

void TrackLaneComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0f0f23));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());

    // Only paint the dirty region: keeps grid painting O(visible) instead of
    // O(timeline width) as the canvas grows.
    const auto dirtyArea = g.getClipBounds().getIntersection (getLocalBounds());
    TimelineGrid::drawBarBackground (g, editViewState.edit, editViewState, dirtyArea);
    TimelineGrid::drawGridLines (g, editViewState.edit, editViewState, dirtyArea);

    if (dragCreateActive)
        paintRangeSelection (g, dragCreateAnchor, dragCreateCurrent);
    else if (rangeSelectionActive)
        paintRangeSelection (g, rangeSelectionStart, rangeSelectionEnd);
}

void TrackLaneComponent::paintRangeSelection (juce::Graphics& g, te::TimePosition start, te::TimePosition end) const
{
    auto rangeStart = start;
    auto rangeEnd = end;

    if (rangeEnd <= rangeStart)
    {
        const auto gridBeats = TimelineGrid::gridIntervalBeats (editViewState.edit, editViewState);
        const auto& ts = editViewState.edit.tempoSequence;
        const auto startBeat = ts.toBeats (rangeStart).inBeats();
        rangeEnd = ts.toTime (te::BeatPosition::fromBeats (startBeat + gridBeats));
    }

    int x1 = editViewState.timeToX (rangeStart);
    int x2 = editViewState.timeToX (rangeEnd);
    if (x2 < x1)
        std::swap (x1, x2);

    auto r = juce::Rectangle<int> (x1, 2, juce::jmax (2, x2 - x1), getHeight() - 4);
    g.setColour (juce::Colour (0xff4361ee).withAlpha (0.35f));
    g.fillRoundedRectangle (r.toFloat(), 4.0f);
    g.setColour (juce::Colour (0xff4361ee).withAlpha (0.9f));
    g.drawRoundedRectangle (r.toFloat(), 4.0f, 1.5f);
}

bool TrackLaneComponent::canDragCreateClips() const
{
    return EngineHelpers::getTrackKind (*track) == TrackKind::midi;
}

void TrackLaneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showLaneContextMenu (e);
        return;
    }

    if (e.mods.isLeftButtonDown() && canDragCreateClips())
    {
        clearRangeSelection();

        const auto time = editViewState.xToTime (e.x);
        dragCreateAnchor = TimelineGrid::snapTime (editViewState.edit, editViewState, time);
        dragCreateCurrent = dragCreateAnchor;
        dragCreateActive = true;
        repaint();
    }
}

void TrackLaneComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragCreateActive)
        return;

    const auto time = editViewState.xToTime (e.x);
    dragCreateCurrent = TimelineGrid::snapTime (editViewState.edit, editViewState, time);
    repaint();
}

void TrackLaneComponent::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);

    if (! dragCreateActive)
        return;

    dragCreateActive = false;

    rangeSelectionStart = juce::jmin (dragCreateAnchor, dragCreateCurrent);
    rangeSelectionEnd = juce::jmax (dragCreateAnchor, dragCreateCurrent);
    rangeSelectionActive = true;

    if (auto* timeline = findParentComponentOfClass<TimelineComponent>())
        timeline->clearRangeSelectionsExcept (this);

    repaint();
}

te::TimeRange TrackLaneComponent::getRangeSelection() const
{
    auto start = rangeSelectionStart;
    auto end = rangeSelectionEnd;

    if (end <= start)
    {
        const auto gridBeats = TimelineGrid::gridIntervalBeats (editViewState.edit, editViewState);
        const auto& ts = editViewState.edit.tempoSequence;
        const auto startBeat = ts.toBeats (start).inBeats();
        end = ts.toTime (te::BeatPosition::fromBeats (startBeat + gridBeats));
    }

    return { start, end };
}

void TrackLaneComponent::clearRangeSelection()
{
    if (! rangeSelectionActive)
        return;

    rangeSelectionActive = false;
    repaint();
}

void TrackLaneComponent::createMidiClipFromRangeSelection()
{
    if (! rangeSelectionActive || ! canDragCreateClips())
        return;

    if (auto clip = EngineHelpers::createMidiClipOnTrack (*track, getRangeSelection()))
        editViewState.selectionManager.selectOnly (clip.get());

    clearRangeSelection();
}

void TrackLaneComponent::showLaneContextMenu (const juce::MouseEvent& e)
{
    const auto insertTime = rangeSelectionActive ? rangeSelectionStart
                                                 : TimelineGrid::snapTime (editViewState.edit, editViewState,
                                                                           editViewState.xToTime (e.x));
    setInsertPoint (editViewState, track.get(), insertTime);

    showTimelineContextMenu (*this, e.getScreenPosition(), editViewState, track.get(),
                             canDragCreateClips() && rangeSelectionActive,
                             [this] { createMidiClipFromRangeSelection(); });
}

void TrackLaneComponent::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (auto* timeline = findParentComponentOfClass<TimelineComponent>())
        timeline->mouseWheelMove (e.getEventRelativeTo (timeline), wheel);
    else
        juce::Component::mouseWheelMove (e, wheel);
}

void TrackLaneComponent::resized()
{
    updateClipBounds();
}

void TrackLaneComponent::handleAsyncUpdate()
{
    if (compareAndReset (updateClips))
        buildClips();
    if (compareAndReset (updatePositions))
        updateClipBounds();
}

void TrackLaneComponent::buildClips()
{
    clips.clear();

    if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
    {
        for (auto* c : clipTrack->getClips())
        {
            te::Clip::Ptr clip (c);
            ClipComponent* cc = nullptr;

            if (dynamic_cast<te::WaveAudioClip*> (c))
                cc = new AudioClipComponent (editViewState, clip);
            else if (dynamic_cast<te::MidiClip*> (c))
                cc = new MidiClipComponent (editViewState, clip);
            else
                cc = new ClipComponent (editViewState, clip);

            cc->onDoubleClick = [this] (te::Clip& clipRef)
            {
                if (onClipDoubleClick)
                    onClipDoubleClick (clipRef);
            };

            clips.add (cc);
            addAndMakeVisible (cc);
        }
    }
    updateClipBounds();
}

void TrackLaneComponent::refreshLayout()
{
    updateClipBounds();
}

void TrackLaneComponent::updateClipBounds()
{
    const int height = getHeight();

    // Cull clip components that are outside the visible viewport (plus a margin
    // so small scrolls don't immediately require re-showing components).
    const int visibleStartX = editViewState.timeToX (editViewState.viewX1.get());
    const int visibleEndX = editViewState.timeToX (editViewState.viewX2.get());
    const int margin = juce::jmax (200, (visibleEndX - visibleStartX) / 2);

    for (auto* cc : clips)
    {
        const auto pos = cc->getClip().getPosition();
        const int x = editViewState.timeToX (pos.getStart());
        const int x2 = editViewState.timeToX (pos.getEnd());

        const bool visible = x2 >= visibleStartX - margin && x <= visibleEndX + margin;
        cc->setVisible (visible);

        if (visible)
            cc->setBounds (x, 2, juce::jmax (4, x2 - x), height - 4);
    }
}

PlayheadOverlay::PlayheadOverlay (te::Edit& e, EditViewState& evs)
    : edit (e), editViewState (evs)
{
    startTimerHz (30);
    setInterceptsMouseClicks (true, false);
}

void PlayheadOverlay::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::red);
    g.fillRect (xPosition, 0, 2, getHeight());
}

bool PlayheadOverlay::hitTest (int x, int y)
{
    juce::ignoreUnused (y);
    return std::abs (x - xPosition) <= 4;
}

void PlayheadOverlay::mouseDown (const juce::MouseEvent& e)
{
    mouseDrag (e);
}

void PlayheadOverlay::mouseDrag (const juce::MouseEvent& e)
{
    const auto time = editViewState.xToTime (e.x);
    edit.getTransport().setPosition (TimelineGrid::snapTime (edit, editViewState, time));
}

void PlayheadOverlay::timerCallback()
{
    if (getWidth() <= 0)
        return;

    const auto newX = editViewState.timeToX (edit.getTransport().getPosition());
    if (newX != xPosition)
    {
        xPosition = newX;
        repaint();
    }
}

} // namespace arrange
