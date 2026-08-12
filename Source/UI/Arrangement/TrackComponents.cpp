#include "TrackComponents.h"
#include "TimelineComponent.h"
#include "TimelineLOD.h"
#include "LaneClipSummaryPaint.h"
#include "ArrangementSelectionHelpers.h"
#include "Engine/AudioToMidiTypes.h"
#include "Engine/EngineHelpers.h"
#include "Engine/AraHelpers.h"
#include "Engine/WarpEngine.h"
#include "Engine/TrackInputRouting.h"
#include "Engine/TrackPluginChainModel.h"
#include "Engine/UiTelemetryHub.h"
#include "UI/AppLookAndFeel.h"
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
    loopSelection,
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
    editWarpMarkers = 405,
    openAraEditor = 406,
    convertAraToMidi = 407,
    convertToMidiMelody = 420,
    convertToMidiHarmony,
    convertToMidiDrums,
    showTakeLanes = 500,
    hideTakeLanes,
    newComp,
    flattenComp,
    unpackTakes,
    consolidate,
    exportToLibrary = 410,
    applyGrooveBase = 600
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
                              std::function<void (te::Clip*)> onEditWarpMarkers,
                              std::function<void (te::Clip*, AudioToMidiMode)> onAudioToMidi,
                              std::function<void()> onTakeLanesChanged,
                              std::function<void()> onClipsChanged,
                              std::function<void (te::Clip&)> onExportToLibrary,
                              GroovePoolManager* groovePool,
                              bool offerLoopSelection,
                              std::function<void()> onLoopSelection)
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

    bool hasMidiClip = false;
    for (auto* clip : editViewState.selectionManager.getItemsOfType<te::Clip>())
    {
        if (dynamic_cast<te::MidiClip*> (clip) != nullptr)
        {
            hasMidiClip = true;
            break;
        }
    }

    if (groovePool != nullptr && hasMidiClip)
    {
        juce::PopupMenu grooveMenu;
        const auto templates = groovePool->getAllTemplates();

        for (int i = 0; i < templates.size(); ++i)
            grooveMenu.addItem ((int) TimelineMenuResult::applyGrooveBase + i, templates.getReference (i).name);

        menu.addSubMenu ("Apply Groove", grooveMenu, true);
    }

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

    if (hasTakeClip)
    {
        menu.addSeparator();
        menu.addItem ((int) TimelineMenuResult::exportToLibrary, "Export to Library...");
    }

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

        if (takeClip != nullptr)
        {
            if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (takeClip))
            {
                if (WarpEngine::supportsWarp (*audioClip))
                    menu.addItem ((int) TimelineMenuResult::editWarpMarkers, "Edit Warp Markers...");

                if (AraHelpers::isBuildEnabled())
                {
                    menu.addItem ((int) TimelineMenuResult::openAraEditor, "Open ARA Editor...");
                    if (AraHelpers::isUsingAra (*audioClip))
                        menu.addItem ((int) TimelineMenuResult::convertAraToMidi, "Convert ARA Analysis to MIDI");
                }

                juce::PopupMenu convertMenu;
                convertMenu.addItem ((int) TimelineMenuResult::convertToMidiMelody, "Melody");
                convertMenu.addItem ((int) TimelineMenuResult::convertToMidiHarmony, "Harmony");
                convertMenu.addItem ((int) TimelineMenuResult::convertToMidiDrums, "Drums");
                menu.addSubMenu ("Convert to MIDI Track", convertMenu);
            }
        }
    }

    if (offerLoopSelection || offerCreateMidiClip)
    {
        menu.addSeparator();
        if (offerLoopSelection)
            menu.addItem ((int) TimelineMenuResult::loopSelection, "Loop Selection");
        if (offerCreateMidiClip)
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
                        [&editViewState, track, takeClip, groovePool,
                         onCreateMidiClip = std::move (onCreateMidiClip),
                         onLoopSelection = std::move (onLoopSelection),
                         onShowClipProperties = std::move (onShowClipProperties),
                         onEditWarpMarkers = std::move (onEditWarpMarkers),
                         onAudioToMidi = std::move (onAudioToMidi),
                         onTakeLanesChanged = std::move (onTakeLanesChanged),
                         onClipsChanged = std::move (onClipsChanged),
                         onExportToLibrary = std::move (onExportToLibrary)] (int result)
    {
        const int r = result;

        if (groovePool != nullptr && r >= (int) TimelineMenuResult::applyGrooveBase)
        {
            const int grooveIdx = r - (int) TimelineMenuResult::applyGrooveBase;
            const auto templates = groovePool->getAllTemplates();

            if (juce::isPositiveAndBelow (grooveIdx, templates.size()))
            {
                const auto& groove = templates.getReference (grooveIdx);
                groovePool->setSelectedGrooveId (groove.id);

                juce::String error;
                EngineHelpers::applyGrooveToSelection (editViewState.edit, editViewState.selectionManager, groove, &error);

                if (error.isNotEmpty())
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Apply Groove", error);
            }

            return;
        }

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
            case TimelineMenuResult::loopSelection:
                if (onLoopSelection)
                    onLoopSelection();
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
            case TimelineMenuResult::editWarpMarkers:
                if (takeClip != nullptr && onEditWarpMarkers)
                    onEditWarpMarkers (takeClip);
                break;
            case TimelineMenuResult::openAraEditor:
                if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (takeClip))
                {
                    if (! AraHelpers::isUsingAra (*audioClip))
                        audioClip->setTimeStretchMode (te::TimeStretcher::Mode::ara);

                    AraHelpers::showAraWindow (*audioClip);
                }
                break;
            case TimelineMenuResult::convertAraToMidi:
                if (auto* audioClip = dynamic_cast<te::AudioClipBase*> (takeClip))
                    AraHelpers::convertAraToMidi (*audioClip);
                break;
            case TimelineMenuResult::convertToMidiMelody:
                if (takeClip != nullptr && onAudioToMidi)
                    onAudioToMidi (takeClip, AudioToMidiMode::melody);
                break;
            case TimelineMenuResult::convertToMidiHarmony:
                if (takeClip != nullptr && onAudioToMidi)
                    onAudioToMidi (takeClip, AudioToMidiMode::harmony);
                break;
            case TimelineMenuResult::convertToMidiDrums:
                if (takeClip != nullptr && onAudioToMidi)
                    onAudioToMidi (takeClip, AudioToMidiMode::drums);
                break;
            case TimelineMenuResult::exportToLibrary:
                if (takeClip != nullptr && onExportToLibrary)
                    onExportToLibrary (*takeClip);
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
    trackName.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    trackName.setInterceptsMouseClicks (false, false);

    kindBadge.setJustificationType (juce::Justification::centred);
    kindBadge.setFont (juce::FontOptions (8.0f, juce::Font::bold));
    kindBadge.setColour (juce::Label::textColourId, juce::Colours::white);
    kindBadge.setOpaque (true);
    kindBadge.setInterceptsMouseClicks (false, false);
    updateKindBadge();

    inputLabel.setText ("Input:", juce::dontSendNotification);
    inputLabel.setJustificationType (juce::Justification::centredLeft);
    inputLabel.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    inputLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    inputLabel.setInterceptsMouseClicks (false, false);

    outputLabel.setText ("Output:", juce::dontSendNotification);
    outputLabel.setJustificationType (juce::Justification::centredLeft);
    outputLabel.setFont (juce::FontOptions (9.0f, juce::Font::bold));
    outputLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
    outputLabel.setInterceptsMouseClicks (false, false);

    armButton.setVisible (dynamic_cast<te::AudioTrack*> (track.get()) != nullptr
                          && ! track->isMasterTrack()
                          && ! EngineHelpers::isReturnTrack (*track)
                          && TrackMonitorRouting::shouldShowMonitorControl (*track));
    armButton.setTooltip ("Arm for recording");
    monitorButton.setVisible (armButton.isVisible());
    monitorButton.bindToTrack (track);

    muteButton.setClickingTogglesState (true);
    soloButton.setClickingTogglesState (true);
    armButton.setClickingTogglesState (true);
    muteButton.setTooltip ("Mute");
    soloButton.setTooltip ("Solo");
    muteButton.setVisible (! track->isMasterTrack());
    soloButton.setVisible (! track->isMasterTrack());
    colourSwatch.setVisible (! track->isMasterTrack());

    gainSlider.setTooltip ("Volume");
    panSlider.setTooltip ("Pan");
    colourSwatch.bindToTrack (track);
    colourSwatch.setDescription ("Track colour");

    armButton.onClick = [this]
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        {
            const bool arm = ! EngineHelpers::isTrackArmed (*audioTrack);
            EngineHelpers::armTrackWithDefaultInput (*audioTrack, arm);
            updateFromModel();
            if (onArmChanged) onArmChanged (*track);
        }
    };

    muteButton.onClick = [this]
    {
        if (track == nullptr)
            return;

        track->setMute (! track->isMuted (false));
        updateFromModel();
        if (onMuteChanged) onMuteChanged (*track);
    };

    soloButton.onClick = [this]
    {
        if (track == nullptr)
            return;

        track->setSolo (! track->isSolo (false));
        updateFromModel();
        if (onSoloChanged) onSoloChanged (*track);
    };

    setupInputSelector();
    setupOutputSelector();
    bindMixControls();

    addAndMakeVisible (trackName);
    addAndMakeVisible (kindBadge);
    addAndMakeVisible (colourSwatch);
    addAndMakeVisible (inputLabel);
    addAndMakeVisible (inputSelector);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (outputSelector);
    addAndMakeVisible (armButton);
    addAndMakeVisible (monitorButton);
    addAndMakeVisible (muteButton);
    addAndMakeVisible (soloButton);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (panSlider);

    track->state.addListener (this);
    refreshRoutingControls();
    updateFromModel();
    updateRoutingLayout();
}

