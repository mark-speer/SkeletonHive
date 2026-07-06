#include "ClipSlotComponent.h"
#include "Engine/ContentDragManager.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

ClipSlotComponent::ClipSlotComponent (SessionManager& session, SessionMidiMapper& midiMapper,
                                      EditViewState& viewState, ClipLibraryManager* clipLibrary,
                                      te::EditItemID track, int scene)
    :     sessionManager (session),
      sessionMidiMapper (midiMapper),
      editViewState (viewState),
      clipLibraryManager (clipLibrary),
      trackId (track),
      sceneIndex (scene)
{
    refresh();
}

void ClipSlotComponent::refresh()
{
    repaint();
}

void ClipSlotComponent::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

te::Clip* ClipSlotComponent::getClip() const
{
    return sessionManager.getSlotClip (trackId, sceneIndex);
}

ClipSlotState ClipSlotComponent::getState() const
{
    if (getClip() == nullptr)
        return ClipSlotState::empty;

    if (sessionManager.isSlotRecording (trackId, sceneIndex))
        return ClipSlotState::recording;

    if (sessionManager.isSlotPlaying (trackId, sceneIndex))
        return ClipSlotState::playing;

    return ClipSlotState::loaded;
}

void ClipSlotComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto state = getState();

    juce::Colour fill (0xff2a3344);
    if (auto* clip = getClip())
        fill = EngineHelpers::getClipFillColour (*clip, fill);

    g.setColour (fill.darker (state == ClipSlotState::empty ? 0.35f : 0.0f));
    g.fillRoundedRectangle (bounds, 4.0f);

    juce::Colour border (0xff56657a);
    if (selected)
        border = juce::Colour (0xff48cae4);
    else if (state == ClipSlotState::playing)
        border = juce::Colour (0xff06d6a0);
    else if (state == ClipSlotState::recording)
        border = juce::Colour (0xffef476f);

    g.setColour (border);
    g.drawRoundedRectangle (bounds, 4.0f, state == ClipSlotState::playing ? 2.5f : 1.0f);

    if (auto* clip = getClip())
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (clip->getName(), getLocalBounds().reduced (4), juce::Justification::centred, 2);
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("+", getLocalBounds(), juce::Justification::centred);
    }

    const auto followAction = sessionManager.getSlotFollowAction (trackId, sceneIndex);
    const bool legato = sessionManager.getSlotLegatoLaunch (trackId, sceneIndex);
    const bool midiMapped = sessionMidiMapper.hasMappingForSlot (trackId, sceneIndex);

    if (followAction != FollowAction::none || legato || midiMapped)
    {
        auto badge = getLocalBounds().removeFromTop (10).removeFromRight (10).reduced (2).toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.fillEllipse (badge);

        if (followAction != FollowAction::none)
        {
            g.setColour (juce::Colour (0xff1b263b));
            g.setFont (juce::FontOptions (7.0f));
            g.drawText ("F", badge.toNearestInt(), juce::Justification::centred);
        }
    }
}

void ClipSlotComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    if (onTrackFocus)
        onTrackFocus (trackId, sceneIndex);
}

void ClipSlotComponent::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && e.mods.isPopupMenu())
    {
        showContextMenu (e.getScreenPosition());
        return;
    }

    if (e.mouseWasClicked() && getClip() != nullptr)
        sessionManager.toggleSlot (trackId, sceneIndex);
}

bool ClipSlotComponent::isInterestedInDragSource (const SourceDetails& details)
{
    if (auto* obj = details.description.getDynamicObject())
    {
        const auto type = obj->getProperty ("type").toString();
        return type == ContentDragTypes::sampleInsert
            || type == ContentDragTypes::clipPreset;
    }

    return false;
}

te::ClipTrack* ClipSlotComponent::getTrack() const
{
    for (auto track : te::getAllTracks (editViewState.edit))
    {
        if (track->itemID == trackId)
            return dynamic_cast<te::ClipTrack*> (track);
    }

    return nullptr;
}

void ClipSlotComponent::itemDropped (const SourceDetails& details)
{
    auto* clipTrack = getTrack();
    if (clipTrack == nullptr)
        return;

    if (auto* obj = details.description.getDynamicObject())
    {
        const auto type = obj->getProperty ("type").toString();

        if (type == ContentDragTypes::sampleInsert)
        {
            const auto payload = ContentDragPayload::parse (details.description);
            if (payload.isValid())
                sessionManager.loadSampleIntoSlot (*clipTrack, sceneIndex, payload.file);
        }
        else if (type == ContentDragTypes::clipPreset && clipLibraryManager != nullptr)
        {
            const auto payload = ClipPresetDragPayload::parse (details.description);
            if (payload.isValid())
                sessionManager.loadPresetIntoSlot (*clipTrack, sceneIndex, payload.presetFile, *clipLibraryManager);
        }
    }

    refresh();
}

