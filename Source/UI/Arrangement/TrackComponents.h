#pragma once

#include "ClipComponents.h"
#include "TakeLaneComponent.h"
#include "Engine/EngineHelpers.h"
#include "Engine/GroovePoolManager.h"
#include "Engine/PluginDragManager.h"
#include "Engine/ContentDragManager.h"
#include <functional>

namespace skeletonhive
{

class UiTelemetryHub;

class TrackLaneComponent;

void showTimelineContextMenu (juce::Component& target,
                              juce::Point<int> screenPosition,
                              EditViewState& editViewState,
                              te::Track* track,
                              bool offerCreateMidiClip,
                              std::function<void()> onCreateMidiClip,
                              te::Clip* contextClip = nullptr,
                              std::function<void()> onShowClipProperties = nullptr,
                              std::function<void()> onTakeLanesChanged = nullptr,
                              std::function<void()> onClipsChanged = nullptr,
                              std::function<void (te::Clip&)> onExportToLibrary = nullptr,
                              GroovePoolManager* groovePool = nullptr);

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
                             private te::ValueTreeAllEventListener,
                             public juce::DragAndDropTarget
{
public:
    TrackHeaderComponent (EditViewState& evs, te::Track::Ptr t);
    ~TrackHeaderComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void resized() override;

    std::function<void (te::Track&)> onArmChanged;
    std::function<void (te::Track&)> onMuteChanged;
    std::function<void (te::Track&)> onSoloChanged;
    std::function<void (te::Track&)> onTrackSelected;

    te::EditItemID getTrackId() const { return track != nullptr ? track->itemID : te::EditItemID(); }

    // juce::DragAndDropTarget
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDragEnter (const SourceDetails& dragSourceDetails) override;
    void itemDragMove (const SourceDetails& dragSourceDetails) override;
    void itemDragExit (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

private:
    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { updateKindBadge(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { updateKindBadge(); }

    void updateKindBadge();
    void showHeaderContextMenu (juce::Point<int> screenPosition);
    void showInputSelectionMenu();
    EngineHelpers::TrackDropZone dropZoneForPosition (juce::Point<int> localPos) const;
    void moveSelectedTracksToDropZone (EngineHelpers::TrackDropZone zone);

    EditViewState& editViewState;
    te::Track::Ptr track;
    juce::Label trackName;
    juce::Label kindBadge;
    juce::TextButton armButton { "R" }, muteButton { "M" }, soloButton { "S" };

    EngineHelpers::TrackDropZone dropHighlightZone = EngineHelpers::TrackDropZone::below;
    bool dropHighlightActive = false;
    bool dragStarted = false;
};

class PluginSlotButton : public juce::TextButton,
                         private juce::Timer
{
public:
    PluginSlotButton (EditViewState& evs, te::Plugin::Ptr p);
    ~PluginSlotButton() override;
    te::Plugin::Ptr getPlugin() { return plugin; }

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    std::function<void (te::Plugin&)> onRemove;
    std::function<void (te::Plugin&, int direction)> onMove;
    std::function<void (te::Plugin&, int targetSlotIndex)> onDropAtSlot;

private:
    void timerCallback() override;
    void showSlotMenu();
    void showWetDryDialog();
    void updateEnabledLook();
    void refreshLoadState();

    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
    EngineHelpers::PluginLoadState loadState = EngineHelpers::PluginLoadState::ok;
    juce::String loadStatusMessage;
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
                           public juce::DragAndDropTarget,
                           public juce::FileDragAndDropTarget
{
public:
    TrackLaneComponent (EditViewState& evs, te::Track::Ptr t);
    ~TrackLaneComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void resized() override;

    void clearRangeSelection();
    bool hasRangeSelection() const { return rangeSelectionActive; }

    /** Re-applies clip bounds and visibility culling for the current view. */
    void refreshLayout();

    te::Track& getTrack() { return *track; }

    std::function<void (te::Clip&)> onClipDoubleClick;
    std::function<void()> onClipSelectionChanged;
    std::function<void()> onShowClipProperties;
    std::function<void (te::Clip&, const juce::MouseEvent&)> onClipCrossTrackDragMove;
    std::function<void (te::Clip&, const juce::MouseEvent&)> onClipCrossTrackDragEnd;
    std::function<void()> onTakeLanesChanged;
    std::function<te::Plugin::Ptr (const juce::PluginDescription& desc)> createPlugin;
    std::function<void (te::Track&)> onAddPlugin;
    std::function<void (const juce::File&, te::Clip*)> onSampleInserted;
    std::function<void (te::Clip&)> onExportClipToLibrary;
    std::function<te::Clip* (const juce::File& presetFile, int localX)> onClipPresetDropped;

    GroovePoolManager* groovePool = nullptr;

    // juce::DragAndDropTarget — browser drop onto lane appends to chain or inserts sample
    bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override;
    void itemDropped (const SourceDetails& dragSourceDetails) override;

    // juce::FileDragAndDropTarget — OS file drops onto lane
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markAndUpdate (updateClips); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markAndUpdate (updateClips); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { markAndUpdate (updatePositions); }
    void handleAsyncUpdate() override;

    void buildClips();
    void updateClipBounds();
    void updateTakeLaneStack();
    int getClipAreaHeight() const;
    bool canDragCreateClips() const;
    te::TimeRange getRangeSelection() const;
    void createMidiClipFromRangeSelection();
    te::Clip* insertSampleAtX (const juce::File& file, int localX);
    te::Clip* insertClipPresetAtX (const juce::File& presetFile, int localX);
    bool isSupportedAudioFile (const juce::File& file) const;
    void showLaneContextMenu (const juce::MouseEvent& e);
    void paintRangeSelection (juce::Graphics& g, te::TimePosition start, te::TimePosition end) const;
    te::Clip* findClipAtX (int x) const;
    bool isLaneLevelRendering() const;
    void placePlayheadAtX (int x);

    EditViewState& editViewState;
    te::Track::Ptr track;
    juce::OwnedArray<ClipComponent> clips;
    bool updateClips = false, updatePositions = false;

    bool dragCreateActive = false;
    bool pendingTimelineInteraction = false;
    bool rangeSelectionActive = false;
    juce::Point<int> pendingDragStartPos;
    te::TimePosition dragCreateAnchor;
    te::TimePosition dragCreateCurrent;
    te::TimePosition rangeSelectionStart;
    te::TimePosition rangeSelectionEnd;

    std::unique_ptr<TakeLaneStack> takeLaneStack;

    static constexpr int timelineClickDragThresholdPx = 4;
};

class PlayheadOverlay : public juce::Component
{
public:
    PlayheadOverlay (te::Edit& edit, EditViewState& evs, UiTelemetryHub* telemetryHub = nullptr);
    ~PlayheadOverlay() override;

    void paint (juce::Graphics& g) override;
    void updateFromTransport();

private:
    te::Edit& edit;
    EditViewState& editViewState;
    UiTelemetryHub* telemetryHub = nullptr;
    int xPosition = 0;
};

} // namespace skeletonhive