TrackHeaderComponent::~TrackHeaderComponent()
{
    track->state.removeListener (this);
}

void TrackHeaderComponent::paint (juce::Graphics& g)
{
    const auto theme = AppLookAndFeel::getCurrentTheme();
    const bool selected = editViewState.selectionManager.isSelected (track.get());
    auto bg = track->isMasterTrack()
                  ? juce::Colour (0xff222240)
                  : AppColours::headerBackground (theme);

    if (hovered)
        bg = bg.brighter (0.05f);

    g.fillAll (bg);

    if (track->isMasterTrack())
    {
        g.setColour (juce::Colour (0xff4a6fa5));
        g.fillRect (0, 0, 4, getHeight());
    }
    else if (! track->isFolderTrack() && ! EngineHelpers::isReturnTrack (*track))
    {
        auto accent = EngineHelpers::getTrackDisplayColour (*track);

        if (selected)
            accent = accent.brighter (0.2f);

        g.setColour (accent);
        g.fillRect (0, 0, 4, getHeight());
    }

    if (selected)
    {
        g.setColour (AppColours::clipSelectedBorder (theme).withAlpha (0.55f));
        g.drawRect (getLocalBounds().reduced (1));
    }

    if (dropHighlightActive)
    {
        g.setColour (AppColours::accentValidDrop (theme).withAlpha (0.35f));

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
        g.setColour (AppColours::accentFrozen (theme).withAlpha (0.12f));
        g.fillRect (getLocalBounds());
        g.setColour (AppColours::accentFrozen (theme));
        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
        g.drawText ("FROZEN", getLocalBounds().removeFromBottom (12).withTrimmedLeft (4),
                    juce::Justification::centredLeft, false);
    }

    g.setColour (AppColours::trackSeparator (theme));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void TrackHeaderComponent::mouseEnter (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    hovered = true;
    repaint();
}

void TrackHeaderComponent::mouseExit (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    hovered = false;
    repaint();
}

void TrackHeaderComponent::updateFromModel()
{
    muteButton.setToggleState (track->isMuted (false), juce::dontSendNotification);
    soloButton.setToggleState (track->isSolo (false), juce::dontSendNotification);

    const auto theme = AppLookAndFeel::getCurrentTheme();
    bool armed = false;
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        armed = EngineHelpers::isTrackArmed (*audioTrack);

    armButton.setToggleState (armed, juce::dontSendNotification);
    armButton.setColour (juce::TextButton::buttonOnColourId, AppColours::armActive (theme));
    armButton.setColour (juce::TextButton::buttonColourId,
                         armed ? AppColours::armActive (theme) : AppColours::monitorOff (theme));

    monitorButton.updateFromModel();
    colourSwatch.updateFromModel();
    gainSlider.updateFromModel();
    panSlider.updateFromModel();
}

void TrackHeaderComponent::layoutModeChanged()
{
    resized();
}

bool TrackHeaderComponent::isCompactLayout() const
{
    return getWidth() < compactWidthThreshold;
}

int TrackHeaderComponent::getPreferredHeight (const te::Track& track)
{
    int height = nameRowHeight + buttonRowHeight + mixRowHeight;

    if (TrackInputRouting::shouldShowAudioSource (track)
        || TrackInputRouting::shouldShowMidiSource (track))
        height += routingRowHeight;

    if (TrackOutputRouting::shouldShowOutputSelector (track))
        height += routingRowHeight;

    return height;
}

void TrackHeaderComponent::resized()
{
    auto r = getLocalBounds().reduced (2);
    const int btnW = 26;
    const int indent = EngineHelpers::getTrackIndentLevel (*track) * 12;
    const bool compact = isCompactLayout();

    const bool showInput = TrackInputRouting::shouldShowAudioSource (*track)
                           || TrackInputRouting::shouldShowMidiSource (*track);
    const bool showOutput = TrackOutputRouting::shouldShowOutputSelector (*track);

    inputLabel.setVisible (showInput);
    inputSelector.setVisible (showInput);
    outputLabel.setVisible (showOutput);
    outputSelector.setVisible (showOutput);

    auto nameRow = r.removeFromTop (nameRowHeight);
    nameRow.removeFromLeft (indent);
    colourSwatch.setBounds (nameRow.removeFromRight (14).reduced (0, 4));
    kindBadge.setBounds (nameRow.removeFromLeft (compact ? 0 : 36).reduced (0, 3));
    if (compact)
        kindBadge.setVisible (false);
    else
        kindBadge.setVisible (true);
    nameRow.removeFromLeft (compact ? 0 : 4);
    trackName.setBounds (nameRow);

    if (showInput)
    {
        auto row = r.removeFromTop (routingRowHeight);
        row.removeFromLeft (indent);
        inputLabel.setBounds (row.removeFromLeft (42));
        inputSelector.setBounds (row);
    }

    if (showOutput)
    {
        auto row = r.removeFromTop (routingRowHeight);
        row.removeFromLeft (indent);
        outputLabel.setBounds (row.removeFromLeft (42));
        outputSelector.setBounds (row);
    }

    auto buttonRow = r.removeFromTop (buttonRowHeight);
    buttonRow.removeFromLeft (indent);
    soloButton.setBounds (buttonRow.removeFromRight (btnW));
    muteButton.setBounds (buttonRow.removeFromRight (btnW));
    if (monitorButton.isVisible())
        monitorButton.setBounds (buttonRow.removeFromRight (btnW * 2));
    if (armButton.isVisible())
        armButton.setBounds (buttonRow.removeFromRight (btnW));

    auto mixRow = r.removeFromTop (mixRowHeight);
    mixRow.removeFromLeft (indent);

    if (compact)
    {
        gainSlider.setBounds (mixRow.removeFromTop (mixRowHeight / 2).reduced (0, 1));
        panSlider.setBounds (mixRow.reduced (0, 1));
    }
    else
    {
        panSlider.setBounds (mixRow.removeFromRight (56).reduced (2, 1));
        gainSlider.setBounds (mixRow.reduced (0, 1));
    }
}

void TrackHeaderComponent::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;

    if (e.mods.isPopupMenu())
    {
        editViewState.selectionManager.selectOnly (track.get());
        showHeaderContextMenu (e.getScreenPosition());
        return;
    }

    if (e.mods.isLeftButtonDown())
    {
        if (auto* target = e.eventComponent)
        {
            if (target != this
                && (dynamic_cast<juce::Button*> (target) != nullptr
                    || dynamic_cast<juce::Slider*> (target) != nullptr))
                return;
        }

        editViewState.selectionManager.selectOnly (track.get());
        if (onTrackSelected)
            onTrackSelected (*track);
    }
}