void ClipSlotComponent::showContextMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;

    const bool hasClip = getClip() != nullptr;
    menu.addItem (1, "Launch", hasClip);
    menu.addItem (2, "Stop", hasClip && sessionManager.isSlotPlaying (trackId, sceneIndex));
    menu.addSeparator();

    const auto currentFollow = sessionManager.getSlotFollowAction (trackId, sceneIndex);
    juce::PopupMenu followMenu;
    followMenu.addItem (10, "None", true, currentFollow == FollowAction::none);
    followMenu.addItem (11, "Play Next", true, currentFollow == FollowAction::playNext);
    followMenu.addItem (12, "Play Previous", true, currentFollow == FollowAction::playPrevious);
    followMenu.addItem (13, "Play Random", true, currentFollow == FollowAction::playRandom);
    followMenu.addItem (14, "Stop", true, currentFollow == FollowAction::stop);
    menu.addSubMenu ("Follow Action", followMenu, hasClip);

    menu.addItem (20, "Legato Launch", true, sessionManager.getSlotLegatoLaunch (trackId, sceneIndex));

    menu.addSeparator();
    menu.addItem (30, "MIDI Learn Launch...", hasClip);
    menu.addItem (31, "MIDI Learn Toggle...", hasClip);
    if (sessionMidiMapper.hasMappingForSlot (trackId, sceneIndex))
        menu.addItem (32, "Remove MIDI Mapping");

    menu.addSeparator();
    menu.addItem (3, "Clear Slot", hasClip);
    menu.addItem (4, "Duplicate to Scene...", hasClip);
    menu.addItem (5, "Commit Loop to Arrangement", hasClip);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [this] (int result)
    {
        switch (result)
        {
            case 1: sessionManager.launchSlot (trackId, sceneIndex); break;
            case 2: sessionManager.stopSlot (trackId, sceneIndex); break;
            case 3: sessionManager.clearSlot (trackId, sceneIndex); refresh(); break;
            case 4: promptDuplicateToScene(); break;
            case 5:
                if (onCommitLoopToArrangement)
                    onCommitLoopToArrangement (trackId, sceneIndex);
                break;
            case 10: sessionManager.setSlotFollowAction (trackId, sceneIndex, FollowAction::none); refresh(); break;
            case 11: sessionManager.setSlotFollowAction (trackId, sceneIndex, FollowAction::playNext); refresh(); break;
            case 12: sessionManager.setSlotFollowAction (trackId, sceneIndex, FollowAction::playPrevious); refresh(); break;
            case 13: sessionManager.setSlotFollowAction (trackId, sceneIndex, FollowAction::playRandom); refresh(); break;
            case 14: sessionManager.setSlotFollowAction (trackId, sceneIndex, FollowAction::stop); refresh(); break;
            case 20:
                sessionManager.setSlotLegatoLaunch (trackId, sceneIndex, ! sessionManager.getSlotLegatoLaunch (trackId, sceneIndex));
                refresh();
                break;
            case 30: sessionMidiMapper.armLearn (trackId, sceneIndex, SessionMidiAction::launchSlot); break;
            case 31: sessionMidiMapper.armLearn (trackId, sceneIndex, SessionMidiAction::toggleSlot); break;
            case 32:
                sessionMidiMapper.removeMappingsForSlot (trackId, sceneIndex);
                refresh();
                break;
            default: break;
        }
    });
}

void ClipSlotComponent::promptDuplicateToScene()
{
    auto w = std::make_shared<juce::AlertWindow> ("Duplicate to Scene",
                                                  "Target scene number (1-based):",
                                                  juce::AlertWindow::QuestionIcon);
    w->addTextEditor ("scene", juce::String (sceneIndex + 2));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, this] (int r)
    {
        if (r != 1)
            return;

        const int target = w->getTextEditorContents ("scene").getIntValue() - 1;
        if (target >= 0 && target < sessionManager.getSceneCount())
        {
            sessionManager.duplicateSlotToScene (trackId, sceneIndex, target);
            refresh();
        }
    }));
}

} // namespace skeletonhive
