#include "PluginTrayComponent.h"
#include "Engine/AppCommands.h"
#include "Engine/PluginPresetManager.h"
#include "Engine/EngineHelpers.h"
#include "Engine/PluginDragManager.h"
#include "UI/AppLookAndFeel.h"
#include "UI/Arrangement/TrackComponents.h"

namespace skeletonhive
{

PluginTrayComponent::PluginTrayComponent (EditViewState& evs, PluginStateManager& sm)
    : editViewState (evs), pluginStateManager (sm)
{
    trackTitle.setJustificationType (juce::Justification::centredLeft);
    trackTitle.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    trackTitle.setColour (juce::Label::textColourId, juce::Colours::white);

    outputLabel.setJustificationType (juce::Justification::centred);
    outputLabel.setColour (juce::Label::backgroundColourId, AppColours::pluginOutputNode (AppLookAndFeel::getCurrentTheme()));
    outputLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f));

    addButton.onClick = [this]
    {
        if (currentTrack != nullptr && onAddPlugin)
            onAddPlugin (*currentTrack);
    };

    chainViewport.setViewedComponent (&chainContent, false);
    chainViewport.setScrollBarsShown (false, true);

    addAndMakeVisible (trackTitle);
    addAndMakeVisible (addButton);
    addAndMakeVisible (chainViewport);
    addAndMakeVisible (outputLabel);
}

PluginTrayComponent::~PluginTrayComponent()
{
    if (currentTrack != nullptr)
        currentTrack->pluginList.state.removeListener (this);

    for (auto* rackType : listenedRackTypes)
        if (rackType != nullptr)
            rackType->state.removeListener (this);
}

void PluginTrayComponent::setTrack (te::Track* track)
{
    if (currentTrack != nullptr)
        currentTrack->pluginList.state.removeListener (this);

    for (auto* rackType : listenedRackTypes)
        if (rackType != nullptr)
            rackType->state.removeListener (this);
    listenedRackTypes.clear();

    currentTrack = track;
    chainModel.reset();
    expandedRackIds.clear();

    if (track != nullptr)
    {
        if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (track))
        {
            chainModel = std::make_unique<TrackPluginChainModel> (*audioTrack);
            track->pluginList.state.addListener (this);
        }

        trackTitle.setText (track->getName(), juce::dontSendNotification);
    }
    else
    {
        trackTitle.setText ("No track selected", juce::dontSendNotification);
    }

    markDirty();
}

void PluginTrayComponent::handleAsyncUpdate()
{
    if (needsRebuild)
    {
        needsRebuild = false;
        rebuildSlots();
    }

    syncSelectionHighlight();
}

bool PluginTrayComponent::isRackExpanded (const te::RackInstance& rack) const
{
    return expandedRackIds.contains (rack.itemID);
}

void PluginTrayComponent::setRackExpanded (te::RackInstance& rack, bool expanded)
{
    if (expanded)
    {
        if (! expandedRackIds.contains (rack.itemID))
            expandedRackIds.add (rack.itemID);
    }
    else
    {
        expandedRackIds.removeAllInstancesOf (rack.itemID);
    }

    markDirty();
}

void PluginTrayComponent::updateRackListeners()
{
    for (auto* rackType : listenedRackTypes)
        if (rackType != nullptr)
            rackType->state.removeListener (this);

    listenedRackTypes.clear();

    if (chainModel == nullptr)
        return;

    for (auto* plugin : chainModel->getUserChainPlugins())
    {
        if (auto* rack = dynamic_cast<te::RackInstance*> (plugin))
        {
            if (isRackExpanded (*rack) && rack->type != nullptr
                && ! listenedRackTypes.contains (rack->type.get()))
            {
                rack->type->state.addListener (this);
                listenedRackTypes.add (rack->type.get());
            }
        }
    }
}

