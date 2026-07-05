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
    createMidiClip,
    newFolderTrack
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

    menu.addSeparator();
    menu.addItem ((int) TimelineMenuResult::newFolderTrack, "New Folder Track");

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
            case TimelineMenuResult::newFolderTrack:
                EngineHelpers::createFolderTrack (editViewState.edit, &editViewState.selectionManager);
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
    armButton.setVisible (dynamic_cast<te::AudioTrack*> (track.get()) != nullptr);

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
    if (armButton.isVisible())
        armButton.setBounds (r.removeFromRight (btnW));
    kindBadge.setBounds (r.removeFromLeft (42).reduced (0, 3));
    r.removeFromLeft (4);

    // Indent nested tracks under their FolderTrack parent(s).
    r.removeFromLeft (EngineHelpers::getTrackIndentLevel (*track) * 12);
    trackName.setBounds (r);
}

void TrackHeaderComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == te::IDs::name)
        trackName.setText (track->getName(), juce::dontSendNotification);
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

void PluginSlotButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    if (EngineHelpers::isPluginSoloed (*te::getTrackContainingPlugin (plugin->edit, plugin.get()), *plugin))
    {
        g.setColour (juce::Colours::gold.withAlpha (0.35f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
    }

    if (plugin->canSidechain() && plugin->getSidechainSourceName().isNotEmpty())
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
        sidechainBase = 200,
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
    {
        juce::PopupMenu sidechainMenu;
        const auto sources = plugin->getSidechainSourceNames (true);
        const auto current = plugin->getSidechainSourceName();

        for (int i = 0; i < sources.size(); ++i)
            sidechainMenu.addItem (sidechainBase + i, sources[i], true, sources[i] == current);

        menu.addSubMenu ("Sidechain Source", sidechainMenu, true);
    }

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

        if (result >= sidechainBase && result < moveToRackBase)
        {
            const auto sources = p.getSidechainSourceNames (true);
            const int idx = result - sidechainBase;
            if (juce::isPositiveAndBelow (idx, sources.size()))
                p.setSidechainSourceByName (sources[idx]);
            return;
        }

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
    return EngineHelpers::canHostMidiClips (*track);
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
        if (auto plugin = createPlugin (pd))
            EngineHelpers::insertPluginOnTrack (*audioTrack, plugin);
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
