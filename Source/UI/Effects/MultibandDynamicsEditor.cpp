#include "MultibandDynamicsEditor.h"

namespace skeletonhive
{

std::unique_ptr<te::Plugin::EditorComponent> MultibandDynamicsEditor::create (MultibandDynamicsPlugin& plugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new MultibandDynamicsEditor (plugin));
}

MultibandDynamicsEditor::MultibandDynamicsEditor (MultibandDynamicsPlugin& plug)
    : multiband (plug),
      controls (420)
{
    multiband.state.addListener (this);

    titleLabel.setText ("Multiband Dynamics", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (controls);

    auto isUpdating = [this] { return updatingFromModel; };

    const te::AutomatableParameter::Ptr params[] =
    {
        multiband.lowCrossoverParam, multiband.highCrossoverParam,
        multiband.lowThresholdParam, multiband.midThresholdParam, multiband.highThresholdParam,
        multiband.lowRatioParam, multiband.midRatioParam, multiband.highRatioParam,
        multiband.outputParam
    };

    for (auto& param : params)
    {
        if (param != nullptr)
            controls.addRow<AutomatableSliderRow> (*param, isUpdating);
    }

    setSize (460, 480);
}

MultibandDynamicsEditor::~MultibandDynamicsEditor()
{
    multiband.state.removeListener (this);
}

void MultibandDynamicsEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    controls.setBounds (r);
}

} // namespace skeletonhive
