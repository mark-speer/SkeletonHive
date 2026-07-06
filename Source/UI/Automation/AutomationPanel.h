#pragma once

#include "AutomationLaneComponent.h"

namespace skeletonhive
{

/** Bottom-panel automation editor for the selected track.

    Shows one lane per parameter (volume/pan by default, plus every parameter
    that already has automation). More lanes can be added from the parameter
    combo. Read/Touch/Latch set the track's te::AutomationMode and toggle the
    engine's AutomationRecordManager write state, so fader/plugin moves during
    playback are recorded by TE itself.
*/
class AutomationPanel : public juce::Component
{
public:
    AutomationPanel (te::Edit& edit, EditViewState& viewState);

    void setTrack (te::Track* track);
    te::Track* getTrack() const { return track.get(); }

    int getPreferredHeight() const;

    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    void rebuildLanes();
    void rebuildParameterChoices();
    void addLaneForParameter (te::AutomatableParameter& param);
    void removeLane (AutomationLaneComponent& lane);
    void setMode (te::AutomationMode mode);
    void syncModeButtons();
    void layoutLanes();

    te::Edit& edit;
    EditViewState& editViewState;
    te::Track::Ptr track;

    juce::Label trackLabel;
    juce::TextButton readButton { "Read" }, touchButton { "Touch" }, latchButton { "Latch" };
    juce::ComboBox addParamBox;

    juce::Viewport lanesViewport;
    juce::Component lanesHolder;
    juce::OwnedArray<AutomationLaneComponent> lanes;
    juce::Array<te::AutomatableParameter*> parameterChoices;

    static constexpr int headerHeight = 26;
    static constexpr int laneHeight = 64;
    static constexpr int maxPanelHeight = 240;
};

} // namespace skeletonhive