void PluginTrayComponent::buildLayoutEntries()
{
    layoutEntries.clear();

    if (chainModel == nullptr)
        return;

    for (auto* plugin : chainModel->getUserChainPlugins())
    {
        if (auto* rack = dynamic_cast<te::RackInstance*> (plugin))
        {
            TrayLayoutEntry header;
            header.plugin = plugin;
            header.rackHeader = rack;
            layoutEntries.add (header);

            if (isRackExpanded (*rack))
            {
                for (auto* inner : EngineHelpers::getRackInternalPlugins (*rack))
                {
                    TrayLayoutEntry nested;
                    nested.plugin = inner;
                    nested.rackContext = rack;
                    layoutEntries.add (nested);
                }
            }
        }
        else
        {
            TrayLayoutEntry entry;
            entry.plugin = plugin;
            layoutEntries.add (entry);
        }
    }
}

void PluginTrayComponent::rebuildSlots()
{
    slots.clear();
    chainContent.removeAllChildren();
    buildLayoutEntries();
    updateRackListeners();

    if (chainModel == nullptr)
    {
        chainContent.setSize (0, trayHeight - 8);
        resized();
        return;
    }

    for (int i = 0; i < layoutEntries.size(); ++i)
    {
        const auto& entry = layoutEntries.getReference (i);
        te::Plugin::Ptr ptr (entry.plugin);
        auto* slot = new PluginSlotComponent (editViewState, ptr, &pluginStateManager);

        if (entry.rackContext != nullptr)
        {
            slot->setNestedInRack (true);
            slot->setRackContext (entry.rackContext);
        }

        if (entry.rackHeader != nullptr)
        {
            slot->setRackHeader (true, isRackExpanded (*entry.rackHeader));
            slot->onRackExpandChanged = [this] (te::RackInstance& rack, bool expanded)
            {
                setRackExpanded (rack, expanded);
            };
            slot->onShowRackMacros = [this] (te::RackInstance& rack) { showRackMacros (rack); };
        }

        slot->onRemove = [this] (te::Plugin& p) { removePlugin (p); };

        slot->onMove = [this, entry] (te::Plugin& p, int dir)
        {
            if (entry.rackContext != nullptr)
            {
                const int slotIndex = EngineHelpers::rackSlotForPlugin (*entry.rackContext, p);
                moveRackPluginToSlot (*entry.rackContext, p, slotIndex + dir);
            }
            else
            {
                movePlugin (p, dir);
            }
        };

        slot->onChanged = [this] (te::Plugin&)
        {
            syncSelectionHighlight();
            layoutSlots();
        };

        slots.add (slot);
        chainContent.addAndMakeVisible (slot);
    }

    layoutSlots();
}

int PluginTrayComponent::slotWidthForIndex (int index) const
{
    if (! juce::isPositiveAndBelow (index, slots.size()))
        return PluginSlotComponent::defaultWidth;

    auto* slot = slots[index];
    if (slot->isCollapsed())
        return PluginSlotComponent::collapsedWidth;

    if (slot->isNestedInRack())
        return PluginSlotComponent::nestedWidth;

    return PluginSlotComponent::defaultWidth;
}

void PluginTrayComponent::layoutSlots()
{
    int x = 0;
    const int slotHeight = trayHeight - 12;

    for (int i = 0; i < slots.size(); ++i)
    {
        const int w = slotWidthForIndex (i);
        slots[i]->setBounds (x, 4, w, slotHeight);
        x += w + slotGap;
    }

    chainContent.setSize (juce::jmax (x, 1), trayHeight - 8);
    resized();
}

void PluginTrayComponent::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto header = r.removeFromTop (22);
    trackTitle.setBounds (header.removeFromLeft (juce::jmax (120, getWidth() / 3)));
    addButton.setBounds (header.removeFromLeft (28).reduced (1));

    outputLabel.setBounds (r.removeFromRight (72).reduced (0, 4));
    chainViewport.setBounds (r);
}

void PluginTrayComponent::paint (juce::Graphics& g)
{
    g.fillAll (AppColours::pluginTrayBackground (AppLookAndFeel::getCurrentTheme()));
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());

    if (dropHighlightSlot >= 0 && chainModel != nullptr)
    {
        int x = 0;
        for (int i = 0; i < dropHighlightSlot && i < slots.size(); ++i)
            x += slotWidthForIndex (i) + slotGap;

        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillRect (chainViewport.getX() + x + 4, chainViewport.getY() + 4, 3, chainViewport.getHeight() - 8);
    }
}

