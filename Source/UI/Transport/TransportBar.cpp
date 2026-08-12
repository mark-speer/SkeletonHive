#include "TransportBar.h"
#include "TransportIcons.h"
#include "UI/AppLookAndFeel.h"
#include "BinaryData.h"
#include "Engine/EngineHelpers.h"
#include "Engine/SessionArrangementBridge.h"
#include "Engine/SessionManager.h"

namespace skeletonhive
{

namespace
{
const juce::StringArray timeSigChoices { "4/4", "3/4", "6/8", "2/4", "5/4", "7/8", "12/8" };
}

TransportBar::TransportBar (te::Edit& e, TransportController& tc)
    : edit (e), transportController (tc)
{
    returnButton.setTooltip ("Return to start");
    returnButton.onClick = [this] { transportController.returnToStart(); };
    TransportIcons::setButtonImage (returnButton,
                                    BinaryData::return_to_start_svg,
                                    BinaryData::return_to_start_svgSize,
                                    {},
                                    *this);

    playButton.setTooltip ("Play/Pause");
    playButton.onClick = [this] { transportController.togglePlay(); updateButtonStates(); };

    stopButton.setTooltip ("Stop");
    stopButton.onClick = [this] { transportController.stop(); updateButtonStates(); };

    recordButton.setTooltip ("Record");
    recordButton.onClick = [this] { transportController.toggleRecord(); updateButtonStates(); };

    loopButton.setClickingTogglesState (true);
    loopButton.setTooltip ("Loop playback");
    loopButton.onClick = [this] { transportController.setLooping (loopButton.getToggleState()); };

    punchButton.setClickingTogglesState (true);
    punchButton.setTooltip ("Punch recording: only record inside the loop brace");
    punchButton.onClick = [this] { transportController.enablePunchIn (punchButton.getToggleState()); };

    takesButton.setClickingTogglesState (true);
    takesButton.setTooltip ("Create takes on loop record (requires loop brace >= 2 seconds; disables MIDI merge)");
    takesButton.setToggleState (EngineHelpers::isCreateTakesOnLoopEnabled (edit), juce::dontSendNotification);
    takesButton.onClick = [this]
    {
        EngineHelpers::setCreateTakesOnLoopEnabled (edit, takesButton.getToggleState());
    };

    clickButton.setClickingTogglesState (true);
    clickButton.setTooltip ("Metronome click (volume slider to the right)");
    clickButton.setToggleState (transportController.isClickEnabled(), juce::dontSendNotification);
    clickButton.onClick = [this] { transportController.setClickEnabled (clickButton.getToggleState()); };

    const auto loopAccent = AppColours::accentLoop (AppLookAndFeel::getCurrentTheme());
    TransportIcons::setButtonImages (loopButton,
                                       BinaryData::loop_svg, BinaryData::loop_svgSize, {},
                                       BinaryData::loop_active_svg, BinaryData::loop_active_svgSize, loopAccent,
                                       *this);
    TransportIcons::setButtonImages (punchButton,
                                     BinaryData::punch_svg, BinaryData::punch_svgSize, {},
                                     BinaryData::punch_active_svg, BinaryData::punch_active_svgSize, {},
                                     *this);
    TransportIcons::setButtonImages (takesButton,
                                     BinaryData::takes_svg, BinaryData::takes_svgSize, {},
                                     BinaryData::takes_active_svg, BinaryData::takes_active_svgSize, {},
                                     *this);
    TransportIcons::setButtonImages (clickButton,
                                     BinaryData::metronome_svg, BinaryData::metronome_svgSize, {},
                                     BinaryData::metronome_active_svg, BinaryData::metronome_active_svgSize, {},
                                     *this);
    TransportIcons::setButtonImage (stopButton,
                                    BinaryData::stop_svg,
                                    BinaryData::stop_svgSize,
                                    {},
                                    *this);

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

    viewLabel.setJustificationType (juce::Justification::centred);
    viewLabel.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));

    launchQuantizeBox.addItem ("Quantize: None", (int) LaunchQuantization::none + 1);
    launchQuantizeBox.addItem ("Quantize: 1 Bar", (int) LaunchQuantization::bar + 1);
    launchQuantizeBox.addItem ("Quantize: 1/2", (int) LaunchQuantization::halfBar + 1);
    launchQuantizeBox.addItem ("Quantize: 1/4", (int) LaunchQuantization::beat + 1);
    launchQuantizeBox.addItem ("Quantize: 1/8", (int) LaunchQuantization::halfBeat + 1);
    launchQuantizeBox.addItem ("Quantize: 1/16", (int) LaunchQuantization::eighthBeat + 1);
    launchQuantizeBox.onChange = [this]
    {
        if (sessionManager != nullptr)
            sessionManager->setLaunchQuantization (static_cast<LaunchQuantization> (launchQuantizeBox.getSelectedId() - 1));
    };

    sceneLaunchModeBox.addItem ("Scene: Stop Others", (int) SceneLaunchMode::stopOthers + 1);
    sceneLaunchModeBox.addItem ("Scene: Additive", (int) SceneLaunchMode::additive + 1);
    sceneLaunchModeBox.onChange = [this]
    {
        if (sessionManager != nullptr)
            sessionManager->setSceneLaunchMode (static_cast<SceneLaunchMode> (sceneLaunchModeBox.getSelectedId() - 1));
    };

    recordToArrangementButton.setClickingTogglesState (true);
    recordToArrangementButton.setTooltip ("Record session launches into the arrangement timeline");
    recordToArrangementButton.onClick = [this]
    {
        if (onToggleRecordToArrangement)
            onToggleRecordToArrangement();
    };

    captureButton.setTooltip ("Capture playing session clips into the arrangement (Shift+C)");
    captureButton.onClick = [this]
    {
        if (onCaptureSession)
            onCaptureSession();
    };

    performanceButton.setClickingTogglesState (true);
    performanceButton.setTooltip ("Show rack macro performance panel (Alt+P)");
    performanceButton.onClick = [this]
    {
        if (onTogglePerformancePanel)
            onTogglePerformancePanel();
    };

    writePositionLabel.setJustificationType (juce::Justification::centredLeft);
    writePositionLabel.setFont (juce::FontOptions (10.0f));

    addAudioButton.onClick = [this] { if (onAddAudioTrack) onAddAudioTrack(); };
    addMidiButton.onClick = [this] { if (onAddMidiTrack) onAddMidiTrack(); };
    addClipButton.onClick = [this] { if (onAddMidiClip) onAddMidiClip(); };
    mixerButton.onClick = [this] { if (onToggleMixer) onToggleMixer(); };
    sidechainButton.onClick = [this] { if (onToggleSidechain) onToggleSidechain(); };

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
    addAndMakeVisible (takesButton);
    addAndMakeVisible (addAudioButton);
    addAndMakeVisible (addMidiButton);
    addAndMakeVisible (addClipButton);
    addAndMakeVisible (mixerButton);
    addAndMakeVisible (sidechainButton);
    addAndMakeVisible (learnButton);
    addAndMakeVisible (clickButton);
    addAndMakeVisible (clickVolumeSlider);
    addAndMakeVisible (countInBox);
    addAndMakeVisible (positionLabel);
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);
    addAndMakeVisible (timeSigBox);
    addAndMakeVisible (viewLabel);
    addAndMakeVisible (launchQuantizeBox);
    addAndMakeVisible (sceneLaunchModeBox);
    addAndMakeVisible (recordToArrangementButton);
    addAndMakeVisible (captureButton);
    addAndMakeVisible (performanceButton);
    addAndMakeVisible (writePositionLabel);

    updateSessionControlsVisibility();

    edit.getTransport().addChangeListener (this);
    EngineHelpers::setCreateTakesOnLoopEnabled (edit, takesButton.getToggleState());
    startTimerHz (15);
    updateButtonStates();
}

