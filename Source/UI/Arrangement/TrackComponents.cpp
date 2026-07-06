#include "TrackComponents.h"
#include "TimelineComponent.h"
#include "TimelineLOD.h"
#include "LaneClipSummaryPaint.h"
#include "Engine/EngineHelpers.h"
#include "Engine/UiTelemetryHub.h"
#include "UI/Routing/SidechainMenu.h"
#include "TracktionCommon.h"
#include "TimelineGrid.h"

namespace skeletonhive
{

namespace
{
constexpr int minClipLaneHeight = 24;

enum class TimelineMenuResult
{
    cut = 1,
    copy,
    paste,
    createMidiClip,
    newFolderTrack,
    fadeInLinear = 100,
    fadeInConvex,
    fadeInConcave,
    fadeInSCurve,
    fadeOutLinear = 110,
    fadeOutConvex,
    fadeOutConcave,
    fadeOutSCurve,
    moveTrackUp = 200,
    moveTrackDown,
    moveTrackOutOfFolder,
    freezeTrack = 300,
    flattenTrack,
    clipProperties = 400,
    showTakeLanes = 500,
    hideTakeLanes,
    newComp,
    flattenComp,
    unpackTakes,
    consolidate
};

void setFadeCurveOnAudioClips (EditViewState& editViewState, bool fadeIn, te::AudioFadeCurve::Type type)
{
    const auto clips = EngineHelpers::expandWithGroupedPeers (editViewState.selectionManager.getItemsOfType<te::Clip>());

    for (auto* clip : clips)
    {
        if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (clip))
        {
            if (fadeIn)
                audioClip->setFadeInType (type);
            else
                audioClip->setFadeOutType (type);

            audioClip->checkFadeLengthsForOverrun();
        }
    }
}

void addFadeCurveSubmenu (juce::PopupMenu& parent, const juce::String& name, int baseId)
{
    juce::PopupMenu sub;
    sub.addItem (baseId + 0, "Linear");
    sub.addItem (baseId + 1, "Convex");
    sub.addItem (baseId + 2, "Concave");
    sub.addItem (baseId + 3, "S-Curve");
    parent.addSubMenu (name, sub);
}

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
                              std::function<void()> onCreateMidiClip,
                              te::Clip* contextClip,
                              std::function<void()> onShowClipProperties,
                              std::function<void()> onTakeLanesChanged,
                              std::function<void()> onClipsChanged)
{
    juce::PopupMenu menu;
    const bool hasSelection = editViewState.selectionManager.getNumObjectsSelected() > 0;
    const bool canPaste = te::Clipboard::getInstance()->getContent() != nullptr;

    menu.addItem ((int) TimelineMenuResult::cut, "Cut", hasSelection);
    menu.addItem ((int) TimelineMenuResult::copy, "Copy", hasSelection);
    menu.addItem ((int) TimelineMenuResult::paste, "Paste", canPaste);

    bool canConsolidate = hasSelection;
    if (canConsolidate)
    {
        for (auto* clip : editViewState.selectionManager.getItemsOfType<te::Clip>())
        {
            if (EngineHelpers::hasMultipleTakes (*clip))
            {
                canConsolidate = false;
                break;
            }
        }
    }

    menu.addItem ((int) TimelineMenuResult::consolidate, "Consolidate", canConsolidate);

    te::Clip* takeClip = contextClip;
    if (takeClip == nullptr)
    {
        const auto selected = editViewState.selectionManager.getItemsOfType<te::Clip>();
        if (selected.size() == 1)
            takeClip = selected.getFirst();
    }

    bool hasAudioClip = takeClip != nullptr && dynamic_cast<te::AudioClipBase*> (takeClip) != nullptr;
    if (! hasAudioClip)
    {
        for (auto* clip : editViewState.selectionManager.getItemsOfType<te::Clip>())
        {
            if (dynamic_cast<te::AudioClipBase*> (clip) != nullptr)
            {
                hasAudioClip = true;
                break;
            }
        }
    }

    const bool hasTakeClip = takeClip != nullptr
                          && (dynamic_cast<te::WaveAudioClip*> (takeClip) != nullptr
                              || dynamic_cast<te::MidiClip*> (takeClip) != nullptr);

    if (hasTakeClip && EngineHelpers::hasMultipleTakes (*takeClip))
    {
        menu.addSeparator();
        const bool expanded = EngineHelpers::isTakeLanesExpanded (editViewState, *takeClip);
        menu.addItem ((int) TimelineMenuResult::showTakeLanes, expanded ? "Hide Take Lanes" : "Show Take Lanes");
        menu.addItem ((int) TimelineMenuResult::newComp, "New Comp");
        menu.addItem ((int) TimelineMenuResult::flattenComp, "Flatten Comp to Main Clip...");
        menu.addItem ((int) TimelineMenuResult::unpackTakes, "Unpack Takes to New Clips");
    }

    if (hasAudioClip)
    {
        menu.addSeparator();
        addFadeCurveSubmenu (menu, "Fade In Curve", (int) TimelineMenuResult::fadeInLinear);
        addFadeCurveSubmenu (menu, "Fade Out Curve", (int) TimelineMenuResult::fadeOutLinear);
        menu.addItem ((int) TimelineMenuResult::clipProperties, "Clip Properties...");
    }

    if (offerCreateMidiClip)
    {
        menu.addSeparator();
        menu.addItem ((int) TimelineMenuResult::createMidiClip, "Create MIDI Clip");
    }

    menu.addSeparator();
    menu.addItem ((int) TimelineMenuResult::newFolderTrack, "New Folder Track");

    if (track != nullptr && track->isMovable())
    {
        menu.addSeparator();
        menu.addItem ((int) TimelineMenuResult::moveTrackUp, "Move Track Up");
        menu.addItem ((int) TimelineMenuResult::moveTrackDown, "Move Track Down");

        if (track->getParentFolderTrack() != nullptr)
            menu.addItem ((int) TimelineMenuResult::moveTrackOutOfFolder, "Move Out of Folder");
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&target)
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [&editViewState, track, takeClip, onCreateMidiClip = std::move (onCreateMidiClip),
                         onShowClipProperties = std::move (onShowClipProperties),
                         onTakeLanesChanged = std::move (onTakeLanesChanged),
                         onClipsChanged = std::move (onClipsChanged)] (int result)
    {
        const int r = result;
        if (r >= (int) TimelineMenuResult::fadeInLinear && r <= (int) TimelineMenuResult::fadeInSCurve)
        {
            setFadeCurveOnAudioClips (editViewState, true,
                                    static_cast<te::AudioFadeCurve::Type> (r - (int) TimelineMenuResult::fadeInLinear));
            return;
        }

        if (r >= (int) TimelineMenuResult::fadeOutLinear && r <= (int) TimelineMenuResult::fadeOutSCurve)
        {
            setFadeCurveOnAudioClips (editViewState, false,
                                    static_cast<te::AudioFadeCurve::Type> (r - (int) TimelineMenuResult::fadeOutLinear));
            return;
        }

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
            case TimelineMenuResult::consolidate:
            {
                juce::String error;
                const auto created = EngineHelpers::consolidateClips (editViewState.edit,
                                                                      editViewState.selectionManager,
                                                                      &error);
                if (created.isEmpty() && error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Consolidate", error);
                else if (! created.isEmpty() && onClipsChanged)
                    onClipsChanged();
                break;
            }
            case TimelineMenuResult::createMidiClip:
                if (onCreateMidiClip)
                    onCreateMidiClip();
                break;
            case TimelineMenuResult::newFolderTrack:
                EngineHelpers::createFolderTrack (editViewState.edit, &editViewState.selectionManager);
                break;
            case TimelineMenuResult::moveTrackUp:
                if (track != nullptr)
                    EngineHelpers::moveTrackBySiblingDelta (*track, -1);
                break;
            case TimelineMenuResult::moveTrackDown:
                if (track != nullptr)
                    EngineHelpers::moveTrackBySiblingDelta (*track, 1);
                break;
            case TimelineMenuResult::moveTrackOutOfFolder:
                if (track != nullptr)
                    EngineHelpers::moveTrackOutOfFolder (*track);
                break;
            case TimelineMenuResult::clipProperties:
                if (onShowClipProperties)
                    onShowClipProperties();
                break;
            case TimelineMenuResult::showTakeLanes:
                if (takeClip != nullptr)
                {
                    EngineHelpers::toggleTakeLanesExpanded (editViewState, *takeClip);
                    if (onTakeLanesChanged)
                        onTakeLanesChanged();
                }
                break;
            case TimelineMenuResult::newComp:
                if (takeClip != nullptr)
                {
                    EngineHelpers::ensureCompTake (*takeClip);
                    if (onTakeLanesChanged)
                        onTakeLanesChanged();
                }
                break;
            case TimelineMenuResult::flattenComp:
                if (takeClip != nullptr)
                {
                    juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                                        "Flatten Comp",
                                                        "This permanently replaces all takes with the current comp.\n"
                                                        "This cannot be undone.",
                                                        "Flatten", "Cancel", nullptr,
                                                        juce::ModalCallbackFunction::create ([takeClip] (int button)
                    {
                        if (button == 1 && takeClip != nullptr)
                            EngineHelpers::flattenCompToMain (*takeClip, false);
                    }));
                }
                break;
            case TimelineMenuResult::unpackTakes:
                if (takeClip != nullptr)
                {
                    if (auto* wave = dynamic_cast<te::WaveAudioClip*> (takeClip))
                        wave->unpackTakes (false);
                    else if (auto* midi = dynamic_cast<te::MidiClip*> (takeClip))
                        midi->unpackTakes (false);
                }
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

    // Recording arm only makes sense for AudioTrack; folders/returns hide it.
    armButton.setVisible (dynamic_cast<te::AudioTrack*> (track.get()) != nullptr
                          && ! EngineHelpers::isReturnTrack (*track));
    armButton.setTooltip ("Arm for recording (right-click to choose inputs)");
    armButton.setToggleState (dynamic_cast<te::AudioTrack*> (track.get()) != nullptr
                                  && EngineHelpers::isTrackArmed (*dynamic_cast<te::AudioTrack*> (track.get())),
                              juce::dontSendNotification);

    armButton.onClick = [this]
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        {
            const bool arm = ! EngineHelpers::isTrackArmed (*audioTrack);
            EngineHelpers::armTrackWithDefaultInput (*audioTrack, arm);
            armButton.setToggleState (EngineHelpers::isTrackArmed (*audioTrack), juce::dontSendNotification);
            if (onArmChanged) onArmChanged (*track);
        }
    };

    armButton.addMouseListener (this, false);

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

    if (dropHighlightActive)
    {
        g.setColour (juce::Colour (0xff06d6a0).withAlpha (0.35f));

        switch (dropHighlightZone)
        {
            case EngineHelpers::TrackDropZone::above:
                g.fillRect (0, 0, getWidth(), getHeight() / 4);
                g.setColour (juce::Colours::white);
                g.drawHorizontalLine (0, 0.0f, (float) getWidth());
                break;
            case EngineHelpers::TrackDropZone::below:
                g.fillRect (0, getHeight() * 3 / 4, getWidth(), getHeight() / 4);
                g.setColour (juce::Colours::white);
                g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
                break;
            case EngineHelpers::TrackDropZone::intoFolder:
                g.fillRect (getWidth() / 4, getHeight() / 4, getWidth() / 2, getHeight() / 2);
                break;
            case EngineHelpers::TrackDropZone::promoteTopLevel:
                g.fillRect (0, 0, EngineHelpers::getTrackIndentLevel (*track) * 12 + 8, getHeight());
                break;
        }
    }

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());
        audioTrack != nullptr && audioTrack->isFrozen (te::Track::anyFreeze))
    {
        g.setColour (juce::Colour (0xff4cc9f0).withAlpha (0.12f));
        g.fillRect (getLocalBounds());
        g.setColour (juce::Colour (0xff4cc9f0));
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText ("FROZEN", getLocalBounds().removeFromBottom (12).withTrimmedLeft (4),
                    juce::Justification::centredLeft, false);
    }

    g.setColour (juce::Colours::white.withAlpha (0.2f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TrackHeaderComponent::resized()
{
    auto r = getLocalBounds().reduced (2);
    const int btnW = 22;
    soloButton.setBounds (r.removeFromRight (btnW));
    muteButton.setBounds (r.removeFromRight (btnW));
    if (armButton.isVisible())
        armButton.setBounds (r.removeFromRight (btnW));
    kindBadge.setBounds (r.removeFromLeft (42).reduced (0, 3));
    r.removeFromLeft (4);

    // Indent nested tracks under their FolderTrack parent(s).
    r.removeFromLeft (EngineHelpers::getTrackIndentLevel (*track) * 12);
    trackName.setBounds (r);
}

void TrackHeaderComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.eventComponent == &armButton)
    {
        if (e.mods.isPopupMenu())
            showInputSelectionMenu();
        return;
    }

    dragStarted = false;

    if (e.mods.isPopupMenu())
    {
        editViewState.selectionManager.selectOnly (track.get());
        showHeaderContextMenu (e.getScreenPosition());
        return;
    }

    if (e.mods.isLeftButtonDown())
    {
        editViewState.selectionManager.selectOnly (track.get());
        if (onTrackSelected)
            onTrackSelected (*track);
    }
}

void TrackHeaderComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || ! track->isMovable())
        return;

    if (e.getDistanceFromDragStart() < 6)
        return;

    dragStarted = true;

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
        container->startDragging (EngineHelpers::encodeTrackDrag (track->itemID), this);
}

EngineHelpers::TrackDropZone TrackHeaderComponent::dropZoneForPosition (juce::Point<int> localPos) const
{
    const int indentPx = EngineHelpers::getTrackIndentLevel (*track) * 12;

    if (localPos.x < indentPx + 8)
        return EngineHelpers::TrackDropZone::promoteTopLevel;

    const float ratio = (float) localPos.y / (float) juce::jmax (1, getHeight());

    if (track->isFolderTrack() && ratio > 0.25f && ratio < 0.75f)
        return EngineHelpers::TrackDropZone::intoFolder;

    if (ratio < 0.25f)
        return EngineHelpers::TrackDropZone::above;

    return EngineHelpers::TrackDropZone::below;
}

void TrackHeaderComponent::moveSelectedTracksToDropZone (EngineHelpers::TrackDropZone zone)
{
    auto selected = editViewState.selectionManager.getItemsOfType<te::Track>();

    if (selected.isEmpty())
        selected.add (track.get());

    for (auto* t : selected)
    {
        if (t == nullptr || ! t->isMovable())
            continue;

        if (! EngineHelpers::canReparentTrack (*t, *track, zone))
            continue;

        const auto point = EngineHelpers::insertPointForDrop (editViewState.edit, *track, zone);
        EngineHelpers::moveTrackToInsertPoint (editViewState.edit, *t, point);
    }
}

