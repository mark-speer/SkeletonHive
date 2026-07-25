#include "NamEditor.h"

namespace skeletonhive
{

std::unique_ptr<te::Plugin::EditorComponent> NamEditor::create (NamPlugin& plugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new NamEditor (plugin));
}

NamEditor::NamEditor (NamPlugin& plug)
    : nam (plug),
      controls (420)
{
    nam.state.addListener (this);

    titleLabel.setText ("Neural Amp Modeler", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    pathLabel.setJustificationType (juce::Justification::centredLeft);
    pathLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.85f));
    pathLabel.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (pathLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
    statusLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (statusLabel);

    browseButton.onClick = [this] { browseForModel(); };
    reloadButton.onClick = [safeNam = te::Plugin::Ptr (&nam)]
    {
        if (auto* plugin = dynamic_cast<NamPlugin*> (safeNam.get()))
        {
            const auto path = plugin->getModelPath();
            if (path.isNotEmpty())
                plugin->loadModelFile (path);
        }
    };
    addAndMakeVisible (browseButton);
    addAndMakeVisible (reloadButton);
    addAndMakeVisible (controls);

    auto isUpdating = [this] { return updatingFromModel; };

    if (nam.inputParam != nullptr)
        controls.addRow<AutomatableSliderRow> (*nam.inputParam, isUpdating);
    if (nam.outputParam != nullptr)
        controls.addRow<AutomatableSliderRow> (*nam.outputParam, isUpdating);

    refreshStatus();
    setSize (460, 280);
}

NamEditor::~NamEditor()
{
    nam.state.removeListener (this);
}

void NamEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (24));
    r.removeFromTop (6);

    auto buttonRow = r.removeFromTop (28);
    browseButton.setBounds (buttonRow.removeFromRight (96).reduced (2));
    reloadButton.setBounds (buttonRow.removeFromRight (80).reduced (2));
    pathLabel.setBounds (buttonRow);

    r.removeFromTop (4);
    statusLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);
    controls.setBounds (r);
}

void NamEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    if (property.toString() == "modelPath" || property.toString() == "status")
        refreshStatus();
}

void NamEditor::refreshStatus()
{
    const auto path = nam.getModelPath();
    pathLabel.setText (path.isNotEmpty() ? path : "No .nam model selected", juce::dontSendNotification);
    statusLabel.setText (nam.getStatusMessage(), juce::dontSendNotification);
}

void NamEditor::browseForModel()
{
    auto start = juce::File (nam.getModelPath());
    if (! start.existsAsFile())
        start = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    // Keep the plugin alive across the async chooser; do not capture `this`
    // (editor can be destroyed while the dialog is open).
    te::Plugin::Ptr safeNam (&nam);
    auto fc = std::make_shared<juce::FileChooser> ("Load NAM model", start, "*.nam");
    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                     [safeNam, fc] (const juce::FileChooser&)
                     {
                         const auto result = fc->getResult();
                         if (! result.existsAsFile())
                             return;

                         if (auto* plugin = dynamic_cast<NamPlugin*> (safeNam.get()))
                             plugin->loadModelFile (result.getFullPathName());
                     });
}

} // namespace skeletonhive
