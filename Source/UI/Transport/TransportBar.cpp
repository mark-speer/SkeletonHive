#include "TransportBar.h"
#include "Engine/EngineHelpers.h"

namespace skeletonhive
{

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
    punchButton.onClick = [this] { transportController.enablePunchIn (punchButton.getToggleState()); };

    newButton.onClick = [this] { if (onNewProject) onNewProject(); };
    openButton.onClick = [this] { if (onOpenProject) onOpenProject(); };
    saveButton.onClick = [this] { if (onSaveProject) onSaveProject(); };
    importButton.onClick = [this] { if (onImportAudio) onImportAudio(); };
    addAudioButton.onClick = [this] { if (onAddAudioTrack) onAddAudioTrack(); };
    addMidiButton.onClick = [this] { if (onAddMidiTrack) onAddMidiTrack(); };
    addClipButton.onClick = [this] { if (onAddMidiClip) onAddMidiClip(); };
    settingsButton.onClick = [this] { if (onAudioSettings) onAudioSettings(); };
    pluginsButton.onClick = [this] { if (onScanPlugins) onScanPlugins(); };
    mixerButton.onClick = [this] { if (onToggleMixer) onToggleMixer(); };
    sidechainButton.onClick = [this] { if (onToggleSidechain) onToggleSidechain(); };

    tempoSlider.setRange (20.0, 300.0, 0.1);
    tempoSlider.setValue (transportController.getTempo(), juce::dontSendNotification);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 50, 20);
    tempoSlider.onValueChange = [this]
    {
        transportController.setTempo (tempoSlider.getValue());
    };

    timeSigBox.addItem ("4/4", 1);
    timeSigBox.addItem ("3/4", 2);
    timeSigBox.addItem ("6/8", 3);
    timeSigBox.setSelectedId (1, juce::dontSendNotification);
    timeSigBox.onChange = [this]
    {
        switch (timeSigBox.getSelectedId())
        {
            case 2: transportController.setTimeSignature (3, 4); break;
            case 3: transportController.setTimeSignature (6, 8); break;
            default: transportController.setTimeSignature (4, 4); break;
        }
    };

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
    addAndMakeVisible (importButton);
    addAndMakeVisible (addAudioButton);
    addAndMakeVisible (addMidiButton);
    addAndMakeVisible (addClipButton);
    addAndMakeVisible (settingsButton);
    addAndMakeVisible (pluginsButton);
    addAndMakeVisible (mixerButton);
    addAndMakeVisible (sidechainButton);
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
    for (auto* btn : { &newButton, &openButton, &saveButton, &importButton })
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
    for (auto* btn : { &addAudioButton, &addMidiButton, &addClipButton, &settingsButton, &pluginsButton, &mixerButton, &sidechainButton })
    {
        btn->setBounds (row2.removeFromLeft (btnW + 10).reduced (1));
    }
}

void TransportBar::timerCallback()
{
    positionLabel.setText (EngineHelpers::getPositionString (edit), juce::dontSendNotification);
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
}

} // namespace skeletonhive