void TrackHeaderComponent::showInputSelectionMenu()
{
    auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());
    if (audioTrack == nullptr)
        return;

    auto& edit = editViewState.edit;
    juce::PopupMenu menu;
    juce::Array<te::InputDeviceInstance*> instances;

    for (auto* instance : edit.getAllInputDevices())
    {
        instances.add (instance);
        menu.addItem (instances.size(), instance->getInputDevice().getName(), true,
                      EngineHelpers::isInputAssignedToTrack (*instance, *audioTrack));
    }

    if (instances.isEmpty())
        menu.addItem (-1, "No input devices available", false);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&armButton),
                        [safeThis = juce::Component::SafePointer (this), instances] (int result)
    {
        if (result <= 0 || safeThis == nullptr)
            return;

        auto* at = dynamic_cast<te::AudioTrack*> (safeThis->track.get());
        auto* instance = instances[result - 1];
        if (at == nullptr || instance == nullptr)
            return;

        EngineHelpers::setInputAssignedToTrack (*instance, *at,
                                                ! EngineHelpers::isInputAssignedToTrack (*instance, *at));
        safeThis->armButton.setToggleState (EngineHelpers::isTrackArmed (*at), juce::dontSendNotification);
    });
}

void TrackHeaderComponent::showHeaderContextMenu (juce::Point<int> screenPosition)
{
    juce::PopupMenu menu;
    menu.addItem ((int) TimelineMenuResult::moveTrackUp, "Move Up", track->isMovable());
    menu.addItem ((int) TimelineMenuResult::moveTrackDown, "Move Down", track->isMovable());

    if (track->getParentFolderTrack() != nullptr)
        menu.addItem ((int) TimelineMenuResult::moveTrackOutOfFolder, "Move Out of Folder");

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());
        audioTrack != nullptr && ! EngineHelpers::isReturnTrack (*track))
    {
        menu.addSeparator();
        menu.addItem ((int) TimelineMenuResult::freezeTrack,
                      audioTrack->isFrozen (te::Track::individualFreeze) ? "Unfreeze Track" : "Freeze Track");
        menu.addItem ((int) TimelineMenuResult::flattenTrack, "Flatten to Audio Clip...");
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
                        [this] (int result)
    {
        switch (static_cast<TimelineMenuResult> (result))
        {
            case TimelineMenuResult::moveTrackUp:
                EngineHelpers::moveTrackBySiblingDelta (*track, -1);
                break;
            case TimelineMenuResult::moveTrackDown:
                EngineHelpers::moveTrackBySiblingDelta (*track, 1);
                break;
            case TimelineMenuResult::moveTrackOutOfFolder:
                EngineHelpers::moveTrackOutOfFolder (*track);
                break;
            case TimelineMenuResult::freezeTrack:
                if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
                {
                    // Kicks off an async background render; TE swaps playback
                    // to the freeze file and disables the frozen plugins.
                    audioTrack->setFrozen (! audioTrack->isFrozen (te::Track::individualFreeze),
                                           te::Track::individualFreeze);
                    repaint();
                }
                break;
            case TimelineMenuResult::flattenTrack:
                if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
                {
                    if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
                    {
                        const auto range = EngineHelpers::resolveProductionRange (editViewState.edit,
                                                                                  *clipTrack,
                                                                                  editViewState.selectionManager);
                        if (range.getLength() <= 0s)
                        {
                            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                                    "Flatten Track",
                                                                    "Select clips on this track, set a loop range, "
                                                                    "or extend the project before flattening.");
                            break;
                        }

                        auto runFlatten = [safeTrack = te::Track::Ptr (track), range]()
                        {
                            if (auto* at = dynamic_cast<te::AudioTrack*> (safeTrack.get()))
                            {
                                juce::String error;
                                if (EngineHelpers::flattenTrackToAudioClip (*at, range, true, &error) == nullptr
                                    && error.isNotEmpty())
                                {
                                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                                            "Flatten Track", error);
                                }
                            }
                        };

                        if (editViewState.selectionManager.getItemsOfType<te::Clip>().isEmpty()
                            && editViewState.edit.getTransport().getLoopRange().getLength() <= 0s)
                        {
                            juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                                "Flatten Track",
                                                                "No clips are selected and the loop range is empty.\n"
                                                                "Flatten the entire project length on this track?",
                                                                "Flatten", "Cancel", nullptr,
                                                                juce::ModalCallbackFunction::create ([runFlatten] (int button)
                            {
                                if (button == 1)
                                    runFlatten();
                            }));
                        }
                        else
                        {
                            runFlatten();
                        }
                    }
                }
                break;
            default:
                break;
        }
    });
}

