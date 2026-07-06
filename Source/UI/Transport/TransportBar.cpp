#include "TransportBar.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

namespace
{
const juce::StringArray timeSigChoices { "4/4", "3/4", "6/8", "2/4", "5/4", "7/8", "12/8" };
}

TransportBar::TransportBar (te::Edit& e, TransportController& tc)
    : edit (e), transportController (tc)
{
    returnButton.onClick = [this] { transportController.returnToStart(); };
    playButton.onClick = [this] { transportController.togglePlay(); updateButtonStates(); };
    stopButton.onClick = [this] { transportController.stop(); updateButtonStates(); };
    recordButton.onClick = [this] { transportController.toggleRecord(); updateButtonStates(); };

    loopButton.setClickingTogglesState (true);
    loopButton.onClick = [this] { transportController.setLooping (loopButton.getToggleState()); };

    punchButton.setClickingTogglesState (true);
    punchButton.setTooltip ("Punch recording: only record inside the loop brace");
    punchButton.onClick = [this] { transportController.enablePunchIn (punchButton.getToggleState()); };

    clickButton.setClickingTogglesState (true);
    clickButton.setTooltip ("Metronome click (volume slider to the right)");
    clickButton.setToggleState (transportController.isClickEnabled(), juce::dontSendNotification);
    clickButton.onClick = [this] { transportController.setClickEnabled (clickButton.getToggleState()); };

    clickVolumeSlider.setRange (0.2, 1.0, 0.01);
    clickVolumeSlider.setValue (transportController.getClickVolume(), juce::dontSendNotification);
    clickVolumeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    clickVolumeSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    clickVolumeSlider.setTooltip ("Metronome volume");
    clickVolumeSlider.onValueChange = [this]
    {
        transportController.setClickVolume ((float) clickVolumeSlider.getValue());
    };

    countInBox.addItem ("No Count-In", (int) te::Edit::CountIn::none + 1);
    countInBox.addItem ("1 Bar Count-In", (int) te::Edit::CountIn::oneBar + 1);
    countInBox.addItem ("2 Bar Count-In", (int) te::Edit::CountIn::twoBar + 1);
    countInBox.addItem ("2 Beat Count-In", (int) te::Edit::CountIn::twoBeat + 1);
    countInBox.addItem ("1 Beat Count-In", (int) te::Edit::CountIn::oneBeat + 1);
    countInBox.setSelectedId ((int) transportController.getCountInMode() + 1, juce::dontSendNotification);
    countInBox.setTooltip ("Count-in before recording starts");
    countInBox.onChange = [this]
    {
        transportController.setCountInMode (static_cast<te::Edit::CountIn> (countInBox.getSelectedId() - 1));
    };

    newButton.onClick = [this] { if (onNewProject) onNewProject(); };
    openButton.onClick = [this] { if (onOpenProject) onOpenProject(); };
    saveButton.onClick = [this] { if (onSaveProject) onSaveProject(); };
    saveAsButton.onClick = [this] { if (onSaveProjectAs) onSaveProjectAs(); };
    exportButton.onClick = [this] { if (onExport) onExport(); };
    importButton.onClick = [this] { if (onImportAudio) onImportAudio(); };
    addAudioButton.onClick = [this] { if (onAddAudioTrack) onAddAudioTrack(); };
    addMidiButton.onClick = [this] { if (onAddMidiTrack) onAddMidiTrack(); };
    addClipButton.onClick = [this] { if (onAddMidiClip) onAddMidiClip(); };
    settingsButton.onClick = [this] { if (onShowPreferences) onShowPreferences(); else if (onAudioSettings) onAudioSettings(); };
    pluginsButton.onClick = [this] { if (onScanPlugins) onScanPlugins(); };
    mixerButton.onClick = [this] { if (onToggleMixer) onToggleMixer(); };
    sidechainButton.onClick = [this] { if (onToggleSidechain) onToggleSidechain(); };
    automationButton.setTooltip ("Show/hide the automation panel for the selected track");
    automationButton.onClick = [this] { if (onToggleAutomation) onToggleAutomation(); };

    learnButton.setClickingTogglesState (true);
    learnButton.setTooltip ("MIDI learn mode — adjust a control, then move a MIDI controller");
    learnButton.onClick = [this] { if (onToggleMidiLearn) onToggleMidiLearn(); };

    tempoSlider.setRange (20.0, 300.0, 0.1);
    tempoSlider.setValue (transportController.getTempo(), juce::dontSendNotification);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 20);
    tempoSlider.setTooltip ("Tempo at the playhead position");
    tempoSlider.onValueChange = [this]
    {
        transportController.setTempo (tempoSlider.getValue());
    };

    for (int i = 0; i < timeSigChoices.size(); ++i)
        timeSigBox.addItem (timeSigChoices[i], i + 1);
    timeSigBox.setTooltip ("Time signature at the playhead position");
    timeSigBox.onChange = [this]
    {
        const auto sig = timeSigBox.getText();
        const int numerator = sig.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        const int denominator = sig.fromFirstOccurrenceOf ("/", false, false).getIntValue();
        if (numerator > 0 && denominator > 0)
            transportController.setTimeSignature (numerator, denominator);
    };
    syncTempoControls();

    positionLabel.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (returnButton);
    addAndMakeVisible (playButton);
    addAndMakeVisible (stopButton);
    addAndMakeVisible (recordButton);
    addAndMakeVisible (loopButton);
    addAndMakeVisible (punchButton);
    addAndMakeVisible (newButton);
    addAndMakeVisible (openButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (saveAsButton);
    addAndMakeVisible (exportButton);
    addAndMakeVisible (importButton);
    addAndMakeVisible (addAudioButton);
    addAndMakeVisible (addMidiButton);
    addAndMakeVisible (addClipButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (pluginsButton);
    addAndMakeVisible (mixerButton);
    addAndMakeVisible (sidechainButton);
    addAndMakeVisible (automationButton);
    addAndMakeVisible (learnButton);
    addAndMakeVisible (clickButton);
    addAndMakeVisible (clickVolumeSlider);
    addAndMakeVisible (countInBox);
    addAndMakeVisible (positionLabel);
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);
    addAndMakeVisible (timeSigBox);

    edit.getTransport().addChangeListener (this);
    startTimerHz (15);
    updateButtonStates();
}

