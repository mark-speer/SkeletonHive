#include "DrumRackEditor.h"

#include "DrumPadComponent.h"
#include "Engine/DrumRackHelpers.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

std::unique_ptr<te::Plugin::EditorComponent> DrumRackEditor::create (te::RackInstance& rackInstance)
{
    if (! DrumRackHelpers::isDrumRack (rackInstance))
        return {};

    return std::unique_ptr<te::Plugin::EditorComponent> (new DrumRackEditor (rackInstance));
}

DrumRackEditor::DrumRackEditor (te::RackInstance& rackInstance)
    : rack (&rackInstance)
{
    DrumRackHelpers::ensureMacroBindings (rackInstance);

    titleLabel.setText ("Drum Rack", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    detailLabel.setJustificationType (juce::Justification::centredLeft);
    detailLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (detailLabel);

    browseButton.onClick = [this] { browseForPadSample (selectedPad); };
    addAndMakeVisible (browseButton);

    clearButton.onClick = [this] { clearSelectedPad(); };
    addAndMakeVisible (clearButton);

    const int padCount = DrumRackHelpers::getPadCount (rackInstance);

    for (int padIndex = 0; padIndex < padCount; ++padIndex)
    {
        auto* pad = pads.add (new DrumPadComponent (padIndex, DrumRackHelpers::midiNoteForPad (padIndex)));

        pad->onSelected = [this] (int index) { setSelectedPad (index); };
        pad->onBrowseRequested = [this] (int index) { browseForPadSample (index); };
        pad->onSampleDropped = [this] (int index, const juce::File& file) { assignSample (index, file); };
        pad->onClearRequested = [this] (int index)
        {
            if (rack != nullptr)
                DrumRackHelpers::clearPadSample (*rack, index);

            refreshPads();
        };

        addAndMakeVisible (pad);
    }

    refreshPads();
    setSelectedPad (0);
    startTimerHz (4);
    setSize (420, 460);
}

void DrumRackEditor::refreshPads()
{
    if (rack == nullptr)
        return;

    for (int padIndex = 0; padIndex < pads.size(); ++padIndex)
    {
        const auto sampleName = DrumRackHelpers::getPadSampleName (*rack, padIndex);
        pads[padIndex]->setSampleName (sampleName);
        pads[padIndex]->setSelected (padIndex == selectedPad);
    }

    const auto sampleName = DrumRackHelpers::getPadSampleName (*rack, selectedPad);
    const int note = DrumRackHelpers::midiNoteForPad (selectedPad);
    detailLabel.setText ("Pad " + juce::String (selectedPad + 1) + "  ·  MIDI " + juce::String (note)
                             + (sampleName.isEmpty() ? "  ·  Empty" : "  ·  " + sampleName),
                         juce::dontSendNotification);

    clearButton.setEnabled (sampleName.isNotEmpty());
}

void DrumRackEditor::setSelectedPad (int padIndex)
{
    if (! juce::isPositiveAndBelow (padIndex, pads.size()))
        return;

    selectedPad = padIndex;
    refreshPads();
}

void DrumRackEditor::browseForPadSample (int padIndex)
{
    if (rack == nullptr)
        return;

    setSelectedPad (padIndex);

    EngineHelpers::browseForAudioFile (rack->edit.engine, [this, padIndex] (const juce::File& file)
    {
        assignSample (padIndex, file);
    });
}

void DrumRackEditor::assignSample (int padIndex, const juce::File& file)
{
    if (rack == nullptr)
        return;

    const auto error = DrumRackHelpers::assignSampleToPad (*rack, padIndex, file);

    if (error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Sample Load Failed",
                                                error);
        return;
    }

    setSelectedPad (padIndex);
}

void DrumRackEditor::clearSelectedPad()
{
    if (rack == nullptr)
        return;

    DrumRackHelpers::clearPadSample (*rack, selectedPad);
    refreshPads();
}

void DrumRackEditor::timerCallback()
{
    if (rack == nullptr)
        return;

    int revision = 0;

    for (int padIndex = 0; padIndex < pads.size(); ++padIndex)
        if (const auto* sampler = DrumRackHelpers::getPadSampler (*rack, padIndex))
            revision += sampler->getNumSounds() * 31
                        + (sampler->getNumSounds() > 0 ? sampler->getSoundName (0).hashCode() : 0);

    if (revision != lastKnownSoundRevision)
    {
        lastKnownSoundRevision = revision;
        refreshPads();
    }
}

void DrumRackEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    titleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);

    auto detailRow = r.removeFromBottom (28);
    clearButton.setBounds (detailRow.removeFromRight (64).reduced (1));
    detailRow.removeFromRight (4);
    browseButton.setBounds (detailRow.removeFromRight (84).reduced (1));
    detailRow.removeFromRight (6);
    detailLabel.setBounds (detailRow);

    r.removeFromBottom (8);

    const int columns = 4;
    const int rows = juce::jmax (1, (pads.size() + columns - 1) / columns);
    const int cellW = r.getWidth() / columns;
    const int cellH = r.getHeight() / rows;

    for (int i = 0; i < pads.size(); ++i)
    {
        const int row = i / columns;
        const int col = i % columns;
        pads[i]->setBounds (r.getX() + col * cellW + 2,
                            r.getY() + row * cellH + 2,
                            cellW - 4,
                            cellH - 4);
    }
}

} // namespace skeletonhive
