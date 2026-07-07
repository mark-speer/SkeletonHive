#include "SynthModMatrixPanel.h"

#include "Engine/SynthHelpers.h"

namespace skeletonhive
{

SynthModMatrixPanel::SynthModMatrixPanel (te::FourOscPlugin& synthPlugin)
    : synth (synthPlugin)
{
    titleLabel.setText ("Mod Matrix", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    destinationLabel.setText ("Destination", juce::dontSendNotification);
    sourceLabel.setText ("Source", juce::dontSendNotification);
    depthLabel.setText ("Depth", juce::dontSendNotification);

    addAndMakeVisible (destinationLabel);
    addAndMakeVisible (sourceLabel);
    addAndMakeVisible (depthLabel);

    destinations = SynthHelpers::getModMatrixDestinations (synth);
    sources = SynthHelpers::getModMatrixSources();

    for (int i = 0; i < destinations.size(); ++i)
        destinationBox.addItem (destinations[i].label, i + 1);

    for (int i = 0; i < sources.size(); ++i)
        sourceBox.addItem (sources[i].label, i + 1);

    destinationBox.addListener (this);
    sourceBox.addListener (this);
    addAndMakeVisible (destinationBox);
    addAndMakeVisible (sourceBox);

    depthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    depthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    depthSlider.setRange (-1.0, 1.0, 0.001);
    depthSlider.onValueChange = [this]
    {
        if (updatingFromModel)
            return;

        applyDepth();
    };
    addAndMakeVisible (depthSlider);

    clearButton.onClick = [this] { clearCurrentModulation(); };
    addAndMakeVisible (clearButton);

    if (destinationBox.getNumItems() > 0)
        destinationBox.setSelectedId (1, juce::dontSendNotification);

    if (sourceBox.getNumItems() > 0)
        sourceBox.setSelectedId (1, juce::dontSendNotification);

    refreshFromModel();
}

void SynthModMatrixPanel::refreshFromModel()
{
    updatingFromModel = true;

    const int destIndex = destinationBox.getSelectedId() - 1;
    const int srcIndex = sourceBox.getSelectedId() - 1;

    if (juce::isPositiveAndBelow (destIndex, destinations.size())
        && juce::isPositiveAndBelow (srcIndex, sources.size()))
    {
        const auto& dest = destinations.getReference (destIndex);
        const auto src = sources.getReference (srcIndex).source;
        depthSlider.setValue (synth.getModulationDepth (src, dest.parameter), juce::dontSendNotification);
    }

    updatingFromModel = false;
}

void SynthModMatrixPanel::applyDepth()
{
    const int destIndex = destinationBox.getSelectedId() - 1;
    const int srcIndex = sourceBox.getSelectedId() - 1;

    if (! juce::isPositiveAndBelow (destIndex, destinations.size())
        || ! juce::isPositiveAndBelow (srcIndex, sources.size()))
        return;

    const auto& dest = destinations.getReference (destIndex);
    const auto src = sources.getReference (srcIndex).source;
    synth.setModulationDepth (src, dest.parameter, (float) depthSlider.getValue());
    SynthHelpers::persistPluginState (synth);
}

void SynthModMatrixPanel::clearCurrentModulation()
{
    const int destIndex = destinationBox.getSelectedId() - 1;
    const int srcIndex = sourceBox.getSelectedId() - 1;

    if (! juce::isPositiveAndBelow (destIndex, destinations.size())
        || ! juce::isPositiveAndBelow (srcIndex, sources.size()))
        return;

    const auto& dest = destinations.getReference (destIndex);
    const auto src = sources.getReference (srcIndex).source;
    synth.clearModulation (src, dest.parameter);
    SynthHelpers::persistPluginState (synth);
    refreshFromModel();
}

void SynthModMatrixPanel::comboBoxChanged (juce::ComboBox*)
{
    if (updatingFromModel)
        return;

    refreshFromModel();
}

void SynthModMatrixPanel::resized()
{
    auto r = getLocalBounds().reduced (4);
    titleLabel.setBounds (r.removeFromTop (20));
    r.removeFromTop (4);

    auto row = r.removeFromTop (22);
    destinationLabel.setBounds (row.removeFromLeft (88));
    destinationBox.setBounds (row);

    r.removeFromTop (4);
    row = r.removeFromTop (22);
    sourceLabel.setBounds (row.removeFromLeft (88));
    sourceBox.setBounds (row);

    r.removeFromTop (4);
    row = r.removeFromTop (24);
    depthLabel.setBounds (row.removeFromLeft (88));
    clearButton.setBounds (row.removeFromRight (64).reduced (0, 2));
    depthSlider.setBounds (row);
}

} // namespace skeletonhive
