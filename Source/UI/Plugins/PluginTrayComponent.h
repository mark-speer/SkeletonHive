#pragma once

#include "UI/Arrangement/EditViewState.h"
#include "Engine/PluginStateManager.h"
#include "Engine/PluginDragManager.h"
#include "Engine/PluginScanner.h"
#include "Engine/TrackPluginChainModel.h"
#include "PluginSlotComponent.h"

namespace skeletonhive
{

/** Bottom-panel device chain for the selected track (Ableton-style workflow). */
class PluginTrayComponent : public juce::Component,
                            private juce::AsyncUpdater,
                            private te::ValueTreeAllEventListener,
                            public juce::DragAndDropTarget
{
public:
    PluginTrayComponent (EditViewState& evs, PluginStateManager& stateManager, PluginScanner& scanner);
    ~PluginTrayComponent() override;

    void setTrack (te::Track* track);
    te::Track* getTrack() const { return currentTrack.get(); }

    void setCreatePlugin (std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> fn)
    {
        createPlugin = std::move (fn);
    }

    void setOnAddPlugin (std::function<void (te::Track&)> fn) { onAddPlugin = std::move (fn); }

    bool performCommand (int commandID);
    void resized() override;
    void paint (juce::Graphics& g) override;

    // juce::DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragMove (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    struct TrayLayoutEntry
    {
        te::Plugin* plugin = nullptr;
        te::RackInstance* rackHeader = nullptr;
        te::RackInstance* rackContext = nullptr;
    };

    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markDirty(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markDirty(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { markDirty(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { markDirty(); }

    void handleAsyncUpdate() override;
    void markDirty() { needsRebuild = true; triggerAsyncUpdate(); }

    void rebuildSlots();
    void buildLayoutEntries();
    void layoutSlots();
    int slotWidthForIndex (int index) const;
    int slotIndexAtX (int x) const;
    int userSlotAtFlatIndex (int flatIndex) const;
    int rackInternalSlotAtFlatIndex (int flatIndex, te::RackInstance& rack) const;
    bool isRackExpanded (const te::RackInstance& rack) const;
    void setRackExpanded (te::RackInstance& rack, bool expanded);
    void updateRackListeners();
    void removePlugin (te::Plugin& plugin);
    void movePlugin (te::Plugin& plugin, int direction);
    void movePluginToSlot (te::Plugin& plugin, int targetSlot);
    void moveRackPluginToSlot (te::RackInstance& rack, te::Plugin& plugin, int targetSlot);
    void insertBrowserPlugin (const juce::PluginDescription& desc, int slotIndex);
    void handleCrossTrackDrop (const PluginDragPayload& payload, int slotIndex);
    void handleSlotDrop (const PluginDragPayload& payload, int flatSlotIndex);
    void syncSelectionHighlight();
    void showRackMacros (te::RackInstance& rack);
    void showReplacePicker (te::Plugin& plugin, te::RackInstance* rackContext);
    void showPresetBrowser (te::Plugin& plugin, PluginSlotComponent* slot);

    EditViewState& editViewState;
    PluginStateManager& pluginStateManager;
    PluginScanner& pluginScanner;
    te::Track::Ptr currentTrack;
    std::unique_ptr<TrackPluginChainModel> chainModel;

    juce::Viewport chainViewport;
    juce::Component chainContent;
    juce::TextButton addButton { "+" };
    juce::Label trackTitle;
    juce::Label outputLabel { {}, "Output" };
    juce::OwnedArray<PluginSlotComponent> slots;
    juce::Array<TrayLayoutEntry> layoutEntries;
    juce::Array<te::EditItemID> expandedRackIds;
    juce::Array<te::RackType*> listenedRackTypes;

    int dropHighlightSlot = -1;
    bool needsRebuild = false;

    std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> createPlugin;
    std::function<void (te::Track&)> onAddPlugin;

    static constexpr int slotGap = 4;
    static constexpr int trayHeight = 148;
};

} // namespace skeletonhive
