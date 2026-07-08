#include "PluginSlotComponent.h"
#include "Engine/EngineHelpers.h"
#include "Engine/MultiOutputRouting.h"
#include "Engine/PluginDragManager.h"
#include "Engine/PluginPresetManager.h"
#include "UI/Plugins/PresetBrowserPanel.h"
#include "UI/AppLookAndFeel.h"
#include "UI/Arrangement/TrackComponents.h"
#include "UI/Routing/SidechainMenu.h"

namespace skeletonhive
{

namespace
{
constexpr int bypassButtonSize = 18;
constexpr int expandButtonSize = 14;
} // namespace

PluginSlotComponent::PluginSlotComponent (EditViewState& evs,
                                          te::Plugin::Ptr p,
                                          PluginStateManager* sm)
    : editViewState (evs), plugin (std::move (p)), stateManager (sm)
{
    setRepaintsOnMouseActivity (true);
    updateEnabledLook();
    refreshLoadState();
}

PluginSlotComponent::~PluginSlotComponent()
{
    stopTimer();
}

void PluginSlotComponent::setSelected (bool isSelected)
{
    if (selected != isSelected)
    {
        selected = isSelected;
        repaint();
    }
}

void PluginSlotComponent::setCollapsed (bool isCollapsed)
{
    if (collapsed != isCollapsed)
    {
        collapsed = isCollapsed;
        repaint();
    }
}

void PluginSlotComponent::setNestedInRack (bool nested)
{
    if (nestedInRack != nested)
    {
        nestedInRack = nested;
        repaint();
    }
}

void PluginSlotComponent::setRackHeader (bool header, bool expanded)
{
    rackHeader = header;
    rackExpanded = expanded;
    repaint();
}

juce::Rectangle<int> PluginSlotComponent::getExpandButtonArea() const
{
    return { 4, 4, expandButtonSize, expandButtonSize };
}

void PluginSlotComponent::updateEnabledLook()
{
    setAlpha (plugin->isEnabled() ? 1.0f : 0.45f);
}

void PluginSlotComponent::refreshLoadState()
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

    if (previous != loadState)
        repaint();
}

void PluginSlotComponent::timerCallback()
{
    refreshLoadState();
}

void PluginSlotComponent::openEditor()
{
    if (plugin->windowState != nullptr)
        plugin->windowState->showWindowExplicitly();
}

void PluginSlotComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    if (e.mods.isRightButtonDown())
        return;

    if (rackHeader && getExpandButtonArea().contains (e.getPosition()))
    {
        if (auto* rack = dynamic_cast<te::RackInstance*> (plugin.get()))
        {
            if (onRackExpandChanged)
                onRackExpandChanged (*rack, ! rackExpanded);
        }
        return;
    }

    const auto bypassArea = juce::Rectangle<int> (getWidth() - bypassButtonSize - 4, 4,
                                                  bypassButtonSize, bypassButtonSize);
    if (bypassArea.contains (e.getPosition()))
    {
        plugin->setEnabled (! plugin->isEnabled());
        updateEnabledLook();
        if (onChanged)
            onChanged (*plugin);
        return;
    }

    if (e.mods.isShiftDown() || e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        if (! editViewState.selectionManager.isSelected (plugin.get()))
            editViewState.selectionManager.addToSelection (plugin.get());
    }
    else
        editViewState.selectionManager.selectOnly (plugin.get());

    setSelected (true);
}

void PluginSlotComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = findParentComponentOfClass<juce::DragAndDropContainer>())
    {
        PluginDragPayload payload;

        if (rackContext != nullptr)
        {
            payload.kind = PluginDragPayload::Kind::slotReorder;
            payload.pluginId = plugin->itemID;
            payload.rackInstanceId = rackContext->itemID;
        }
        else if (auto* track = te::getTrackContainingPlugin (plugin->edit, plugin.get()))
        {
            payload.kind = PluginDragPayload::Kind::crossTrack;
            payload.pluginId = plugin->itemID;
            payload.sourceTrackId = track->itemID;
        }
        else
        {
            return;
        }

        container->startDragging (payload.encode(), this, juce::ScaledImage(), true, nullptr, &e.source);
    }
}

void PluginSlotComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    openEditor();
}

void PluginSlotComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const bool instrument = EngineHelpers::isInstrumentPlugin (*plugin);
    const bool rackInstance = dynamic_cast<te::RackInstance*> (plugin.get()) != nullptr;
    const auto theme = AppLookAndFeel::getCurrentTheme();
    auto baseColour = instrument ? AppColours::pluginSlotInstrument (theme)
                                 : AppColours::pluginSlotEffect (theme);

    if (loadState == EngineHelpers::PluginLoadState::failed)
        baseColour = AppColours::pluginSlotFailed (theme);
    else if (loadState == EngineHelpers::PluginLoadState::loading)
        baseColour = AppColours::pluginSlotLoading (theme);

    if (rackInstance)
        baseColour = AppColours::pluginSlotBypassed (theme);
    if (nestedInRack)
        baseColour = baseColour.brighter (0.08f);

    g.setColour (baseColour.brighter (selected ? 0.35f : 0.1f));
    g.fillRoundedRectangle (bounds, 4.0f);

    if (nestedInRack)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawLine (1.0f, 0.0f, 1.0f, (float) getHeight(), 2.0f);
    }

    if (selected)
    {
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (bounds, 4.0f, 2.0f);
    }

    if (EngineHelpers::isPluginSoloed (*te::getTrackContainingPlugin (plugin->edit, plugin.get()), *plugin))
    {
        g.setColour (juce::Colours::gold.withAlpha (0.35f));
        g.fillRoundedRectangle (bounds, 4.0f);
    }

    if (plugin->canSidechain() && plugin->getSidechainSourceID().isValid())
    {
        g.setColour (juce::Colours::cyan);
        g.fillEllipse (4.0f, 4.0f, 6.0f, 6.0f);
    }

    auto titleArea = getLocalBounds().reduced (6).withTrimmedRight (bypassButtonSize + 2);
    if (rackHeader)
        titleArea = titleArea.withTrimmedLeft (expandButtonSize + 2);

    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (plugin->getName(), titleArea, juce::Justification::topLeft, true);

    if (! collapsed)
    {
        g.setFont (juce::FontOptions (9.0f));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        juce::String kind = instrument ? "Instrument" : "Audio Effect";
        if (loadState == EngineHelpers::PluginLoadState::failed)
            kind = "Load failed";
        else if (loadState == EngineHelpers::PluginLoadState::loading)
            kind = "Loading...";
        else if (rackInstance)
            kind = "Rack";
        else if (nestedInRack)
            kind = "In Rack";
        else if (EngineHelpers::isSandboxedExternalPlugin (*plugin))
            kind = "Sandboxed";
        g.drawText (kind, titleArea.withTrimmedTop (14), juce::Justification::topLeft, true);
    }

    if (rackHeader)
    {
        const auto expandArea = getExpandButtonArea().toFloat();
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        juce::Path chevron;
        if (rackExpanded)
        {
            chevron.addTriangle (expandArea.getX(), expandArea.getY() + 3.0f,
                                 expandArea.getRight(), expandArea.getY() + 3.0f,
                                 expandArea.getCentreX(), expandArea.getBottom() - 1.0f);
        }
        else
        {
            chevron.addTriangle (expandArea.getX() + 2.0f, expandArea.getY(),
                                 expandArea.getX() + 2.0f, expandArea.getBottom(),
                                 expandArea.getRight() - 1.0f, expandArea.getCentreY());
        }
        g.fillPath (chevron);
    }

    const auto bypassBounds = juce::Rectangle<float> ((float) getWidth() - bypassButtonSize - 4, 4.0f,
                                                      (float) bypassButtonSize, (float) bypassButtonSize);
    g.setColour (plugin->isEnabled() ? juce::Colours::limegreen.withAlpha (0.8f)
                                    : juce::Colours::grey.withAlpha (0.6f));
    g.fillEllipse (bypassBounds);
}

void PluginSlotComponent::showContextMenu()
{
    showPluginDeviceMenu (*this, editViewState, *plugin, stateManager, [this]
    {
        updateEnabledLook();
        if (onChanged)
            onChanged (*plugin);
    },
    [this] (te::Plugin& p)
    {
        if (onReplace)
            onReplace (p);
    },
    [this] (te::Plugin& p)
    {
        if (onBrowsePresets)
            onBrowsePresets (p);
        else
            PresetBrowserPanel::showForPlugin (p, this, [this] { if (onChanged) onChanged (*plugin); });
    });
}

