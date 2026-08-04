#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

class UiTelemetryHub;

/** Stereo VU meter with peak hold, clip LED, and dB-scaled colour zones. */
class LevelMeter : public juce::Component
{
public:
    LevelMeter (te::LevelMeasurer& measurer, UiTelemetryHub* telemetryHub = nullptr);
    ~LevelMeter() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void updateFromMeasurer();

    static constexpr int preferredWidth = 16;

private:
    static float dbToMeterNorm (float db);
    void paintChannel (juce::Graphics& g, juce::Rectangle<float> bounds,
                       float levelNorm, float peakNorm, bool clipped) const;
    void resetPeaks();

    te::LevelMeasurer& levelMeasurer;
    te::LevelMeasurer::Client levelClient;
    UiTelemetryHub* telemetryHub = nullptr;

    float levels[2] { 0.0f, 0.0f };
    float peakHolds[2] { 0.0f, 0.0f };
    int peakHoldTicks[2] { 0, 0 };
    bool clipped[2] { false, false };
    int numChannels = 1;

    static constexpr float meterFloorDb = -60.0f;
    static constexpr int peakHoldTicksBeforeDecay = 30; // ~1s at 30 Hz
    static constexpr float peakDecayPerTick = 0.02f;
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
    void mouseDown (const juce::MouseEvent& e) override;

private:
    class SendControlRow;

    void initialise();
    void refreshSendControls();
    void updateFromModel();
    void updateControlLabels();
    void showAddSendMenu();
    void showParameterContextMenu (te::AutomatableParameter& param, juce::Component& target);

    te::Edit& edit;
    te::Track* track = nullptr;   // nullptr for the master strip
    te::VolumeAndPanPlugin::Ptr volumePlugin;
    UiTelemetryHub* telemetryHub = nullptr;

    juce::Slider fader, panSlider;
    juce::Label volumeValueLabel, panValueLabel;
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

} // namespace skeletonhive