TransportBar::~TransportBar()
{
    if (sessionArrangementBridge != nullptr)
        sessionArrangementBridge->removeChangeListener (this);

    edit.getTransport().removeChangeListener (this);
}

void TransportBar::resized()
{
    auto r = getLocalBounds().reduced (4);
    const int h = 24;
    const int iconBtnW = 28;
    const int btnW = 52;
    const int groupGap = 6;

    auto row1 = r.removeFromTop (h);
    for (auto* btn : { &returnButton, &playButton, &stopButton, &recordButton })
        btn->setBounds (row1.removeFromLeft (iconBtnW).reduced (1));

    row1.removeFromLeft (groupGap);

    for (auto* btn : { &loopButton, &punchButton, &takesButton })
        btn->setBounds (row1.removeFromLeft (iconBtnW).reduced (1));

    row1.removeFromLeft (groupGap);
    clickButton.setBounds (row1.removeFromLeft (iconBtnW).reduced (1));

    row1.removeFromLeft (groupGap);
    positionLabel.setBounds (row1.removeFromLeft (100));
    tempoLabel.setBounds (row1.removeFromLeft (30));
    tempoSlider.setBounds (row1.removeFromLeft (100));
    timeSigBox.setBounds (row1.removeFromLeft (60));

    auto row2 = r.removeFromTop (h);
    for (auto* btn : { &addAudioButton, &addMidiButton, &addClipButton,
                       &mixerButton, &sidechainButton, &learnButton })
    {
        btn->setBounds (row2.removeFromLeft (btnW + 10).reduced (1));
    }
    row2.removeFromLeft (8);
    clickVolumeSlider.setBounds (row2.removeFromLeft (70).reduced (1));
    countInBox.setBounds (row2.removeFromLeft (110).reduced (1));

    if (sessionViewActive)
    {
        viewLabel.setBounds (row2.removeFromLeft (90).reduced (1));
        launchQuantizeBox.setBounds (row2.removeFromLeft (130).reduced (1));
        sceneLaunchModeBox.setBounds (row2.removeFromLeft (130).reduced (1));
        recordToArrangementButton.setBounds (row2.removeFromLeft (btnW).reduced (1));
        captureButton.setBounds (row2.removeFromLeft (btnW + 8).reduced (1));
        performanceButton.setBounds (row2.removeFromLeft (btnW).reduced (1));
        writePositionLabel.setBounds (row2.removeFromLeft (120).reduced (1));
    }
}

