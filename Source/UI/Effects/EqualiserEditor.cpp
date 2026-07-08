#include "EqualiserEditor.h"

namespace skeletonhive
{

std::unique_ptr<te::Plugin::EditorComponent> EqualiserEditor::create (te::EqualiserPlugin& eqPlugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new EqualiserEditor (eqPlugin));
}

EqualiserEditor::EqualiserEditor (te::EqualiserPlugin& eqPlugin)
    : eq (eqPlugin),
      curve (eqPlugin),
      controls (420)
{
    eq.state.addListener (this);

    titleLabel.setText ("EQ", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (curve);
    addAndMakeVisible (controls);

    auto isUpdating = [this] { return updatingFromModel; };

    if (eq.loGain != nullptr) controls.addRow<AutomatableSliderRow> (*eq.loGain, isUpdating);
    if (eq.loFreq != nullptr) controls.addRow<AutomatableSliderRow> (*eq.loFreq, isUpdating);
    if (eq.loQ != nullptr) controls.addRow<AutomatableSliderRow> (*eq.loQ, isUpdating);
    if (eq.midGain1 != nullptr) controls.addRow<AutomatableSliderRow> (*eq.midGain1, isUpdating);
    if (eq.midFreq1 != nullptr) controls.addRow<AutomatableSliderRow> (*eq.midFreq1, isUpdating);
    if (eq.midQ1 != nullptr) controls.addRow<AutomatableSliderRow> (*eq.midQ1, isUpdating);
    if (eq.midGain2 != nullptr) controls.addRow<AutomatableSliderRow> (*eq.midGain2, isUpdating);
    if (eq.midFreq2 != nullptr) controls.addRow<AutomatableSliderRow> (*eq.midFreq2, isUpdating);
    if (eq.midQ2 != nullptr) controls.addRow<AutomatableSliderRow> (*eq.midQ2, isUpdating);
    if (eq.hiGain != nullptr) controls.addRow<AutomatableSliderRow> (*eq.hiGain, isUpdating);
    if (eq.hiFreq != nullptr) controls.addRow<AutomatableSliderRow> (*eq.hiFreq, isUpdating);
    if (eq.hiQ != nullptr) controls.addRow<AutomatableSliderRow> (*eq.hiQ, isUpdating);

    setSize (460, 620);
}

EqualiserEditor::~EqualiserEditor()
{
    eq.state.removeListener (this);
}

void EqualiserEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromModel();
}

void EqualiserEditor::refreshFromModel()
{
    updatingFromModel = true;
    curve.repaint();
    updatingFromModel = false;
}

void EqualiserEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    curve.setBounds (r.removeFromTop (140));
    r.removeFromTop (6);
    controls.setBounds (r);
}

} // namespace skeletonhive
