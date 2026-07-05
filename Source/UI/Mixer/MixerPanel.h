#pragma once

#include "ChannelStrip.h"

namespace arrange
{

class MixerPanel : public juce::Component,
                   private te::ValueTreeAllEventListener,
                   private juce::AsyncUpdater
{
public:
    explicit MixerPanel (te::Edit& edit);
    ~MixerPanel() override;

    void rebuild();
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int) override;
    void handleAsyncUpdate() override { rebuild(); }

    te::Edit& edit;
    juce::Viewport viewport;
    juce::Component stripContainer;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> masterStrip;
};

} // namespace arrange