bool TrackHeaderComponent::hitTest (int x, int y)
{
    juce::ignoreUnused (x);

    if (track == nullptr)
        return false;

    return y < getPreferredHeight (*track);
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

void TrackHeaderComponent::bindMixControls()
{
    if (track->isMasterTrack())
        volumePlugin = track->edit.getMasterVolumePlugin();
    else if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        volumePlugin = audioTrack->getVolumePlugin();
    else if (auto* folderTrack = dynamic_cast<te::FolderTrack*> (track.get()))
        volumePlugin = folderTrack->getVolumePlugin();

    gainSlider.bindToPlugin (volumePlugin);
    panSlider.bindToPlugin (volumePlugin);
}

void TrackHeaderComponent::setupInputSelector()
{
    inputSelector.menuFactory = [this]
    {
        if (TrackInputRouting::shouldShowAudioSource (*track))
            return buildInputMenu (TrackInputKind::audio, audioSourceOptions);

        return buildInputMenu (TrackInputKind::midi, midiSourceOptions);
    };

    inputSelector.onItemSelected = [this] (int result)
    {
        auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());
        if (audioTrack == nullptr)
            return;

        const auto kind = TrackInputRouting::shouldShowAudioSource (*track)
                              ? TrackInputKind::audio
                              : TrackInputKind::midi;
        const auto& options = kind == TrackInputKind::audio ? audioSourceOptions : midiSourceOptions;

        if (! juce::isPositiveAndBelow (result - 1, options.size()))
            return;

        const auto& option = options[result - 1];
        TrackInputRouting::setActiveSource (*audioTrack, option, kind);
        refreshRoutingControls();
        updateFromModel();
    };
}