bool TrackHeaderComponent::isInterestedInDragSource (const SourceDetails& details)
{
    const auto draggedId = EngineHelpers::parseTrackDrag (details.description);
    return draggedId.isValid() && draggedId != track->itemID;
}

void TrackHeaderComponent::itemDragEnter (const SourceDetails& details)
{
    dropHighlightActive = true;
    dropHighlightZone = dropZoneForPosition (details.localPosition);
    repaint();
}

void TrackHeaderComponent::itemDragMove (const SourceDetails& details)
{
    const auto zone = dropZoneForPosition (details.localPosition);
    if (zone != dropHighlightZone)
    {
        dropHighlightZone = zone;
        repaint();
    }
}

void TrackHeaderComponent::itemDragExit (const SourceDetails&)
{
    dropHighlightActive = false;
    repaint();
}

void TrackHeaderComponent::itemDropped (const SourceDetails& details)
{
    dropHighlightActive = false;
    const auto zone = dropZoneForPosition (details.localPosition);
    moveSelectedTracksToDropZone (zone);
    repaint();
}

void TrackHeaderComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::name)
        trackName.setText (track->getName(), juce::dontSendNotification);
    else if (id == te::IDs::frozen || id == te::IDs::frozenIndividually)
        repaint();
}

void TrackHeaderComponent::updateKindBadge()
{
    if (track->isFolderTrack())
    {
        kindBadge.setText ("FOLDER", juce::dontSendNotification);
        kindBadge.setColour (juce::Label::backgroundColourId, juce::Colour (0xff7209b7));
        return;
    }

    if (EngineHelpers::isReturnTrack (*track))
    {
        kindBadge.setText ("RETURN", juce::dontSendNotification);
        kindBadge.setColour (juce::Label::backgroundColourId, juce::Colour (0xffe63946));
        return;
    }

    const bool isMidi = EngineHelpers::isMidiTrack (*track);
    kindBadge.setText (isMidi ? "MIDI" : "AUDIO", juce::dontSendNotification);
    kindBadge.setColour (juce::Label::backgroundColourId,
                        isMidi ? juce::Colour (0xff4361ee) : juce::Colour (0xff2d6a4f));
}

PluginSlotButton::PluginSlotButton (EditViewState& evs, te::Plugin::Ptr p)
    : TextButton (p->getName()), editViewState (evs), plugin (std::move (p))
{
    if (auto* rack = dynamic_cast<te::RackInstance*> (plugin.get()))
        setButtonText (rack->getName());

    setTooltip (plugin->getName() + " (right-click for options, drag to reorder)");
    updateEnabledLook();
    refreshLoadState();

    onClick = [this]
    {
        if (auto* rack = dynamic_cast<te::RackInstance*> (plugin.get()))
        {
            if (plugin->windowState != nullptr)
                plugin->windowState->showWindowExplicitly();
        }
        else if (plugin->windowState != nullptr)
        {
            plugin->windowState->showWindowExplicitly();
        }

        editViewState.selectionManager.selectOnly (plugin.get());
    };
}

PluginSlotButton::~PluginSlotButton()
{
    stopTimer();
}

void PluginSlotButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    if (loadState == EngineHelpers::PluginLoadState::failed)
    {
        g.setColour (juce::Colour (0xff9b2226).withAlpha (0.55f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    }
    else if (loadState == EngineHelpers::PluginLoadState::loading)
    {
        g.setColour (juce::Colour (0xffca6702).withAlpha (0.45f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    }

    if (EngineHelpers::isPluginSoloed (*te::getTrackContainingPlugin (plugin->edit, plugin.get()), *plugin))
    {
        g.setColour (juce::Colours::gold.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    }

    if (plugin->canSidechain() && plugin->getSidechainSourceID().isValid())
    {
        g.setColour (juce::Colours::cyan);
        g.fillEllipse (4.0f, 4.0f, 6.0f, 6.0f);
    }

    juce::TextButton::paintButton (g, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
}

void PluginSlotButton::updateEnabledLook()
{
    setAlpha (plugin->isEnabled() ? 1.0f : 0.4f);
}

void PluginSlotButton::refreshLoadState()
{
    const auto previous = loadState;
    loadState = EngineHelpers::getExternalPluginLoadState (*plugin, loadStatusMessage);

    if (loadState == EngineHelpers::PluginLoadState::loading)
    {
        if (! isTimerRunning())
            startTimerHz (10);
    }
    else
    {
        stopTimer();
    }

    if (loadState == EngineHelpers::PluginLoadState::failed)
        setTooltip (plugin->getName() + " — " + loadStatusMessage);
    else if (loadState == EngineHelpers::PluginLoadState::loading)
        setTooltip (plugin->getName() + " — Loading...");
    else
        setTooltip (plugin->getName() + " (right-click for options, drag to reorder)");

    if (previous != loadState)
        repaint();
}

void PluginSlotButton::timerCallback()
{
    refreshLoadState();
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

void PluginSlotButton::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
    {
        juce::var desc;
        desc = juce::String (PluginDragTypes::slotReorder) + ":" + plugin->itemID.toString();
        container->startDragging (desc, this, juce::ScaledImage(), true, nullptr, &e.source);
    }
}

void PluginSlotButton::showWetDryDialog()
{
    if (! EngineHelpers::hasWetDryMix (*plugin))
        return;

    auto* dry = EngineHelpers::getDryParam (*plugin);
    auto* wet = EngineHelpers::getWetParam (*plugin);
    if (dry == nullptr || wet == nullptr)
        return;

    struct WetDryPanel : public juce::Component
    {
        WetDryPanel (te::AutomatableParameter& dryP, te::AutomatableParameter& wetP)
            : dryParam (dryP), wetParam (wetP)
        {
            dryLabel.setText ("Dry %", juce::dontSendNotification);
            wetLabel.setText ("Wet %", juce::dontSendNotification);
            drySlider.setRange (0.0, 100.0, 1.0);
            wetSlider.setRange (0.0, 100.0, 1.0);
            drySlider.setValue (dryParam.getCurrentValue() * 100.0, juce::dontSendNotification);
            wetSlider.setValue (wetParam.getCurrentValue() * 100.0, juce::dontSendNotification);

            drySlider.onValueChange = [this]
            {
                dryParam.setParameter ((float) (drySlider.getValue() / 100.0), juce::sendNotification);
            };
            wetSlider.onValueChange = [this]
            {
                wetParam.setParameter ((float) (wetSlider.getValue() / 100.0), juce::sendNotification);
            };

            addAndMakeVisible (dryLabel);
            addAndMakeVisible (wetLabel);
            addAndMakeVisible (drySlider);
            addAndMakeVisible (wetSlider);
            setSize (300, 80);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (8);
            auto row1 = r.removeFromTop (28);
            dryLabel.setBounds (row1.removeFromLeft (48));
            drySlider.setBounds (row1);
            auto row2 = r.removeFromTop (28);
            wetLabel.setBounds (row2.removeFromLeft (48));
            wetSlider.setBounds (row2);
        }

        juce::Label dryLabel, wetLabel;
        juce::Slider drySlider, wetSlider;
        te::AutomatableParameter& dryParam, &wetParam;
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Wet/Dry Mix — " + plugin->getName();
    opts.content.setOwned (new WetDryPanel (*dry, *wet));
    opts.componentToCentreAround = this;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.launchAsync();
}

void PluginSlotButton::showSlotMenu()
{
    enum MenuIds
    {
        bypass = 1, moveLeft, moveRight, remove,
        wetDry = 100, soloDevice = 110, showMacros = 120,
        moveToRackBase = 300
    };

    juce::PopupMenu menu;
    auto* trackPtr = te::getTrackContainingPlugin (plugin->edit, plugin.get());
    menu.addItem (bypass, "Bypass", true, ! plugin->isEnabled());

    const bool soloed = trackPtr != nullptr && EngineHelpers::isPluginSoloed (*trackPtr, *plugin);
    menu.addItem (soloDevice, soloed ? "Unsolo Device" : "Solo Device", true, soloed);

    if (EngineHelpers::hasWetDryMix (*plugin))
        menu.addItem (wetDry, "Wet/Dry Mix...");

    if (auto* rack = dynamic_cast<te::RackInstance*> (plugin.get()))
    {
        menu.addItem (showMacros, "Show Macro Knobs");
        menu.addItem (121, "Add Macro Knob");
        juce::ignoreUnused (rack);
    }

    if (plugin->canSidechain())
        SidechainMenu::addSidechainMenuItems (menu, *plugin);

    if (dynamic_cast<te::RackInstance*> (plugin.get()) == nullptr)
    {
        juce::PopupMenu rackMenu;
        int rackIdx = 0;
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (te::getTrackContainingPlugin (plugin->edit, plugin.get())))
        {
            for (auto* pl : audioTrack->pluginList)
                if (auto* rack = dynamic_cast<te::RackInstance*> (pl))
                {
                    rackMenu.addItem (moveToRackBase + rackIdx, "Move into " + rack->getName(), true, false);
                    ++rackIdx;
                }
        }

        if (rackIdx > 0)
            menu.addSubMenu ("Move into Rack", rackMenu);
    }

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
        auto* trackPtr = te::getTrackContainingPlugin (p.edit, &p);
        if (trackPtr == nullptr)
            return;
        auto& track = *trackPtr;

        if (SidechainMenu::handleSidechainMenuResult (result, p, moveToRackBase,
                                                      [safeThis]
        {
            if (safeThis != nullptr)
                safeThis->repaint();
        }))
            return;

        if (result >= moveToRackBase)
        {
            if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (te::getTrackContainingPlugin (p.edit, &p)))
            {
                int rackIdx = 0;
                for (auto* pl : audioTrack->pluginList)
                {
                    if (auto* rack = dynamic_cast<te::RackInstance*> (pl))
                    {
                        if (result == moveToRackBase + rackIdx)
                        {
                            if (rack->type->addPlugin (p, {}, true))
                                p.deleteFromParent();
                            break;
                        }
                        ++rackIdx;
                    }
                }
            }
            return;
        }

        switch (result)
        {
            case bypass:
                p.setEnabled (! p.isEnabled());
                safeThis->updateEnabledLook();
                break;
            case wetDry:
                safeThis->showWetDryDialog();
                break;
            case soloDevice:
                if (EngineHelpers::isPluginSoloed (track, p))
                    EngineHelpers::clearSoloedPlugin (track);
                else
                    EngineHelpers::setSoloedPlugin (track, &p);
                safeThis->repaint();
                break;
            case showMacros:
                if (auto* footer = safeThis->findParentComponentOfClass<TrackFooterComponent>())
                    footer->setExpandedRack (dynamic_cast<te::RackInstance*> (&p));
                break;
            case 121:
                if (auto* rack = dynamic_cast<te::RackInstance*> (&p))
                    rack->type->getMacroParameterListForWriting().createMacroParameter();
                break;
            case moveLeft:
                if (safeThis->onMove) safeThis->onMove (p, -1);
                break;
            case moveRight:
                if (safeThis->onMove) safeThis->onMove (p, 1);
                break;
            case remove:
                if (EngineHelpers::isPluginSoloed (track, p))
                    EngineHelpers::clearSoloedPlugin (track);
                if (safeThis->onRemove) safeThis->onRemove (p);
                break;
            default:
                break;
        }
    });
}

//==============================================================================
// RackMacroPanel

RackMacroPanel::RackMacroPanel (EditViewState& evs, te::RackInstance& rack)
    : editViewState (evs), rack (&rack)
{
    for (auto macro : rack.type->getMacroParameters())
    {
        auto* label = macroLabels.add (new juce::Label ({}, macro->getParameterName()));
        label->setJustificationType (juce::Justification::centredRight);

        auto* slider = macroSliders.add (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
        slider->setRange (0.0, 1.0, 0.001);
        slider->setValue (macro->getCurrentValue(), juce::dontSendNotification);
        slider->onValueChange = [macro, slider]
        {
            macro->setParameter ((float) slider->getValue(), juce::sendNotification);
        };

        addAndMakeVisible (label);
        addAndMakeVisible (slider);
    }

    setSize (320, juce::jmax (24, macroSliders.size() * 24 + 8));
}

void RackMacroPanel::resized()
{
    auto r = getLocalBounds().reduced (2);
    for (int i = 0; i < macroSliders.size(); ++i)
    {
        auto row = r.removeFromTop (22);
        macroLabels[i]->setBounds (row.removeFromLeft (72));
        macroSliders[i]->setBounds (row);
    }
}

//==============================================================================
// TrackFooterComponent

void TrackFooterComponent::setExpandedRack (te::RackInstance* rack)
{
    if (rack == nullptr)
        return;

    auto panel = std::make_unique<RackMacroPanel> (editViewState, *rack);
    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                              localAreaToGlobal (getBounds()), nullptr);
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

void TrackFooterComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showFooterContextMenu (e);
}

void TrackFooterComponent::showFooterContextMenu (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    enum { insertRack = 1, groupRack = 2 };

    juce::PopupMenu menu;
    menu.addItem (insertRack, "Insert Empty Rack");

    const auto selected = editViewState.selectionManager.getItemsOfType<te::Plugin>();
    if (selected.size() >= 2)
        menu.addItem (groupRack, "Group Selected into Rack");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        {
            if (result == insertRack)
            {
                if (auto* rack = EngineHelpers::insertEmptyRack (*audioTrack))
                    setExpandedRack (rack);
            }
            else if (result == groupRack)
            {
                groupSelectedIntoRack();
            }
        }
    });
}

