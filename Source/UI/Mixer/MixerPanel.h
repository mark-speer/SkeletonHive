#pragma once

#include "ChannelStrip.h"
#include "UI/Arrangement/TrackComponents.h"

namespace arrange
{

class UiTelemetryHub;

class MixerPanel : public juce::Component,
                   private te::ValueTreeAllEventListener,
                   private FlaggedAsyncUpdater
{
public:
    MixerPanel (te::Edit& edit, UiTelemetryHub* telemetryHub = nullptr);
    ~MixerPanel() override;

    void rebuild();
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void valueTreeChanged() override {}
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree& child) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree& child, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void handleAsyncUpdate() override;

    static bool isMixerTrack (const juce::ValueTree& child);
    juce::Array<te::Track*> collectMixerTracks() const;
    ChannelStrip* findStripForTrack (te::Track& track) const;
    void syncTrackStrips();
    void layoutStrips();

    te::Edit& edit;
    UiTelemetryHub* telemetryHub = nullptr;
    juce::Viewport viewport;
    juce::Component stripContainer;
    juce::OwnedArray<ChannelStrip> strips;
    std::unique_ptr<ChannelStrip> masterStrip;

    bool rebuildTrackList = false;
    bool relayoutStrips = false;
};

} // namespace arrange