void TrackHeaderComponent::setupOutputSelector()
{
    outputSelector.menuFactory = [this]
    {
        return buildOutputMenu (outputOptions);
    };

    outputSelector.onItemSelected = [this] (int result)
    {
        auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get());
        if (audioTrack == nullptr)
            return;

        if (! juce::isPositiveAndBelow (result - 1, outputOptions.size()))
            return;

        juce::String error;
        if (! TrackOutputRouting::setActiveOutput (*audioTrack, outputOptions[result - 1], error))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Output Routing",
                                                    error.isNotEmpty() ? error : "Could not set output routing");
            return;
        }

        refreshRoutingControls();
    };
}

juce::PopupMenu TrackHeaderComponent::buildInputMenu (TrackInputKind kind,
                                                      juce::Array<TrackInputOption>& storedOptions)
{
    juce::PopupMenu menu;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        storedOptions = TrackInputRouting::getSourceOptions (editViewState.edit, *audioTrack, kind);

        for (int i = 0; i < storedOptions.size(); ++i)
        {
            const auto& opt = storedOptions.getReference (i);
            menu.addItem (i + 1, opt.displayName, opt.available);
        }
    }

    return menu;
}

juce::PopupMenu TrackHeaderComponent::buildOutputMenu (juce::Array<TrackOutputOption>& storedOptions)
{
    juce::PopupMenu menu;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        storedOptions = TrackOutputRouting::getOutputOptions (editViewState.edit, *audioTrack);

        juce::PopupMenu hardwareMenu;
        juce::PopupMenu trackMenu;

        for (int i = 0; i < storedOptions.size(); ++i)
        {
            const auto& opt = storedOptions.getReference (i);

            if (opt.type == TrackOutputOption::Type::master)
            {
                menu.addItem (i + 1, opt.displayName, opt.available);
            }
            else if (opt.type == TrackOutputOption::Type::hardware)
            {
                hardwareMenu.addItem (i + 1, opt.displayName, opt.available);
            }
            else
            {
                trackMenu.addItem (i + 1, opt.displayName, opt.available);
            }
        }

        if (hardwareMenu.getNumItems() > 0)
            menu.addSubMenu ("Hardware", hardwareMenu);

        if (trackMenu.getNumItems() > 0)
            menu.addSubMenu ("Tracks", trackMenu);
    }

    return menu;
}