void TrackFooterComponent::groupSelectedIntoRack()
{
    if (auto* rack = EngineHelpers::wrapPluginsInRack (editViewState.selectionManager))
        setExpandedRack (rack);
}

int TrackFooterComponent::slotIndexAtX (int x) const
{
    const int slotWidth = 80;
    const int origin = addButton.getRight() + 1;
    if (x < origin)
        return 0;

    return juce::jlimit (0, plugins.size(), (x - origin) / slotWidth);
}

bool TrackFooterComponent::isInterestedInDragSource (const SourceDetails& details)
{
    const auto desc = details.description.toString();
    return desc.startsWith (PluginDragTypes::slotReorder)
        || desc.startsWith (PluginDragTypes::browserInsert);
}

void TrackFooterComponent::itemDragEnter (const SourceDetails&)
{
    dropHighlightSlot = -1;
    repaint();
}

void TrackFooterComponent::itemDragMove (const SourceDetails& details)
{
    const int slot = slotIndexAtX (details.localPosition.x);
    if (slot != dropHighlightSlot)
    {
        dropHighlightSlot = slot;
        repaint();
    }
}

void TrackFooterComponent::itemDragExit (const SourceDetails&)
{
    dropHighlightSlot = -1;
    repaint();
}

void TrackFooterComponent::itemDropped (const SourceDetails& details)
{
    dropHighlightSlot = -1;
    repaint();

    const auto desc = details.description.toString();
    const int slot = slotIndexAtX (details.localPosition.x);

    if (desc.startsWith (PluginDragTypes::slotReorder))
    {
        const auto idStr = desc.fromFirstOccurrenceOf (":", false, false);
        const auto pluginId = te::EditItemID::fromVar (idStr);

        for (auto* slotBtn : plugins)
            if (slotBtn->getPlugin()->itemID == pluginId)
                movePluginToSlot (*slotBtn->getPlugin(), slot);
    }
    else if (desc.startsWith (PluginDragTypes::browserInsert))
    {
        const auto idStr = desc.fromFirstOccurrenceOf (":", false, false);
        insertBrowserPlugin (EngineHelpers::lookupKnownPlugin (editViewState.edit.engine, idStr), slot);
    }
}

