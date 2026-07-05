#pragma once

#include "ClipComponents.h"
#include <functional>

namespace arrange
{

namespace PluginDragTypes
{
inline constexpr const char* slotReorder = "arrangePluginSlot";
inline constexpr const char* browserInsert = "arrangePluginBrowser";
} // namespace PluginDragTypes

class TrackLaneComponent;

void showTimelineContextMenu (juce::Component& target,
                              juce::Point<int> screenPosition,
                              EditViewState& editViewState,
                              te::Track* track,
                              bool offerCreateMidiClip,
                              std::function<void()> onCreateMidiClip);

class FlaggedAsyncUpdater : public juce::AsyncUpdater
{
public:
    void markAndUpdate (bool& flag) { flag = true; triggerAsyncUpdate(); }
    bool compareAndReset (bool& flag) noexcept
    {
        if (! flag) return false;
        flag = false;
        return true;
    }
};

class TrackHeaderComponent : public juce::Component,
                             private te::ValueTreeAllEventListener
{
public:
    TrackHeaderComponent (EditViewState& evs, te::Track::Ptr t);
    ~TrackHeaderComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    std::function<void (te::Track&)> onArmChanged;
    std::function<void (te::Track&)> onMuteChanged;
    std::function<void (te::Track&)> onSoloChanged;

private:
    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { updateKindBadge(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { updateKindBadge(); }

    void updateKindBadge();

    EditViewState& editViewState;
    te::Track::Ptr track;
    juce::Label trackName;
    juce::Label kindBadge;
    juce::TextButton armButton { "R" }, muteButton { "M" }, soloButton { "S" };
};

class PluginSlotButton : public juce::TextButton
{
public:
    PluginSlotButton (EditViewState& evs, te::Plugin::Ptr p);
    te::Plugin::Ptr getPlugin() { return plugin; }

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    std::function<void (te::Plugin&)> onRemove;
    std::function<void (te::Plugin&, int direction)> onMove;
    std::function<void (te::Plugin&, int targetSlotIndex)> onDropAtSlot;

private:
    void showSlotMenu();
    void showWetDryDialog();
    void updateEnabledLook();

    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
};

class RackMacroPanel : public juce::Component
{
public:
    RackMacroPanel (EditViewState& evs, te::RackInstance& rack);
    void resized() override;

private:
    EditViewState& editViewState;
    te::RackInstance::Ptr rack;
    juce::OwnedArray<juce::Slider> macroSliders;
    juce::OwnedArray<juce::Label> macroLabels;
};

class TrackFooterComponent : public juce::Component,
                             private FlaggedAsyncUpdater,
                             private te::ValueTreeAllEventListener,
                             public juce::DragAndDropTarget
{
public:
    TrackFooterComponent (EditViewState& evs, te::Track::Ptr t);
    ~TrackFooterComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    std::function<void (te::Track&)> onAddPlugin;
    std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> createPlugin;

    void setExpandedRack (te::RackInstance* rack);

    // juce::DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragMove (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markAndUpdate (updatePlugins); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markAndUpdate (updatePlugins); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { markAndUpdate (updatePlugins); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void handleAsyncUpdate() override;

    void buildPlugins();
    void movePlugin (te::Plugin& plugin, int direction);
    void movePluginToSlot (te::Plugin& plugin, int targetSlotIndex);
    void removePlugin (te::Plugin& plugin);
    void insertBrowserPlugin (const juce::PluginDescription& desc, int slotIndex);
    int slotIndexAtX (int x) const;
    void showFooterContextMenu (const juce::MouseEvent& e);
    void groupSelectedIntoRack();

    EditViewState& editViewState;
    te::Track::Ptr track;
    juce::TextButton addButton { "+" };
    juce::OwnedArray<PluginSlotButton> plugins;
    int dropHighlightSlot = -1;
    bool updatePlugins = false;
};

class TrackLaneComponent : public juce::Component,
                           private te::ValueTreeAllEventListener,
                           private FlaggedAsyncUpdater,
                           public juce::DragAndDropTarget
{
public:
    TrackLaneComponent (EditViewState& evs, te::Track::Ptr t);
    ~TrackLaneComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void resized() override;

    void clearRangeSelection();
    bool hasRangeSelection() const { return rangeSelectionActive; }

    /** Re-applies clip bounds and visibility culling for the current view. */
    void refreshLayout();

    std::function<void (te::Clip&)> onClipDoubleClick;
    std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> createPlugin;
    std::function<void (te::Track&)> onAddPlugin;

    // juce::DragAndDropTarget — browser drop onto lane appends to chain
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markAndUpdate (updateClips); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markAndUpdate (updateClips); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { markAndUpdate (updatePositions); }
    void handleAsyncUpdate() override;

    void buildClips();
    void updateClipBounds();
    bool canDragCreateClips() const;
    te::TimeRange getRangeSelection() const;
    void createMidiClipFromRangeSelection();
    void showLaneContextMenu (const juce::MouseEvent& e);
    void paintRangeSelection (juce::Graphics& g, te::TimePosition start, te::TimePosition end) const;

    EditViewState& editViewState;
    te::Track::Ptr track;
    juce::OwnedArray<ClipComponent> clips;
    bool updateClips = false, updatePositions = false;

    bool dragCreateActive = false;
    bool rangeSelectionActive = false;
    te::TimePosition dragCreateAnchor;
    te::TimePosition dragCreateCurrent;
    te::TimePosition rangeSelectionStart;
    te::TimePosition rangeSelectionEnd;
};

class PlayheadOverlay : public juce::Component,
                        private juce::Timer
{
public:
    PlayheadOverlay (te::Edit& edit, EditViewState& evs);

    void paint (juce::Graphics& g) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    void timerCallback() override;

    te::Edit& edit;
    EditViewState& editViewState;
    int xPosition = 0;
};

} // namespace arrange
