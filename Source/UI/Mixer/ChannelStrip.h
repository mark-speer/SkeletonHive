#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class LevelMeter : public juce::Component,
                   private juce::Timer
{
public:
    explicit LevelMeter (te::LevelMeasurer& measurer);
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    te::LevelMeasurer& levelMeasurer;
    float level = 0.0f;
};

/** A mixer channel strip with two-way model binding: fader/pan follow parameter
    changes (automation, plugin edits) and mute/solo follow the track state.

    Constructed either for a normal track or, with the Edit-only constructor,
    for the master bus.
*/
class ChannelStrip : public juce::Component,
                     private te::AutomatableParameter::Listener,
                     private juce::ValueTree::Listener
{
public:
    explicit ChannelStrip (te::Track& track);
    explicit ChannelStrip (te::Edit& edit);   // master strip
    ~ChannelStrip() override;

    bool isMasterStrip() const { return track == nullptr; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void initialise();
    void refreshSendControls();
    void updateFromModel();

    te::AuxSendPlugin* getSend() const;

    // AutomatableParameter::Listener
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override;

    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override {}
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override {}
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override {}
    void valueTreeParentChanged (juce::ValueTree&) override {}

    te::Edit& edit;
    te::Track* track = nullptr;   // nullptr for the master strip
    te::VolumeAndPanPlugin::Ptr volumePlugin;
    te::Plugin::Ptr sendPlugin;

    juce::Slider fader, panSlider, sendSlider;
    juce::TextButton muteButton { "M" }, soloButton { "S" }, addSendButton { "+Send" };
    juce::Label nameLabel;
    std::unique_ptr<LevelMeter> meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace arrange
