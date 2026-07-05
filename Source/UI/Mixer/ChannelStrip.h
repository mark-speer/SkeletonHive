#pragma once

#include "TracktionCommon.h"

namespace arrange
{

class UiTelemetryHub;

class LevelMeter : public juce::Component
{
public:
    LevelMeter (te::LevelMeasurer& measurer, UiTelemetryHub* telemetryHub = nullptr);
    ~LevelMeter() override;

    void paint (juce::Graphics& g) override;
    void updateFromMeasurer();

private:
    te::LevelMeasurer& levelMeasurer;
    UiTelemetryHub* telemetryHub = nullptr;
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
    explicit ChannelStrip (te::Track& track, UiTelemetryHub* telemetryHub = nullptr);
    explicit ChannelStrip (te::Edit& edit, UiTelemetryHub* telemetryHub = nullptr);   // master strip
    ~ChannelStrip() override;

    bool isMasterStrip() const { return track == nullptr; }
    te::Track* getTrack() const { return track; }

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class SendControlRow;

    void initialise();
    void refreshSendControls();
    void updateFromModel();
    void showAddSendMenu();

    te::Edit& edit;
    te::Track* track = nullptr;   // nullptr for the master strip
    te::VolumeAndPanPlugin::Ptr volumePlugin;
    UiTelemetryHub* telemetryHub = nullptr;

    juce::Slider fader, panSlider;
    juce::TextButton muteButton { "M" }, soloButton { "S" }, addSendButton { "+Send" };
    juce::Label nameLabel;
    std::unique_ptr<LevelMeter> meter;

    juce::OwnedArray<SendControlRow> sendRows;

    // AutomatableParameter::Listener
    void curveHasChanged (te::AutomatableParameter&) override {}
    void currentValueChanged (te::AutomatableParameter&) override;

    // ValueTree::Listener
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { refreshSendControls(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { refreshSendControls(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { refreshSendControls(); }
    void valueTreeParentChanged (juce::ValueTree&) override {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

} // namespace arrange
