#pragma once

#include "Engine/TransportController.h"

namespace skeletonhive
{

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

    void setLearnModeActive (bool active);

private:
    void resized() override;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateButtonStates();
    void syncTempoControls();

    te::Edit& edit;
    TransportController& transportController;

    juce::TextButton returnButton { "<<" }, playButton { "Play" }, stopButton { "Stop" },
        recordButton { "Rec" }, loopButton { "Loop" }, punchButton { "Punch" },
        clickButton { "Click" };
    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" },
        saveAsButton { "Save As" }, exportButton { "Export" },
        importButton { "Import" }, addAudioButton { "+ Audio" }, addMidiButton { "+ MIDI" },
        addClipButton { "+ Clip" },         settingsButton { "Prefs" }, pluginsButton { "Plugins" },
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