int PluginTrayComponent::slotIndexAtX (int x) const
{
    int pos = 0;
    for (int i = 0; i < slots.size(); ++i)
    {
        const int w = slotWidthForIndex (i);
        if (x < pos + w / 2)
            return i;
        pos += w + slotGap;
    }
    return slots.size();
}

int PluginTrayComponent::userSlotAtFlatIndex (int flatIndex) const
{
    int userSlot = 0;
    for (int i = 0; i < flatIndex && i < layoutEntries.size(); ++i)
    {
        const auto& entry = layoutEntries.getReference (i);
        if (entry.rackContext == nullptr)
            ++userSlot;
    }
    return userSlot;
}

int PluginTrayComponent::rackInternalSlotAtFlatIndex (int flatIndex, te::RackInstance& rack) const
{
    int internalSlot = 0;
    bool inSection = false;

    for (int i = 0; i < flatIndex && i < layoutEntries.size(); ++i)
    {
        const auto& entry = layoutEntries.getReference (i);

        if (entry.rackHeader == &rack)
        {
            inSection = true;
            continue;
        }

        if (inSection)
        {
            if (entry.rackContext == &rack)
                ++internalSlot;
            else
                break;
        }
    }

    return internalSlot;
}

bool PluginTrayComponent::isInterestedInDragSource (const SourceDetails& details)
{
    const auto payload = PluginDragPayload::parse (details.description);
    return payload.kind != PluginDragPayload::Kind::unknown && chainModel != nullptr;
}

void PluginTrayComponent::itemDragEnter (const SourceDetails&)
{
    dropHighlightSlot = -1;
    repaint();
}

void PluginTrayComponent::itemDragMove (const SourceDetails& details)
{
    const int slot = slotIndexAtX (details.localPosition.x);
    if (slot != dropHighlightSlot)
    {
        dropHighlightSlot = slot;
        repaint();
    }
}

void PluginTrayComponent::itemDragExit (const SourceDetails&)
{
    dropHighlightSlot = -1;
    repaint();
}

void PluginTrayComponent::itemDropped (const SourceDetails& details)
{
    dropHighlightSlot = -1;
    repaint();

    if (chainModel == nullptr)
        return;

    const auto payload = PluginDragPayload::parse (details.description);
    const int slot = slotIndexAtX (details.localPosition.x);

    if (payload.kind == PluginDragPayload::Kind::browserInsert)
    {
        insertBrowserPlugin (EngineHelpers::lookupKnownPlugin (editViewState.edit.engine,
                                                               payload.pluginIdentifier),
                             userSlotAtFlatIndex (slot));
    }
    else if (payload.kind == PluginDragPayload::Kind::slotReorder)
    {
        handleSlotDrop (payload, slot);
    }
    else if (payload.kind == PluginDragPayload::Kind::crossTrack)
    {
        handleCrossTrackDrop (payload, userSlotAtFlatIndex (slot));
    }
}

void PluginTrayComponent::handleSlotDrop (const PluginDragPayload& payload, int flatSlotIndex)
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (currentTrack.get()))
    {
        if (payload.rackInstanceId.isValid())
        {
            if (auto* rack = EngineHelpers::findRackOnTrack (*audioTrack, payload.rackInstanceId))
            {
                for (auto* plugin : EngineHelpers::getRackInternalPlugins (*rack))
                {
                    if (plugin->itemID == payload.pluginId)
                    {
                        const int target = rackInternalSlotAtFlatIndex (flatSlotIndex, *rack);
                        moveRackPluginToSlot (*rack, *plugin, target);
                        return;
                    }
                }
            }
        }
        else
        {
            for (auto* plugin : chainModel->getUserChainPlugins())
            {
                if (plugin->itemID == payload.pluginId)
                {
                    movePluginToSlot (*plugin, userSlotAtFlatIndex (flatSlotIndex));
                    return;
                }
            }
        }
    }
}

