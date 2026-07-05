#pragma once

#include "EditViewState.h"

namespace arrange
{

void drawMidiClipPreview (juce::Graphics& g, te::MidiClip& clip, juce::Rectangle<int> area, te::TimeRange viewRange);

class ClipComponent : public juce::Component
{
public:
    ClipComponent (EditViewState& evs, te::Clip::Ptr c);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    te::Clip& getClip() { return *clip; }
    std::function<void (te::Clip&)> onDoubleClick;

protected:
    enum class DragMode { none, move, resizeStart, resizeEnd, fadeIn, fadeOut };

    struct GroupDragItem
    {
        te::Clip::Ptr clip;
        te::TimePosition originalStart;
    };
    using RippleDragItem = GroupDragItem;

    te::TimePosition timeAtLaneX (int laneX) const;
    te::TimePosition snapTime (te::TimePosition time) const;
    DragMode dragModeForEvent (const juce::MouseEvent& e) const;
    void updateCursorForMode (DragMode mode);
    void captureGroupDragItems();
    void captureRippleDragItems (te::TimePosition anchor);
    void paintSelectionAndGroupIndicators (juce::Graphics& g) const;

    EditViewState& editViewState;
    te::Clip::Ptr clip;

    DragMode dragMode = DragMode::none;
    te::TimePosition dragAnchorTime;
    te::TimePosition originalStart;
    te::TimePosition originalEnd;
    te::TimeDuration originalFadeIn, originalFadeOut;
    juce::Array<GroupDragItem> groupDragItems;
    juce::Array<RippleDragItem> rippleDragItems;

    static constexpr int resizeHandleWidth = 6;
    static constexpr int fadeHandlePx = 10;
    static constexpr double minClipLengthSeconds = 0.05;
};

class AudioClipComponent : public ClipComponent
{
public:
    AudioClipComponent (EditViewState& evs, te::Clip::Ptr c);
    void paint (juce::Graphics& g) override;

private:
    void updateThumbnail();
    void paintFadeOverlay (juce::Graphics& g) const;
    std::unique_ptr<te::SmartThumbnail> thumbnail;
};

class MidiClipComponent : public ClipComponent
{
public:
    MidiClipComponent (EditViewState& evs, te::Clip::Ptr c);
    void paint (juce::Graphics& g) override;
};

} // namespace arrange
