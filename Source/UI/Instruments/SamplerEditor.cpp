#include "SamplerEditor.h"

#include "Engine/ContentDragManager.h"
#include "Engine/EngineHelpers.h"
#include "Engine/SamplerHelpers.h"

namespace skeletonhive
{

namespace
{

void configureLinearSlider (juce::Slider& slider, const juce::String& suffix, int textBoxWidth = 52)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, textBoxWidth, 20);
    slider.setTextValueSuffix (suffix);
}

} // namespace

class SamplerEditor::SoundListModel : public juce::ListBoxModel
{
public:
    explicit SoundListModel (SamplerEditor& ownerIn) : owner (ownerIn) {}

    int getNumRows() override { return owner.sampler.getNumSounds(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow (row, owner.sampler.getNumSounds()))
            return;

        if (rowIsSelected)
            g.fillAll (juce::Colours::white.withAlpha (0.12f));

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.setFont (juce::FontOptions (12.0f));

        juce::String line = owner.sampler.getSoundName (row);
        line << "  ·  MIDI " << owner.sampler.getKeyNote (row);

        g.drawText (line, 6, 0, width - 12, height, juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        owner.setSelectedSound (lastRowSelected);
    }

    SamplerEditor& owner;
};

std::unique_ptr<te::Plugin::EditorComponent> SamplerEditor::create (te::SamplerPlugin& samplerPlugin)
{
    return std::unique_ptr<te::Plugin::EditorComponent> (new SamplerEditor (samplerPlugin));
}

SamplerEditor::SamplerEditor (te::SamplerPlugin& samplerPlugin)
    : sampler (samplerPlugin),
      waveform (samplerPlugin)
{
    sampler.state.addListener (this);

    titleLabel.setText ("Sampler", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    listModel = std::make_unique<SoundListModel> (*this);
    soundList.setModel (listModel.get());
    soundList.setRowHeight (22);
    addAndMakeVisible (soundList);

    addButton.onClick = [this] { browseForSample(); };
    addAndMakeVisible (addButton);

    removeButton.onClick = [this] { removeSelectedSound(); };
    addAndMakeVisible (removeButton);

    emptyLabel.setText ("Drop or add samples to trigger from MIDI.", juce::dontSendNotification);
    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (emptyLabel);

    gainLabel.setText ("Gain", juce::dontSendNotification);
    panLabel.setText ("Pan", juce::dontSendNotification);
    rootLabel.setText ("Root", juce::dontSendNotification);

    configureLinearSlider (gainSlider, " dB");
    gainSlider.setRange (-48.0, 6.0, 0.1);
    gainSlider.onValueChange = [this]
    {
        if (updatingFromModel || ! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
            return;

        pendingGainDb = (float) gainSlider.getValue();
        pendingPan = (float) panSlider.getValue();
        pendingGainUpdate = true;
        startTimer (120);
    };

    configureLinearSlider (panSlider, "", 44);
    panSlider.setRange (-1.0, 1.0, 0.01);
    panSlider.onValueChange = [this]
    {
        if (updatingFromModel || ! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
            return;

        pendingGainDb = (float) gainSlider.getValue();
        pendingPan = (float) panSlider.getValue();
        pendingGainUpdate = true;
        startTimer (120);
    };

    configureLinearSlider (rootSlider, "", 44);
    rootSlider.setRange (0.0, 127.0, 1.0);
    rootSlider.setNumDecimalPlacesToDisplay (0);
    rootSlider.textFromValueFunction = [] (double value) { return juce::String (juce::roundToInt (value)); };
    rootSlider.onValueChange = [this]
    {
        if (updatingFromModel || ! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
            return;

        const int note = juce::roundToInt (rootSlider.getValue());
        sampler.setSoundParams (selectedSound, note, note, note);
    };

    oneShotButton.setTooltip ("When enabled, notes play to completion instead of stopping on key release.");
    oneShotButton.onClick = [this]
    {
        if (updatingFromModel || ! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
            return;

        sampler.setSoundOpenEnded (selectedSound, oneShotButton.getToggleState());
    };

    oneShotHintLabel.setText ("Controls release behavior (not sample loop).", juce::dontSendNotification);
    oneShotHintLabel.setFont (juce::FontOptions (10.0f));
    oneShotHintLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.55f));

    addAndMakeVisible (gainLabel);
    addAndMakeVisible (panLabel);
    addAndMakeVisible (rootLabel);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (panSlider);
    addAndMakeVisible (rootSlider);
    addAndMakeVisible (oneShotButton);
    addAndMakeVisible (oneShotHintLabel);
    addAndMakeVisible (waveform);

    waveform.onExcerptChanged = [this] { refreshFromModel(); };

    refreshFromModel();
    setSize (480, 360);
}

SamplerEditor::~SamplerEditor()
{
    sampler.state.removeListener (this);
}

void SamplerEditor::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&)
{
    refreshFromModel();
}

void SamplerEditor::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    refreshFromModel();
}

void SamplerEditor::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    refreshFromModel();
}

void SamplerEditor::timerCallback()
{
    stopTimer();

    if (pendingGainUpdate)
        applyPendingGains();
}

void SamplerEditor::applyPendingGains()
{
    pendingGainUpdate = false;

    if (! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
        return;

    sampler.setSoundGains (selectedSound, pendingGainDb, pendingPan);
}

void SamplerEditor::refreshFromModel()
{
    updatingFromModel = true;

    refreshSoundList();

    if (sampler.getNumSounds() == 0)
    {
        selectedSound = -1;
        waveform.setSoundIndex (-1);
    }
    else if (! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
    {
        selectedSound = 0;
        soundList.selectRow (0, juce::dontSendNotification);
    }

    if (juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
    {
        waveform.setSoundIndex (selectedSound);

        gainSlider.setValue (sampler.getSoundGainDb (selectedSound), juce::dontSendNotification);
        panSlider.setValue (sampler.getSoundPan (selectedSound), juce::dontSendNotification);
        rootSlider.setValue (sampler.getKeyNote (selectedSound), juce::dontSendNotification);
        oneShotButton.setToggleState (sampler.isSoundOpenEnded (selectedSound), juce::dontSendNotification);
    }

    updateDetailVisibility();
    updatingFromModel = false;
}

void SamplerEditor::refreshSoundList()
{
    soundList.updateContent();
    emptyLabel.setVisible (sampler.getNumSounds() == 0);
    soundList.setVisible (sampler.getNumSounds() > 0);
    removeButton.setEnabled (juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()));
}

void SamplerEditor::setSelectedSound (int index)
{
    if (index == selectedSound)
        return;

    selectedSound = index;
    refreshFromModel();
}

void SamplerEditor::browseForSample()
{
    EngineHelpers::browseForAudioFile (sampler.edit.engine, [this] (const juce::File& file)
    {
        assignSampleFile (file);
    });
}

void SamplerEditor::removeSelectedSound()
{
    if (! juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds()))
        return;

    sampler.removeSound (selectedSound);
    selectedSound = juce::jmin (selectedSound, sampler.getNumSounds() - 1);
    refreshFromModel();
}

void SamplerEditor::assignSampleFile (const juce::File& file)
{
    const int keyNote = juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds())
                            ? sampler.getKeyNote (selectedSound)
                            : 36 + sampler.getNumSounds();

    const auto error = SamplerHelpers::assignSample (sampler, file, keyNote);

    if (error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Sample Load Failed",
                                                error);
        return;
    }