void TrackHeaderComponent::refreshRoutingControls()
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
    {
        if (TrackInputRouting::shouldShowAudioSource (*track))
        {
            const auto active = TrackInputRouting::getActiveSource (*audioTrack, TrackInputKind::audio);
            inputSelector.setDisplayText (active.displayName, active.available);
            inputSelector.setRouteTooltip (TrackInputRouting::getInputTooltip (*audioTrack, TrackInputKind::audio));
        }
        else if (TrackInputRouting::shouldShowMidiSource (*track))
        {
            const auto active = TrackInputRouting::getActiveSource (*audioTrack, TrackInputKind::midi);
            inputSelector.setDisplayText (active.displayName, active.available);
            inputSelector.setRouteTooltip (TrackInputRouting::getInputTooltip (*audioTrack, TrackInputKind::midi));
        }

        if (TrackOutputRouting::shouldShowOutputSelector (*track))
        {
            const auto active = TrackOutputRouting::getActiveOutput (*audioTrack);
            outputSelector.setDisplayText (active.displayName, active.available);
            outputSelector.setRouteTooltip (TrackOutputRouting::getOutputTooltip (*audioTrack));
        }
    }

    repaint();
}

void TrackHeaderComponent::updateRoutingLayout()
{
    resized();
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
    const auto desc = details.description.toString();

    if (desc.startsWith (PluginDragTypes::browserInsert)
        || desc.startsWith (PluginDragTypes::crossTrack))
        return track != nullptr && track->canContainPlugins();

    if (track != nullptr && track->isMasterTrack())
        return false;

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

    const auto descStr = details.description.toString();

    if (descStr.startsWith (PluginDragTypes::browserInsert))
    {
        const auto idStr = descStr.fromFirstOccurrenceOf (":", false, false);
        insertBrowserPlugin (EngineHelpers::lookupKnownPlugin (editViewState.edit.engine, idStr));
        repaint();
        return;
    }

    if (descStr.startsWith (PluginDragTypes::crossTrack))
    {
        const auto payload = PluginDragPayload::parse (details.description);

        if (payload.kind == PluginDragPayload::Kind::crossTrack)
            handleCrossTrackDrop (payload);

        repaint();
        return;
    }

    const auto zone = dropZoneForPosition (details.localPosition);
    moveSelectedTracksToDropZone (zone);
    repaint();
}

void TrackHeaderComponent::insertBrowserPlugin (const juce::PluginDescription& desc)
{
    if (createPlugin == nullptr || desc.name.isEmpty() || track == nullptr || ! track->canContainPlugins())
        return;

    if (auto plugin = createPlugin (desc))
    {
        TrackPluginChainModel model (*track);
        const int insertIndex = model.resolveInsertIndex (model.getUserChainSize(),
                                                          EngineHelpers::isInstrumentDescription (desc),
                                                          nullptr);
        if (insertIndex >= 0)
        {
            if (EngineHelpers::insertPluginOnTrack (*track, plugin, insertIndex) != nullptr)
            {
                if (onPluginInserted)
                    onPluginInserted (desc);
            }
            else
            {
                EngineHelpers::showPluginInsertFailureAlert (this, desc);
            }
        }
    }
    else
    {
        EngineHelpers::showPluginInsertFailureAlert (this, desc);
    }
}

void TrackHeaderComponent::handleCrossTrackDrop (const PluginDragPayload& payload)
{
    if (track == nullptr || ! track->canContainPlugins())
        return;

    TrackPluginChainModel model (*track);
    const int userSlot = model.getUserChainSize();

    for (auto t : te::getAllTracks (editViewState.edit))
    {
        if (t == nullptr || t->itemID != payload.sourceTrackId || ! t->canContainPlugins())
            continue;

        for (auto p : t->pluginList)
        {
            if (p->itemID == payload.pluginId)
            {
                EngineHelpers::movePluginToTrack (*p, *track, userSlot);
                return;
            }
        }
    }
}

void TrackHeaderComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::name)
    {
        trackName.setText (track->getName(), juce::dontSendNotification);
        refreshRoutingControls();
    }
    else if (id == EngineHelpers::trackKindProperty)
        updateKindBadge();
    else if (id == te::IDs::frozen || id == te::IDs::frozenIndividually)
        repaint();
    else if (id == te::IDs::mute || id == te::IDs::solo)
        updateFromModel();
    else if (id == TrackMonitorRouting::monitorModeProperty)
        monitorButton.updateFromModel();
    else if (id == te::IDs::colour)
        colourSwatch.updateFromModel();
}