TransportBar::~TransportBar()
{
    edit.getTransport().removeChangeListener (this);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (4);
    const int h = 24;
    const int btnW = 52;

    auto row1 = r.removeFromTop (h);
    for (auto* btn : { &newButton, &openButton, &saveButton, &saveAsButton, &exportButton, &importButton })
    {
        btn->setBounds (row1.removeFromLeft (btnW).reduced (1));
    }
    row1.removeFromLeft (8);
    for (auto* btn : { &returnButton, &playButton, &stopButton, &recordButton, &loopButton, &punchButton })
    {
        btn->setBounds (row1.removeFromLeft (btnW).reduced (1));
    }
    positionLabel.setBounds (row1.removeFromLeft (100));
    tempoLabel.setBounds (row1.removeFromLeft (30));
    tempoSlider.setBounds (row1.removeFromLeft (100));
    timeSigBox.setBounds (row1.removeFromLeft (60));

    auto row2 = r.removeFromTop (h);
    for (auto* btn : { &addAudioButton, &addMidiButton, &addClipButton, &settingsButton, &pluginsButton,
                       &mixerButton, &sidechainButton, &automationButton, &learnButton })
    {
        btn->setBounds (row2.removeFromLeft (btnW + 10).reduced (1));
    }
    row2.removeFromLeft (8);
    clickButton.setBounds (row2.removeFromLeft (btnW).reduced (1));
    clickVolumeSlider.setBounds (row2.removeFromLeft (70).reduced (1));
    countInBox.setBounds (row2.removeFromLeft (110).reduced (1));
}

void TransportBar::timerCallback()
{
    positionLabel.setText (EngineHelpers::getPositionString (edit), juce::dontSendNotification);
    syncTempoControls();
}

void TransportBar::syncTempoControls()
{
    // The playhead can sit in a different tempo/time-sig region after scrubbing
    // or during playback; keep the controls following it unless being edited.
    if (! tempoSlider.isMouseButtonDown())
        tempoSlider.setValue (transportController.getTempo(), juce::dontSendNotification);

    const auto sig = transportController.getTimeSignatureString();
    const int idx = timeSigChoices.indexOf (sig);
    if (idx >= 0 && timeSigBox.getSelectedId() != idx + 1 && ! timeSigBox.isPopupActive())
        timeSigBox.setSelectedId (idx + 1, juce::dontSendNotification);
}

void TransportBar::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateButtonStates();
}

void TransportBar::updateButtonStates()
{
    playButton.setButtonText (transportController.isPlaying() ? "Pause" : "Play");
    recordButton.setButtonText (transportController.isRecording() ? "Stop Rec" : "Rec");
    loopButton.setToggleState (transportController.isLooping(), juce::dontSendNotification);
    punchButton.setToggleState (transportController.isPunchInEnabled(), juce::dontSendNotification);
    clickButton.setToggleState (transportController.isClickEnabled(), juce::dontSendNotification);
}

void TransportBar::setLearnModeActive (bool active)
{
    learnButton.setToggleState (active, juce::dontSendNotification);
    learnButton.setColour (juce::TextButton::buttonColourId,
                           active ? juce::Colour (0xffca6702) : findColour (juce::TextButton::buttonColourId));
}

} // namespace skeletonhive
