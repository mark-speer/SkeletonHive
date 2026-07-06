#pragma once

#include "UI/Arrangement/EditViewState.h"
#include "Engine/PluginStateManager.h"

namespace skeletonhive
{

class PluginTrayComponent;

/** Single device slot in the plugin tray (Ableton-style device block). */
class PluginSlotComponent : public juce::Component
{
public:
    PluginSlotComponent (EditViewState& evs,
                         te::Plugin::Ptr plugin,
                         PluginStateManager* stateManager = nullptr);

    te::Plugin::Ptr getPlugin() const { return plugin; }

    void setSelected (bool selected);
    void setCollapsed (bool collapsed);
    bool isCollapsed() const { return collapsed; }

    void setNestedInRack (bool nested);
    bool isNestedInRack() const { return nestedInRack; }

    void setRackHeader (bool header, bool expanded);
    bool isRackHeader() const { return rackHeader; }
    bool isRackExpanded() const { return rackExpanded; }

    void setRackContext (te::RackInstance* rack) { rackContext = rack; }
    te::RackInstance* getRackContext() const { return rackContext; }

    void showWetDryDialog();

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void paint (juce::Graphics& g) override;

    std::function<void (te::Plugin&)> onRemove;
    std::function<void (te::Plugin&, int direction)> onMove;
    std::function<void (te::Plugin&)> onChanged;
    std::function<void (te::RackInstance&, bool expanded)> onRackExpandChanged;
    std::function<void (te::RackInstance&)> onShowRackMacros;

    static constexpr int defaultWidth = 128;
    static constexpr int collapsedWidth = 72;
    static constexpr int nestedWidth = 112;

private:
    void showContextMenu();
    void updateEnabledLook();
    void openEditor();
    juce::Rectangle<int> getExpandButtonArea() const;

    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
    PluginStateManager* stateManager = nullptr;
    te::RackInstance* rackContext = nullptr;
    bool selected = false;
    bool collapsed = false;
    bool nestedInRack = false;
    bool rackHeader = false;
    bool rackExpanded = false;
};

void showPluginDeviceMenu (PluginSlotComponent& slot,
                           EditViewState& editViewState,
                           te::Plugin& plugin,
                           PluginStateManager* stateManager,
                           std::function<void()> onChanged);

} // namespace skeletonhive