void TrackHeaderComponent::updateKindBadge()
{
    if (track->isMasterTrack())
    {
        kindBadge.setText ("MASTER", juce::dontSendNotification);
        kindBadge.setColour (juce::Label::backgroundColourId, juce::Colour (0xff4a6fa5));
        return;
    }

    if (track->isFolderTrack())
    {
        kindBadge.setText ("FOLDER", juce::dontSendNotification);
        kindBadge.setColour (juce::Label::backgroundColourId, AppColours::trackAccentFolder (AppLookAndFeel::getCurrentTheme()));
        return;
    }

    if (EngineHelpers::isReturnTrack (*track))
    {
        kindBadge.setText ("RETURN", juce::dontSendNotification);
        kindBadge.setColour (juce::Label::backgroundColourId, AppColours::trackAccentReturn (AppLookAndFeel::getCurrentTheme()));
        return;
    }

    const bool isMidi = EngineHelpers::isMidiKindTrack (*track);
    kindBadge.setText (isMidi ? "MIDI" : "AUDIO", juce::dontSendNotification);
    kindBadge.setColour (juce::Label::backgroundColourId,
                         isMidi ? AppColours::trackAccentMidi (AppLookAndFeel::getCurrentTheme())
                                : AppColours::trackAccentAudio (AppLookAndFeel::getCurrentTheme()));
    refreshRoutingControls();
    updateRoutingLayout();
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

    if (auto* containingTrack = te::getTrackContainingPlugin (plugin->edit, plugin.get()))
        if (EngineHelpers::isPluginSoloed (*containingTrack, *plugin))
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
        if (track == nullptr || ! track->canContainPlugins())
            return;

        if (result == insertRack)
        {
            if (auto* rack = EngineHelpers::insertEmptyRack (*track))
                setExpandedRack (rack);
        }
        else if (result == groupRack)
        {
            groupSelectedIntoRack();
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
        || desc.startsWith (PluginDragTypes::browserInsert)
        || desc.startsWith (PluginDragTypes::crossTrack);
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
    else if (desc.startsWith (PluginDragTypes::crossTrack))
    {
        const auto payload = PluginDragPayload::parse (details.description);
        if (payload.kind == PluginDragPayload::Kind::crossTrack)
            handleCrossTrackDrop (payload, slot);
    }
}

void TrackFooterComponent::insertBrowserPlugin (const juce::PluginDescription& desc, int slotIndex)
{
    if (createPlugin == nullptr || desc.name.isEmpty() || track == nullptr || ! track->canContainPlugins())
        return;

    if (auto plugin = createPlugin (desc))
    {
        TrackPluginChainModel model (*track);
        const int userSlot = juce::jlimit (0, model.getUserChainSize(), slotIndex);
        const int insertIndex = model.resolveInsertIndex (userSlot,
                                                          EngineHelpers::isInstrumentDescription (desc),
                                                          nullptr);
        if (insertIndex >= 0)
        {
            if (EngineHelpers::insertPluginOnTrack (*track, plugin, insertIndex) != nullptr)
            {
                if (onPluginInserted)
                    onPluginInserted (desc);
            }
            else
            {
                EngineHelpers::showPluginInsertFailureAlert (this, desc);
            }
        }
    }
    else
    {
        EngineHelpers::showPluginInsertFailureAlert (this, desc);
    }
}

void TrackFooterComponent::handleCrossTrackDrop (const PluginDragPayload& payload, int slotIndex)
{
    if (track == nullptr || ! track->canContainPlugins())
        return;

    TrackPluginChainModel model (*track);
    const int userSlot = juce::jlimit (0, model.getUserChainSize(), slotIndex);

    for (auto t : te::getAllTracks (editViewState.edit))
    {
        if (t == nullptr || t->itemID != payload.sourceTrackId || ! t->canContainPlugins())
            continue;

        for (auto p : t->pluginList)
        {
            if (p->itemID == payload.pluginId)
            {
                EngineHelpers::movePluginToTrack (*p, *track, userSlot);
                return;
            }
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
    // Left-drag on clip lanes creates a time-range highlight. Without this flag
    // the timeline viewport's scroll-on-drag steals horizontal drags (especially
    // right-to-left) before the lane can extend the selection.
    if (canDragSelectTimeRange())
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
    const auto theme = AppLookAndFeel::getCurrentTheme();

    if (track->isFolderTrack())
    {
        g.fillAll (AppColours::folderLaneBackground (theme));
        g.setColour (AppColours::trackAccentFolder (theme).withAlpha (0.5f));
        g.drawHorizontalLine (0, 0.0f, (float) getWidth());
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        return;
    }

    g.fillAll (AppColours::laneBackground (theme));
    g.setColour (AppColours::trackSeparator (theme));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
    editViewState.laneBackgroundCache.renderOrFetch (g, editViewState.edit, editViewState,
                                                     track->itemID, getLocalBounds());

    if (isLaneLevelRendering())
    {
        if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get()))
            paintLaneClipSummaries (g, editViewState, *clipTrack, getLocalBounds());
    }
}

bool TrackLaneComponent::canDragCreateClips() const
{
    return EngineHelpers::canHostMidiClips (*track);
}

bool TrackLaneComponent::canDragSelectTimeRange() const
{
    return ! track->isFolderTrack();
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
    if (e.mods.isPopupMenu() || e.mods.isRightButtonDown())
    {
        showLaneContextMenu (e);
        return;
    }

    if (isLaneLevelRendering() && e.mods.isLeftButtonDown())
    {
        if (auto* clip = findClipAtX (e.x))
        {
            ArrangementSelectionHelpers::handleClipClick (editViewState, *clip, e.mods);
            clearRangeSelection();
            repaint();
            return;
        }

        editViewState.selectionManager.deselectAll();
        repaint();
    }

    if (! e.mods.isLeftButtonDown())
        return;

    pendingTimelineInteraction = true;
    clearRangeSelection();
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

    if (onEmptyLaneDrag != nullptr)
        onEmptyLaneDrag (*this, e);
}

void TrackLaneComponent::mouseUp (const juce::MouseEvent& e)
{
    if (! pendingTimelineInteraction)
        return;

    pendingTimelineInteraction = false;

    if (onEmptyLaneDragEnd != nullptr && onEmptyLaneDragEnd (*this, e))
        return;

    placePlayheadAtX (e.x);
}

void TrackLaneComponent::cancelTimelineInteraction()
{
    pendingTimelineInteraction = false;
}

bool TrackLaneComponent::hasRangeSelection() const
{
    if (auto* timeline = findParentComponentOfClass<TimelineComponent>())
        return timeline->hasTimeSelection();

    return false;
}

void TrackLaneComponent::clearRangeSelection()
{
    if (auto* timeline = findParentComponentOfClass<TimelineComponent>())
        timeline->clearTimeSelection();
}

void TrackLaneComponent::applyRangeSelectionToLoop()
{
    if (auto* timeline = findParentComponentOfClass<TimelineComponent>())
        timeline->applyTimeSelectionToLoop();
}

void TrackLaneComponent::createMidiClipFromRangeSelection()
{
    auto* timeline = findParentComponentOfClass<TimelineComponent>();
    if (timeline == nullptr || ! timeline->hasTimeSelection() || ! canDragCreateClips())
        return;

    if (auto clip = EngineHelpers::createMidiClipOnTrack (*track, timeline->getTimeSelection()))
        editViewState.selectionManager.selectOnly (clip.get());

    timeline->clearTimeSelection();
}

void TrackLaneComponent::showLaneContextMenu (const juce::MouseEvent& e)
{
    auto* timeline = findParentComponentOfClass<TimelineComponent>();
    const bool hasSelection = timeline != nullptr && timeline->hasTimeSelection();
    const auto insertTime = hasSelection ? timeline->getTimeSelection().getStart()
                                         : TimelineGrid::snapTime (editViewState.edit, editViewState,
                                                                   editViewState.xToTime (e.x));
    setInsertPoint (editViewState, track.get(), insertTime);

    showTimelineContextMenu (*this, e.getScreenPosition(), editViewState, track.get(),
                             canDragCreateClips() && hasSelection,
                             [this] { createMidiClipFromRangeSelection(); },
                             nullptr, onShowClipProperties, onEditWarpMarkers, onAudioToMidi, onTakeLanesChanged,
                             [this]
                             {
                                 if (onTakeLanesChanged)
                                     onTakeLanesChanged();
                                 if (onClipSelectionChanged)
                                     onClipSelectionChanged();
                             },
                             onExportClipToLibrary,
                             groovePool,
                             hasSelection,
                             [this] { applyRangeSelectionToLoop(); });
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
            if (EngineHelpers::isSessionClip (*c))
                continue;

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
            cc->onEditWarpMarkers = [this] (te::Clip* c)
            {
                if (onEditWarpMarkers && c != nullptr)
                    onEditWarpMarkers (c);
            };
            cc->onAudioToMidi = [this] (te::Clip* c, AudioToMidiMode mode)
            {
                if (onAudioToMidi && c != nullptr)
                    onAudioToMidi (c, mode);
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
            cc->onDragOverlayUpdate = [this] (te::Clip& c, ClipComponent::DragMode mode,
                                              te::TimePosition snapTime, te::TimePosition ghostStart,
                                              te::TimePosition ghostEnd)
            {
                if (onClipDragOverlayUpdate)
                    onClipDragOverlayUpdate (c, mode, snapTime, ghostStart, ghostEnd);
            };
            cc->onDragOverlayClear = [this]
            {
                if (onClipDragOverlayClear)
                    onClipDragOverlayClear();
            };
            cc->onExportToLibrary = [this] (te::Clip& c)
            {
                if (onExportClipToLibrary)
                    onExportClipToLibrary (c);
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
    const auto text = details.description.toString();
    return text.startsWith (PluginDragTypes::browserInsert)
        || text.startsWith (ContentDragTypes::sampleInsert)
        || text.startsWith (ContentDragTypes::clipPreset);
}

void TrackLaneComponent::itemDropped (const SourceDetails& details)
{
    const auto descStr = details.description.toString();

    if (descStr.startsWith (ContentDragTypes::sampleInsert))
    {
        const auto payload = ContentDragPayload::parse (details.description);

        if (payload.isValid())
            insertSampleAtX (payload.file, details.localPosition.x);

        return;
    }

    if (descStr.startsWith (ContentDragTypes::clipPreset))
    {
        const auto payload = ClipPresetDragPayload::parse (details.description);

        if (payload.isValid())
            insertClipPresetAtX (payload.presetFile, details.localPosition.x);

        return;
    }

    if (! descStr.startsWith (PluginDragTypes::browserInsert) || createPlugin == nullptr)
        return;

    const auto idStr = descStr.fromFirstOccurrenceOf (":", false, false);
    const auto pd = EngineHelpers::lookupKnownPlugin (editViewState.edit.engine, idStr);

    if (pd.name.isEmpty())
        return;

    if (track != nullptr && track->canContainPlugins())
    {
        if (auto plugin = createPlugin (pd))
        {
            TrackPluginChainModel model (*track);
            const int insertIndex = model.resolveInsertIndex (model.getUserChainSize(),
                                                              EngineHelpers::isInstrumentDescription (pd),
                                                              nullptr);
            if (insertIndex >= 0)
            {
                if (EngineHelpers::insertPluginOnTrack (*track, plugin, insertIndex) != nullptr)
                {
                    if (onPluginInserted)
                        onPluginInserted (pd);
                }
                else
                    EngineHelpers::showPluginInsertFailureAlert (this, pd);
            }
        }
        else
            EngineHelpers::showPluginInsertFailureAlert (this, pd);
    }
}

bool TrackLaneComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        if (isSupportedAudioFile (juce::File (path)))
            return true;
    }

    return false;
}

void TrackLaneComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (y);

    for (const auto& path : files)
    {
        const juce::File file (path);

        if (isSupportedAudioFile (file))
        {
            insertSampleAtX (file, x);
            break;
        }
    }
}

