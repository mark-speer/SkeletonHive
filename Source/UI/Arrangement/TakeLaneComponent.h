#pragma once

#include "EditViewState.h"
#include "TimelineTypes.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

class TakeLaneComponent : public juce::Component,
                          private juce::Timer
{
public:
    static constexpr int takeLaneHeight = takeLaneStripHeight;
    static constexpr int compLaneHeight = compLaneStripHeight;

    TakeLaneComponent (EditViewState& evs, te::Clip& parentClip, int takeIndex, bool isCompLane);
    ~TakeLaneComponent() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

    void refreshLayout();
    void releaseResources();

    bool isCompLane() const { return compLane; }

    std::function<void()> onCompChanged;

private:
    void timerCallback() override;
    double compTimeAtX (int x) const;
    int xForCompTime (double time) const;
    void ensureThumbnail();
    void releaseThumbnail();
    void paintWaveform (juce::Graphics& g, juce::Rectangle<int> area);
    void paintMidiPreview (juce::Graphics& g, juce::Rectangle<int> area);
    void paintCompSections (juce::Graphics& g, juce::Rectangle<int> area);
    int sectionBoundaryAtX (int x, juce::ValueTree& outSection) const;
    juce::Colour colourForTakeIndex (int index) const;

    EditViewState& editViewState;
    te::Clip::Ptr parentClip;
    int takeIndex = 0;
    bool compLane = false;

    std::shared_ptr<te::SmartThumbnail> thumbnail;
    juce::File cachedThumbnailFile;
    bool thumbnailHeld = false;

    bool draggingSection = false;
    juce::ValueTree draggedSection;
    double dragAnchorCompTime = 0.0;

    int hoveredBoundary = -1;
};

class TakeLaneStack : public juce::Component,
                      private te::ValueTreeAllEventListener,
                      private juce::AsyncUpdater
{
public:
    TakeLaneStack (EditViewState& evs, te::Clip& clip);
    ~TakeLaneStack() override;

    void refreshLayout();
    void releaseResources();

    te::Clip* getClip() const { return clip.get(); }

    std::function<void()> onLayoutChanged;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;

    void rebuildLanes();
    int stackHeight() const;

    EditViewState& editViewState;
    te::Clip::Ptr clip;
    juce::OwnedArray<TakeLaneComponent> lanes;
};

} // namespace skeletonhive
