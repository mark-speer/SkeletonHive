#pragma once

#include "Engine/SynthHelpers.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

/** Simplified mod-matrix editor for te::FourOscPlugin. */
class SynthModMatrixPanel : public juce::Component,
                            private juce::ComboBox::Listener
{
public:
    explicit SynthModMatrixPanel (te::FourOscPlugin& synthPlugin);

    void refreshFromModel();
    void setUpdatingFromModel (bool updating) { updatingFromModel = updating; }

    void resized() override;

private:
    void comboBoxChanged (juce::ComboBox* box) override;
    void applyDepth();
    void clearCurrentModulation();

    te::FourOscPlugin& synth;
    juce::Label titleLabel;
    juce::Label destinationLabel;
    juce::Label sourceLabel;
    juce::Label depthLabel;
    juce::ComboBox destinationBox;
    juce::ComboBox sourceBox;
    juce::Slider depthSlider;
    juce::TextButton clearButton { "Clear" };
    juce::Array<ModMatrixDestination> destinations;
    juce::Array<ModMatrixSourceOption> sources;
    bool updatingFromModel = false;
};

} // namespace skeletonhive