void TrackFooterComponent::insertBrowserPlugin (const juce::PluginDescription& desc, int slotIndex)
{
    if (createPlugin == nullptr || desc.name.isEmpty())
        return;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        if (auto plugin = createPlugin (desc))
        {
            const int baseIndex = EngineHelpers::getUserChainInsertIndex (*audioTrack);
            const int insertIndex = baseIndex + juce::jlimit (0, plugins.size(), slotIndex);
            EngineHelpers::insertPluginOnTrack (*audioTrack, plugin, insertIndex);
        }
        else
        {
            EngineHelpers::showPluginInsertFailureAlert (this, desc);
        }
    }
}

TrackFooterComponent::~TrackFooterComponent()
{
    track->pluginList.state.removeListener (this);
}

void TrackFooterComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16213e));

    if (dropHighlightSlot >= 0)
    {
        const int x = addButton.getRight() + dropHighlightSlot * 80;
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.fillRect (x, 2, 4, getHeight() - 4);
    }
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
        if (! EngineHelpers::isFooterVisiblePlugin (*plugin))
            continue;

        auto* slot = new PluginSlotButton (editViewState, plugin);
        slot->onRemove = [this] (te::Plugin& p) { removePlugin (p); };
        slot->onMove = [this] (te::Plugin& p, int direction) { movePlugin (p, direction); };
        slot->onDropAtSlot = [this] (te::Plugin& p, int target) { movePluginToSlot (p, target); };

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
    int slotIndex = -1;
    for (int i = 0; i < plugins.size(); ++i)
        if (plugins[i]->getPlugin().get() == &plugin)
            slotIndex = i;

    movePluginToSlot (plugin, slotIndex + direction);
}

void TrackFooterComponent::movePluginToSlot (te::Plugin& plugin, int targetSlotIndex)
{
    if (targetSlotIndex < 0 || targetSlotIndex >= plugins.size())
        return;

    int slotIndex = -1;
    for (int i = 0; i < plugins.size(); ++i)
        if (plugins[i]->getPlugin().get() == &plugin)
            slotIndex = i;

    if (slotIndex < 0 || slotIndex == targetSlotIndex)
        return;

    auto& listState = track->pluginList.state;
    const int fromIndex = listState.indexOf (plugin.state);
    const int toIndex = listState.indexOf (plugins[targetSlotIndex]->getPlugin()->state);

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
    // Folder tracks are pure organisation: no clips, so no grid/lane content,
    // just a distinct divider band matching the header's FOLDER badge colour.
    if (track->isFolderTrack())
    {
        g.fillAll (juce::Colour (0xff2a1a3e));
        g.setColour (juce::Colour (0xff7209b7).withAlpha (0.5f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        return;
    }

    g.fillAll (juce::Colour (0xff0f0f23));
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    editViewState.laneBackgroundCache.renderOrFetch (g, editViewState.edit, editViewState,
                                                     track->itemID, getLocalBounds());

    if (isLaneLevelRendering())
    {
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
            paintLaneClipSummaries (g, editViewState, *clipTrack, getLocalBounds());
    }

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
    return EngineHelpers::canHostMidiClips (*track);
}

bool TrackLaneComponent::isLaneLevelRendering() const
{
    return useLaneLevelRendering (editViewState.getPixelsPerBeat());
}

te::Clip* TrackLaneComponent::findClipAtX (int x) const
{
    if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
        return skeletonhive::findClipAtX (editViewState, *clipTrack, x);

    return nullptr;
}

void TrackLaneComponent::placePlayheadAtX (int x)
{
    const auto time = TimelineGrid::snapTime (editViewState.edit, editViewState,
                                              editViewState.xToTime (x));
    editViewState.edit.getTransport().setPosition (juce::jmax (te::TimePosition(), time));
}

void TrackLaneComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        showLaneContextMenu (e);
        return;
    }

    if (isLaneLevelRendering() && e.mods.isLeftButtonDown())
    {
        if (auto* clip = findClipAtX (e.x))
        {
            editViewState.selectionManager.selectOnly (clip);
            repaint();
            return;
        }

        editViewState.selectionManager.deselectAll();
        repaint();
    }

    if (! e.mods.isLeftButtonDown())
        return;

    pendingTimelineInteraction = true;
    pendingDragStartPos = e.getPosition();
    dragCreateActive = false;
    clearRangeSelection();

    const auto time = editViewState.xToTime (e.x);
    dragCreateAnchor = TimelineGrid::snapTime (editViewState.edit, editViewState, time);
    dragCreateCurrent = dragCreateAnchor;
}

void TrackLaneComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! isLaneLevelRendering())
        return;

    if (auto* clip = findClipAtX (e.x))
    {
        if (onClipDoubleClick)
            onClipDoubleClick (*clip);
    }
}

void TrackLaneComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! pendingTimelineInteraction)
        return;

    if (! dragCreateActive
        && e.getDistanceFromDragStart() >= timelineClickDragThresholdPx
        && canDragCreateClips())
    {
        dragCreateActive = true;
    }

    if (! dragCreateActive)
        return;

    const auto time = editViewState.xToTime (e.x);
    dragCreateCurrent = TimelineGrid::snapTime (editViewState.edit, editViewState, time);
    repaint();
}

void TrackLaneComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! pendingTimelineInteraction)
        return;

    pendingTimelineInteraction = false;

    if (dragCreateActive)
    {
        dragCreateActive = false;

        rangeSelectionStart = juce::jmin (dragCreateAnchor, dragCreateCurrent);
        rangeSelectionEnd = juce::jmax (dragCreateAnchor, dragCreateCurrent);
        rangeSelectionActive = true;

        if (auto* timeline = findParentComponentOfClass<TimelineComponent>())
            timeline->clearRangeSelectionsExcept (this);
    }
    else
    {
        placePlayheadAtX (e.x);
    }

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
                             [this] { createMidiClipFromRangeSelection(); },
                             nullptr, nullptr, onTakeLanesChanged,
                             [this]
                             {
                                 if (onTakeLanesChanged)
                                     onTakeLanesChanged();
                                 if (onClipSelectionChanged)
                                     onClipSelectionChanged();
                             });
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

int TrackLaneComponent::getClipAreaHeight() const
{
    const int extra = EngineHelpers::getTakeLaneExtraHeight (editViewState, *track);
    return juce::jmax (minClipLaneHeight, getHeight() - extra);
}

void TrackLaneComponent::updateTakeLaneStack()
{
    const auto expandedId = editViewState.expandedTakeClipId.get();
    te::Clip* expandedClip = nullptr;

    if (expandedId != 0)
    {
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
        {
            for (auto* c : clipTrack->getClips())
            {
                if ((juce::int64) c->itemID.getRawID() == expandedId && EngineHelpers::hasMultipleTakes (*c))
                {
                    expandedClip = c;
                    break;
                }
            }
        }
    }

    if (expandedClip == nullptr)
    {
        if (takeLaneStack != nullptr)
        {
            takeLaneStack->releaseResources();
            removeChildComponent (takeLaneStack.get());
            takeLaneStack.reset();
        }
        return;
    }

    if (takeLaneStack == nullptr || takeLaneStack->getClip() != expandedClip)
    {
        if (takeLaneStack != nullptr)
            takeLaneStack->releaseResources();

        takeLaneStack = std::make_unique<TakeLaneStack> (editViewState, *expandedClip);
        takeLaneStack->onLayoutChanged = [this]
        {
            updateClipBounds();
            if (onTakeLanesChanged)
                onTakeLanesChanged();
        };
        addAndMakeVisible (*takeLaneStack);
    }

    const int clipAreaHeight = getClipAreaHeight();
    const auto pos = expandedClip->getPosition();
    const int x = editViewState.timeToX (pos.getStart());
    const int w = juce::jmax (4, editViewState.timeToX (pos.getEnd()) - x);
    const int stackH = EngineHelpers::getTakeLaneExtraHeight (editViewState, *track);

    takeLaneStack->setBounds (x, clipAreaHeight, w, stackH);
    takeLaneStack->refreshLayout();
    takeLaneStack->toFront (false);
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
            cc->onSelectionChanged = [this]
            {
                if (onClipSelectionChanged)
                    onClipSelectionChanged();
            };
            cc->onShowClipProperties = [this]
            {
                if (onShowClipProperties)
                    onShowClipProperties();
            };
            cc->onTakeLanesChanged = onTakeLanesChanged;
            cc->onCrossTrackDragMove = [this] (te::Clip& c, const juce::MouseEvent& ev)
            {
                if (onClipCrossTrackDragMove)
                    onClipCrossTrackDragMove (c, ev);
            };
            cc->onCrossTrackDragEnd = [this] (te::Clip& c, const juce::MouseEvent& ev)
            {
                if (onClipCrossTrackDragEnd)
                    onClipCrossTrackDragEnd (c, ev);
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
    updateTakeLaneStack();

    const int clipAreaHeight = getClipAreaHeight();

    if (isLaneLevelRendering())
    {
        for (auto* cc : clips)
            cc->setVisible (false);

        return;
    }

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

        const int clipWidth = juce::jmax (4, x2 - x);
        const auto detail = getClipDetailLevel (editViewState.getPixelsPerBeat(), clipWidth);

        if (auto* audioClip = dynamic_cast<AudioClipComponent*> (cc))
        {
            if (visible && shouldHoldWaveformThumbnail (detail, editViewState.drawWaveforms.get()))
                audioClip->ensureThumbnail();
            else
                audioClip->releaseThumbnail();
        }

        if (auto* midiClip = dynamic_cast<MidiClipComponent*> (cc))
        {
            if (! visible || ! shouldShowMidiPreview (detail))
                midiClip->releasePreview();
        }

        if (visible)
        {
            cc->setBounds (x, 2, clipWidth, clipAreaHeight - 4);
            cc->repaint();
        }
    }
}

bool TrackLaneComponent::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith (PluginDragTypes::browserInsert);
}

void TrackLaneComponent::itemDropped (const SourceDetails& details)
{
    const auto descStr = details.description.toString();
    if (! descStr.startsWith (PluginDragTypes::browserInsert) || createPlugin == nullptr)
        return;

    const auto idStr = descStr.fromFirstOccurrenceOf (":", false, false);
    const auto pd = EngineHelpers::lookupKnownPlugin (editViewState.edit.engine, idStr);

    if (pd.name.isEmpty())
        return;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        if (auto plugin = createPlugin (pd))
            EngineHelpers::insertPluginOnTrack (*audioTrack, plugin);
        else
            EngineHelpers::showPluginInsertFailureAlert (this, pd);
    }
}

PlayheadOverlay::PlayheadOverlay (te::Edit& e, EditViewState& evs, UiTelemetryHub* hub)
    : edit (e), editViewState (evs), telemetryHub (hub)
{
    setInterceptsMouseClicks (false, false);

    if (telemetryHub != nullptr)
        telemetryHub->registerPlayhead (this);
}

PlayheadOverlay::~PlayheadOverlay()
{
    if (telemetryHub != nullptr)
        telemetryHub->unregisterPlayhead (this);
}

void PlayheadOverlay::paint (juce::Graphics& g)
{
    g.setColour (juce::Colours::red);
    g.fillRect (xPosition, 0, 2, getHeight());
}

void PlayheadOverlay::updateFromTransport()
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

} // namespace skeletonhive
