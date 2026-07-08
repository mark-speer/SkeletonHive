#include "CompressorEditor.h"

namespace skeletonhive
{

std::unique_ptr<te::Plugin::EditorComponent> CompressorEditor::create (te::CompressorPlugin& compPlugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new CompressorEditor (compPlugin));
}

CompressorEditor::CompressorEditor (te::CompressorPlugin& compPlugin)
    : comp (compPlugin),
      controls (420)
{
    comp.state.addListener (this);

    titleLabel.setText ("Compressor", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (controls);

    auto isUpdating = [this] { return updatingFromModel; };

    for (auto* param : comp.getAutomatableParameters())
    {
        if (param != nullptr)
            controls.addRow<AutomatableSliderRow> (*param, isUpdating);
    }

    sidechainRow = std::make_unique<BoolToggleRow> ("Sidechain Trigger", comp.useSidechainTrigger, isUpdating);
    controls.getContent().addAndMakeVisible (sidechainRow.get());
    controls.relayout();

    setSize (460, 420);
}

CompressorEditor::~CompressorEditor()
{
    comp.state.removeListener (this);
}

void CompressorEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    updatingFromModel = true;
    if (sidechainRow != nullptr)
        sidechainRow->refresh();
    updatingFromModel = false;
}

void CompressorEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    controls.setBounds (r);
}

} // namespace skeletonhive
