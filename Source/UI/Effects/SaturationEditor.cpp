#include "SaturationEditor.h"

namespace skeletonhive
{

std::unique_ptr<te::Plugin::EditorComponent> SaturationEditor::create (SaturationPlugin& plugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new SaturationEditor (plugin));
}

SaturationEditor::SaturationEditor (SaturationPlugin& plug)
    : saturation (plug),
      controls (420)
{
    saturation.state.addListener (this);

    titleLabel.setText ("Saturation", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (controls);

    auto isUpdating = [this] { return updatingFromModel; };

    if (saturation.driveParam != nullptr)
        controls.addRow<AutomatableSliderRow> (*saturation.driveParam, isUpdating);
    if (saturation.mixParam != nullptr)
        controls.addRow<AutomatableSliderRow> (*saturation.mixParam, isUpdating);
    if (saturation.outputParam != nullptr)
        controls.addRow<AutomatableSliderRow> (*saturation.outputParam, isUpdating);

    setSize (460, 260);
}

SaturationEditor::~SaturationEditor()
{
    saturation.state.removeListener (this);
}

void SaturationEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    controls.setBounds (r);
}

} // namespace skeletonhive