void TransportBar::setSessionManager (SessionManager* manager)
{
    sessionManager = manager;
    syncSessionControls();
}

void TransportBar::setSessionArrangementBridge (SessionArrangementBridge* bridge)
{
    if (sessionArrangementBridge != bridge)
    {
        if (sessionArrangementBridge != nullptr)
            sessionArrangementBridge->removeChangeListener (this);

        sessionArrangementBridge = bridge;

        if (sessionArrangementBridge != nullptr)
            sessionArrangementBridge->addChangeListener (this);
    }

    syncSessionCaptureControls();
}

void TransportBar::setSessionViewActive (bool active)
{
    sessionViewActive = active;
    viewLabel.setText (active ? "Session" : "Arrangement", juce::dontSendNotification);
    updateSessionControlsVisibility();
    resized();
}

void TransportBar::syncSessionControls()
{
    if (sessionManager == nullptr)
        return;

    launchQuantizeBox.setSelectedId ((int) sessionManager->getLaunchQuantization() + 1, juce::dontSendNotification);
    sceneLaunchModeBox.setSelectedId ((int) sessionManager->getSceneLaunchMode() + 1, juce::dontSendNotification);
}

void TransportBar::updateSessionControlsVisibility()
{
    launchQuantizeBox.setVisible (sessionViewActive);
    sceneLaunchModeBox.setVisible (sessionViewActive);
    recordToArrangementButton.setVisible (sessionViewActive);
    captureButton.setVisible (sessionViewActive);
    performanceButton.setVisible (sessionViewActive);
    writePositionLabel.setVisible (sessionViewActive);
    viewLabel.setVisible (true);
}

void TransportBar::setPerformancePanelVisible (bool visible)
{
    performanceButton.setToggleState (visible, juce::dontSendNotification);
}

void TransportBar::syncSessionCaptureControls()
{
    if (sessionArrangementBridge == nullptr)
    {
        recordToArrangementButton.setToggleState (false, juce::dontSendNotification);
        writePositionLabel.setText ("Write: 0.0.0", juce::dontSendNotification);
        return;
    }

    recordToArrangementButton.setToggleState (sessionArrangementBridge->isRecordToArrangementEnabled(),
                                              juce::dontSendNotification);
    recordToArrangementButton.setColour (juce::TextButton::buttonColourId,
                                         sessionArrangementBridge->isRecordToArrangementEnabled()
                                             ? juce::Colour (0xffef476f)
                                             : findColour (juce::TextButton::buttonColourId));

    const auto writePos = sessionArrangementBridge->getArrangementWritePosition();
    writePositionLabel.setText ("Write: " + EngineHelpers::timeToTimecodeString (writePos.inSeconds()),
                                juce::dontSendNotification);
}

void TransportBar::timerCallback()
{
    positionLabel.setText (EngineHelpers::getPositionString (edit), juce::dontSendNotification);
    syncTempoControls();
    syncSessionCaptureControls();
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

void TransportBar::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == sessionArrangementBridge)
        syncSessionCaptureControls();

    updateButtonStates();
}

void TransportBar::updateButtonStates()
{
    TransportIcons::updatePlayButton (playButton, transportController.isPlaying(), *this);
    TransportIcons::updateRecordButton (recordButton, transportController.isRecording(), *this);

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
