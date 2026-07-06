#pragma once

#include "Engine/TransportController.h"

namespace skeletonhive
{

class SessionManager;
class SessionArrangementBridge;

class TransportBar : public juce::Component,
                     private juce::ChangeListener,
                     private juce::Timer
{
public:
    TransportBar (te::Edit& edit, TransportController& transport);
    ~TransportBar() override;

    std::function<void()> onNewProject;
    std::function<void()> onOpenProject;
    std::function<void()> onSaveProject;
    std::function<void()> onSaveProjectAs;
    std::function<void()> onCollectAllAndSave;
    std::function<void()> onExport;
    std::function<void()> onImportAudio;
    std::function<void()> onAddAudioTrack;
    std::function<void()> onAddMidiTrack;
    std::function<void()> onAddMidiClip;
    std::function<void()> onAudioSettings;
    std::function<void()> onScanPlugins;
    std::function<void()> onToggleMixer;
    std::function<void()> onToggleSidechain;
    std::function<void()> onToggleAutomation;
    std::function<void()> onShowPreferences;
    std::function<void()> onToggleMidiLearn;
    std::function<void()> onToggleBrowser;

    void setBrowserToggleState (bool visible);

    void setLearnModeActive (bool active);

    void setSessionManager (SessionManager* manager);
    void setSessionArrangementBridge (SessionArrangementBridge* bridge);
    void setSessionViewActive (bool active);

    std::function<void()> onToggleRecordToArrangement;
    std::function<void()> onCaptureSession;

private:
    void syncSessionControls();
    void syncSessionCaptureControls();
    void updateSessionControlsVisibility();
    void resized() override;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateButtonStates();
    void syncTempoControls();

    te::Edit& edit;
    TransportController& transportController;
    SessionManager* sessionManager = nullptr;
    SessionArrangementBridge* sessionArrangementBridge = nullptr;
    bool sessionViewActive = false;

    juce::Label viewLabel { {}, "Arrangement" };
    juce::ComboBox launchQuantizeBox;
    juce::ComboBox sceneLaunchModeBox;
    juce::TextButton recordToArrangementButton { "Rec>" };
    juce::TextButton captureButton { "Capture" };
    juce::Label writePositionLabel { {}, "Write: 0.0.0" };

    juce::TextButton returnButton { "<<" }, playButton { "Play" }, stopButton { "Stop" },
        recordButton { "Rec" }, loopButton { "Loop" }, punchButton { "Punch" },
        clickButton { "Click" }, takesButton { "Takes" };
    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" },
        saveAsButton { "Save As" }, collectButton { "Collect" }, exportButton { "Export" },
        importButton { "Import" }, addAudioButton { "+ Audio" }, addMidiButton { "+ MIDI" },
        addClipButton { "+ Clip" },         settingsButton { "Prefs" }, pluginsButton { "Plugins" },
        browserButton { "Browser" },
        mixerButton { "Mixer" }, sidechainButton { "Sidechain" }, automationButton { "Auto" },
        learnButton { "Learn" };
    juce::Label positionLabel { {}, "00:00:00.000" };
    juce::Slider tempoSlider;
    juce::Label tempoLabel { {}, "BPM" };
    juce::ComboBox timeSigBox;
    juce::Slider clickVolumeSlider;
    juce::ComboBox countInBox;
};

} // namespace skeletonhive