    setSelectedSound (sampler.getNumSounds() - 1);
    soundList.selectRow (selectedSound);
}

void SamplerEditor::updateDetailVisibility()
{
    const bool hasSelection = juce::isPositiveAndBelow (selectedSound, sampler.getNumSounds());

    gainLabel.setVisible (hasSelection);
    panLabel.setVisible (hasSelection);
    rootLabel.setVisible (hasSelection);
    gainSlider.setVisible (hasSelection);
    panSlider.setVisible (hasSelection);
    rootSlider.setVisible (hasSelection);
    oneShotButton.setVisible (hasSelection);
    oneShotHintLabel.setVisible (hasSelection);
    waveform.setVisible (hasSelection);
}

bool SamplerEditor::isInterestedInDragSource (const SourceDetails& dragSourceDetails)
{
    return dragSourceDetails.description.toString().startsWith (ContentDragTypes::sampleInsert);
}

void SamplerEditor::itemDragEnter (const SourceDetails&)
{
    dragHover = true;
    repaint();
}

void SamplerEditor::itemDragExit (const SourceDetails&)
{
    dragHover = false;
    repaint();
}

void SamplerEditor::itemDropped (const SourceDetails& dragSourceDetails)
{
    dragHover = false;

    const auto payload = ContentDragPayload::parse (dragSourceDetails.description);

    if (payload.isValid())
        assignSampleFile (payload.file);

    repaint();
}

void SamplerEditor::resized()
{
    auto r = getLocalBounds().reduced (8);

    if (dragHover)
        r = r.reduced (2);

    titleLabel.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);

    auto buttons = r.removeFromBottom (28);
    removeButton.setBounds (buttons.removeFromRight (72).reduced (1));
    buttons.removeFromRight (4);
    addButton.setBounds (buttons.removeFromLeft (110).reduced (1));

    r.removeFromBottom (6);

    auto controls = r.removeFromBottom (88);
    auto row1 = controls.removeFromTop (24);
    gainLabel.setBounds (row1.removeFromLeft (40));
    gainSlider.setBounds (row1.removeFromLeft (160).reduced (0, 2));
    row1.removeFromLeft (8);
    panLabel.setBounds (row1.removeFromLeft (32));
    panSlider.setBounds (row1.removeFromLeft (120).reduced (0, 2));

    controls.removeFromTop (4);
    auto row2 = controls.removeFromTop (24);
    rootLabel.setBounds (row2.removeFromLeft (40));
    rootSlider.setBounds (row2.removeFromLeft (80).reduced (0, 2));
    row2.removeFromLeft (12);
    oneShotButton.setBounds (row2.removeFromLeft (88).reduced (0, 2));
    oneShotHintLabel.setBounds (row2.reduced (0, 2));

    r.removeFromBottom (6);
    waveform.setBounds (r.removeFromBottom (120));
    r.removeFromBottom (6);

    emptyLabel.setBounds (r);
    soundList.setBounds (r);
}

} // namespace skeletonhive