void PluginSlotComponent::showWetDryDialog()
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
            drySlider.onValueChange = [this] { dryParam.setParameter ((float) (drySlider.getValue() / 100.0), juce::sendNotification); };
            wetSlider.onValueChange = [this] { wetParam.setParameter ((float) (wetSlider.getValue() / 100.0), juce::sendNotification); };
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

void showPluginDeviceMenu (PluginSlotComponent& slot,
                           EditViewState& editViewState,
                           te::Plugin& plugin,
                           PluginStateManager* stateManager,
                           std::function<void()> onChanged,
                           std::function<void (te::Plugin&)> onReplace,
                           std::function<void (te::Plugin&)> onBrowsePresets)
{
    enum MenuIds
    {
        bypass = 1, rename, duplicate, replace, copy, paste, moveLeft, moveRight, remove,
        wetDry = 100, soloDevice = 110, collapse = 115,
        expandRack = 116, showRackMacros = 117, configureOutputs = 118,
        moveToRackBase = 300,
        favorite = 400,
        savePreset = 500,
        loadPreset = 501,
        browsePresets = 502
    };

    const bool isRackInstance = dynamic_cast<te::RackInstance*> (&plugin) != nullptr;

    juce::PopupMenu menu;
    auto* trackPtr = te::getTrackContainingPlugin (plugin.edit, &plugin);
    menu.addItem (bypass, "Bypass", true, ! plugin.isEnabled());

    const bool soloed = trackPtr != nullptr && EngineHelpers::isPluginSoloed (*trackPtr, plugin);
    menu.addItem (soloDevice, soloed ? "Unsolo Device" : "Solo Device", true, soloed);
    menu.addItem (rename, "Rename...");
    menu.addItem (duplicate, "Duplicate");
    if (! isRackInstance)
        menu.addItem (replace, "Replace...");
    menu.addItem (copy, "Copy");
    menu.addItem (paste, "Paste", stateManager != nullptr && stateManager->hasClipboard());
    menu.addItem (collapse, slot.isCollapsed() ? "Expand" : "Collapse");

    if (auto* rack = dynamic_cast<te::RackInstance*> (&plugin))
    {
        menu.addItem (expandRack, slot.isRackExpanded() ? "Collapse Rack Chain" : "Expand Rack Chain");
        menu.addItem (showRackMacros, "Show Macro Knobs");
        juce::ignoreUnused (rack);
    }

    if (EngineHelpers::hasWetDryMix (plugin))
        menu.addItem (wetDry, "Wet/Dry Mix...");

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (trackPtr))
        if (MultiOutputRouting::isMultiOutputCapable (plugin))
            menu.addItem (configureOutputs, "Configure Outputs...");

    if (stateManager != nullptr)
    {
        const auto id = EngineHelpers::getPluginDescription (plugin).createIdentifierString();
        menu.addItem (favorite, stateManager->isFavorite (id) ? "Remove Favorite" : "Add Favorite");
    }

    menu.addItem (savePreset, "Save Preset...");
    menu.addItem (loadPreset, "Load Preset...");
    menu.addItem (browsePresets, "Browse Presets...");

    SidechainMenu::addSidechainMenuItems (menu, plugin);

    menu.addSeparator();
    menu.addItem (moveLeft, "Move Earlier");
    menu.addItem (moveRight, "Move Later");
    menu.addSeparator();
    menu.addItem (remove, "Remove");

    const auto safeSlot = juce::Component::SafePointer<PluginSlotComponent> (&slot);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slot),
                        [safeSlot, &editViewState, &plugin, stateManager, onChanged, onReplace, onBrowsePresets] (int result)
    {
        if (safeSlot == nullptr)
            return;

        auto* trackPtr2 = te::getTrackContainingPlugin (plugin.edit, &plugin);
        if (trackPtr2 == nullptr)
            return;

        auto& track = *trackPtr2;

        if (SidechainMenu::handleSidechainMenuResult (result, plugin, moveToRackBase,
                                                      [safeSlot, onChanged]
        {
            if (safeSlot != nullptr)
            {
                safeSlot->repaint();
                if (onChanged)
                    onChanged();
            }
        }))
            return;

        switch (result)
        {
            case bypass:
                plugin.setEnabled (! plugin.isEnabled());
                break;
            case rename:
            {
                auto w = std::make_shared<juce::AlertWindow> ("Rename Device", "Enter a new name:", juce::AlertWindow::QuestionIcon);
                w->addTextEditor ("name", plugin.getName());
                w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([w, &plugin] (int r)
                {
                    if (r == 1)
                        EngineHelpers::renamePlugin (plugin, w->getTextEditorContents ("name"));
                }));
                break;
            }
            case duplicate:
                if (auto* at = dynamic_cast<te::AudioTrack*> (&track))
                    EngineHelpers::duplicatePluginOnTrack (plugin, *at);
                break;
            case replace:
                if (onReplace)
                    onReplace (plugin);
                break;
            case copy:
                if (stateManager != nullptr)
                    stateManager->setClipboard (PluginPresetManager::capturePluginState (plugin),
                                              EngineHelpers::getPluginDescription (plugin));
                break;
            case paste:
                if (stateManager != nullptr && stateManager->hasClipboard())
                {
                    if (auto* at = dynamic_cast<te::AudioTrack*> (&track))
                    {
                        const auto desc = stateManager->getClipboardDescription();
                        if (auto newPlugin = EngineHelpers::createPluginFromDescription (plugin.edit, desc))
                        {
                            EngineHelpers::insertPluginOnTrack (*at, newPlugin);
                            PluginPresetManager::applyPluginState (*newPlugin, stateManager->getClipboardState());
                        }
                        else if (safeSlot != nullptr)
                        {
                            EngineHelpers::showPluginInsertFailureAlert (safeSlot.getComponent(), desc);
                        }
                    }
                }
                break;
            case wetDry:
                safeSlot->showWetDryDialog();
                break;
            case configureOutputs:
                if (auto* at = dynamic_cast<te::AudioTrack*> (&track))
                    MultiOutputRouting::showConfigureOutputsDialog (*at, plugin, safeSlot.getComponent());
                break;
            case soloDevice:
                if (EngineHelpers::isPluginSoloed (track, plugin))
                    EngineHelpers::clearSoloedPlugin (track);
                else
                    EngineHelpers::setSoloedPlugin (track, &plugin);
                break;
            case collapse:
                safeSlot->setCollapsed (! safeSlot->isCollapsed());
                break;
            case expandRack:
                if (auto* rack = dynamic_cast<te::RackInstance*> (&plugin))
                    if (safeSlot->onRackExpandChanged)
                        safeSlot->onRackExpandChanged (*rack, ! safeSlot->isRackExpanded());
                break;
            case showRackMacros:
                if (auto* rack = dynamic_cast<te::RackInstance*> (&plugin))
                    if (safeSlot->onShowRackMacros)
                        safeSlot->onShowRackMacros (*rack);
                break;
            case favorite:
                if (stateManager != nullptr)
                {
                    const auto id = EngineHelpers::getPluginDescription (plugin).createIdentifierString();
                    if (stateManager->isFavorite (id))
                        stateManager->removeFavorite (id);
                    else
                        stateManager->addFavorite (id);
                }
                break;
            case savePreset:
            {
                auto fc = std::make_shared<juce::FileChooser> ("Save Preset", juce::File(), "*.xml");
                fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                 [fc, &plugin] (const juce::FileChooser&)
                                 {
                                     const auto f = fc->getResult();
                                     if (f != juce::File())
                                         PluginPresetManager::savePresetToFile (plugin, f);
                                 });
                break;
            }
            case loadPreset:
            {
                auto fc = std::make_shared<juce::FileChooser> ("Load Preset", juce::File(), "*.xml");
                fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                 [fc, &plugin] (const juce::FileChooser&)
                                 {
                                     const auto f = fc->getResult();
                                     if (f.existsAsFile())
                                         PluginPresetManager::loadPresetFromFile (plugin, f);
                                 });
                break;
            }
            case browsePresets:
                if (onBrowsePresets)
                    onBrowsePresets (plugin);
                break;
            case moveLeft:
                if (safeSlot->onMove) safeSlot->onMove (plugin, -1);
                break;
            case moveRight:
                if (safeSlot->onMove) safeSlot->onMove (plugin, 1);
                break;
            case remove:
                if (EngineHelpers::isPluginSoloed (track, plugin))
                    EngineHelpers::clearSoloedPlugin (track);
                if (safeSlot->onRemove) safeSlot->onRemove (plugin);
                break;
            default:
                break;
        }

        if (onChanged)
            onChanged();
    });
}

} // namespace skeletonhive