void PluginTrayComponent::handleCrossTrackDrop (const PluginDragPayload& payload, int slotIndex)
{
    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (currentTrack.get()))
    {
        for (auto t : te::getAudioTracks (editViewState.edit))
        {
            if (t->itemID != payload.sourceTrackId)
                continue;

            for (auto p : t->pluginList)
            {
                if (p->itemID == payload.pluginId)
                {
                    EngineHelpers::movePluginToTrack (*p, *audioTrack, slotIndex);
                    markDirty();
                    return;
                }
            }
        }
    }
}

void PluginTrayComponent::insertBrowserPlugin (const juce::PluginDescription& desc, int slotIndex)
{
    if (createPlugin == nullptr || desc.name.isEmpty() || chainModel == nullptr)
        return;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (currentTrack.get()))
    {
        if (auto plugin = createPlugin (desc))
        {
            const int insertIndex = chainModel->resolveInsertIndex (slotIndex,
                                                                    EngineHelpers::isInstrumentDescription (desc),
                                                                    nullptr);
            if (insertIndex >= 0)
            {
                EngineHelpers::insertPluginOnTrack (*audioTrack, plugin, insertIndex);
                pluginStateManager.recordRecentUse (desc.createIdentifierString());
            }
        }
        else
        {
            EngineHelpers::showPluginInsertFailureAlert (this, desc);
        }
    }
}

void PluginTrayComponent::removePlugin (te::Plugin& plugin)
{
    plugin.deleteFromParent();
}

void PluginTrayComponent::movePlugin (te::Plugin& plugin, int direction)
{
    if (chainModel == nullptr)
        return;

    const int slot = chainModel->userSlotForPlugin (plugin);
    movePluginToSlot (plugin, slot + direction);
}

void PluginTrayComponent::movePluginToSlot (te::Plugin& plugin, int targetSlotIndex)
{
    if (chainModel == nullptr)
        return;

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (currentTrack.get()))
        EngineHelpers::movePluginToUserSlot (*audioTrack, plugin, targetSlotIndex);
}

void PluginTrayComponent::moveRackPluginToSlot (te::RackInstance& rack, te::Plugin& plugin, int targetSlotIndex)
{
    EngineHelpers::movePluginInRack (rack, plugin, targetSlotIndex);
}

void PluginTrayComponent::showRackMacros (te::RackInstance& rack)
{
    auto panel = std::make_unique<RackMacroPanel> (editViewState, rack);
    juce::CallOutBox::launchAsynchronously (std::move (panel),
                                            localAreaToGlobal (getScreenBounds()),
                                            nullptr);
}

void PluginTrayComponent::syncSelectionHighlight()
{
    const auto selected = editViewState.selectionManager.getItemsOfType<te::Plugin>();
    for (auto* slot : slots)
        slot->setSelected (selected.contains (slot->getPlugin().get()));
}

bool PluginTrayComponent::performCommand (int commandID)
{
    using namespace AppCommandIDs;

    if (chainModel == nullptr)
        return false;

    const auto selected = editViewState.selectionManager.getItemsOfType<te::Plugin>();
    if (selected.isEmpty())
        return false;

    switch (commandID)
    {
        case pluginDelete:
            for (auto* p : selected)
                removePlugin (*p);
            return true;

        case pluginDuplicate:
            if (auto* at = dynamic_cast<te::AudioTrack*> (currentTrack.get()))
                for (auto* p : selected)
                    EngineHelpers::duplicatePluginOnTrack (*p, *at);
            return true;

        case pluginCopy:
            pluginStateManager.setClipboard (PluginPresetManager::capturePluginState (*selected.getFirst()),
                                             EngineHelpers::getPluginDescription (*selected.getFirst()));
            return true;

        case pluginPaste:
            if (pluginStateManager.hasClipboard())
            {
                const auto desc = pluginStateManager.getClipboardDescription();
                insertBrowserPlugin (desc, chainModel->getUserChainSize());
            }
            return true;

        default:
            break;
    }

    return false;
}

} // namespace skeletonhive