bool TrackLaneComponent::isSupportedAudioFile (const juce::File& file) const
{
    return editViewState.edit.engine.getAudioFileFormatManager().readFormatManager
               .findFormatForFileExtension (file.getFileExtension()) != nullptr;
}

te::Clip* TrackLaneComponent::insertSampleAtX (const juce::File& file, int localX)
{
    auto* clipTrack = dynamic_cast<te::ClipTrack*> (track.get());

    if (clipTrack == nullptr || ! file.existsAsFile())
        return nullptr;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track.get()))
        EngineHelpers::ensureSamplerOnMidiTrack (*audioTrack);

    const auto time = TimelineGrid::snapTime (editViewState.edit, editViewState,
                                              editViewState.xToTime (localX));

    if (auto* clip = EngineHelpers::insertWaveClipFromFile (*clipTrack, file, time, {}))
    {
        editViewState.selectionManager.selectOnly (clip);
        markAndUpdate (updateClips);

        if (onSampleInserted)
            onSampleInserted (file, clip);

        if (onClipSelectionChanged)
            onClipSelectionChanged();

        return clip;
    }

    return nullptr;
}

te::Clip* TrackLaneComponent::insertClipPresetAtX (const juce::File& presetFile, int localX)
{
    if (onClipPresetDropped != nullptr)
        return onClipPresetDropped (presetFile, localX);

    return nullptr;
}

