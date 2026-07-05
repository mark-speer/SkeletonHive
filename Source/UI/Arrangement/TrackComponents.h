#pragma once

#include "ClipComponents.h"
#include <functional>

namespace arrange
{

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

    std::function<void (te::Plugin&)> onRemove;
    std::function<void (te::Plugin&, int direction)> onMove;

private:
    void showSlotMenu();
    void updateEnabledLook();

    EditViewState& editViewState;
    te::Plugin::Ptr plugin;
};

class TrackFooterComponent : public juce::Component,
                             private FlaggedAsyncUpdater,
                             private te::ValueTreeAllEventListener
{
public:
    TrackFooterComponent (EditViewState& evs, te::Track::Ptr t);
    ~TrackFooterComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    std::function<void (te::Track&)> onAddPlugin;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { markAndUpdate (updatePlugins); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { markAndUpdate (updatePlugins); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { markAndUpdate (updatePlugins); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void handleAsyncUpdate() override;

    void buildPlugins();
    void movePlugin (te::Plugin& plugin, int direction);
    void removePlugin (te::Plugin& plugin);

    EditViewState& editViewState;
    te::Track::Ptr track;
    juce::TextButton addButton { "+" };
    juce::OwnedArray<PluginSlotButton> plugins;
    bool updatePlugins = false;
};

class TrackLaneComponent : public juce::Component,
                           private te::ValueTreeAllEventListener,
                           private FlaggedAsyncUpdater
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
