#pragma once

#include "Engine/TransportController.h"

namespace arrange
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
    std::function<void()> onImportAudio;
    std::function<void()> onAddAudioTrack;
    std::function<void()> onAddMidiTrack;
    std::function<void()> onAddMidiClip;
    std::function<void()> onAudioSettings;
    std::function<void()> onScanPlugins;
    std::function<void()> onToggleMixer;

private:
    void resized() override;
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void updateButtonStates();

    te::Edit& edit;
    TransportController& transportController;

    juce::TextButton returnButton { "<<" }, playButton { "Play" }, stopButton { "Stop" },
        recordButton { "Rec" }, loopButton { "Loop" }, punchButton { "Punch" };
    juce::TextButton newButton { "New" }, openButton { "Open" }, saveButton { "Save" },
        importButton { "Import" }, addAudioButton { "+ Audio" }, addMidiButton { "+ MIDI" },
        addClipButton { "+ Clip" }, settingsButton { "Audio" }, pluginsButton { "Plugins" },
        mixerButton { "Mixer" };
    juce::Label positionLabel { {}, "00:00:00.000" };
    juce::Slider tempoSlider;
    juce::Label tempoLabel { {}, "BPM" };
    juce::ComboBox timeSigBox;
};

} // namespace arrange