PlayheadOverlay::PlayheadOverlay (te::Edit& e, EditViewState& evs, UiTelemetryHub* hub)
    : edit (e), editViewState (evs), telemetryHub (hub)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (60);

    if (telemetryHub != nullptr)
        telemetryHub->registerPlayhead (this);
}

PlayheadOverlay::~PlayheadOverlay()
{
    stopTimer();

    if (telemetryHub != nullptr)
        telemetryHub->unregisterPlayhead (this);
}

void PlayheadOverlay::paint (juce::Graphics& g)
{
    const auto theme = AppLookAndFeel::getCurrentTheme();
    const int x = displayXPosition;

    g.setColour (AppColours::playheadGlow (theme).withAlpha (0.15f));
    g.fillRect (x - 1, 0, 3, getHeight());

    g.setColour (AppColours::playheadLine (theme));
    g.fillRect (x, 0, 1, getHeight());
}

void PlayheadOverlay::updateFromTransport()
{
    if (getWidth() <= 0)
        return;

    targetXPosition = editViewState.timeToX (edit.getTransport().getPosition());
    lastTransportUpdateMs = juce::Time::getMillisecondCounterHiRes();

    if (std::abs (targetXPosition - displayXPosition) > 0)
        repaint();
}

void PlayheadOverlay::timerCallback()
{
    if (getWidth() <= 0)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto elapsed = now - lastTransportUpdateMs;

    if (elapsed < 50.0 && targetXPosition != displayXPosition)
    {
        const float t = (float) juce::jlimit (0.0, 1.0, elapsed / 50.0);
        const int newDisplay = (int) std::round (displayXPosition + (targetXPosition - displayXPosition) * t);

        if (newDisplay != displayXPosition)
        {
            displayXPosition = newDisplay;
            repaint();
        }
    }
    else if (displayXPosition != targetXPosition)
    {
        displayXPosition = targetXPosition;
        repaint();
    }
}

} // namespace skeletonhive
