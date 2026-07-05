#pragma once

#include "Application.h"
#include "UI/Transport/TransportBar.h"
#include "UI/Arrangement/TimelineComponent.h"
#include "UI/Mixer/MixerPanel.h"
#include "UI/Plugins/PluginBrowser.h"
#include "UI/Midi/PianoRollEditor.h"
#include "UI/Automation/AutomationLaneComponent.h"
#include "Engine/EngineHelpers.h"

namespace arrange
{

class MainContentComponent : public juce::Component,
                             public juce::DragAndDropContainer,
                             private juce::KeyListener
{
public:
    MainContentComponent (ArrangeApplication& app);
    ~MainContentComponent() override;

    void prepareForShutdown();

private:
    void resized() override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void createDefaultProject();
    void releaseEditUI();
    void rebuildEditUI();
    void handleNewProject();
    void handleOpenProject();
    void handleSaveProject();
    void handleImportAudio();
    void handleAddAudioTrack();
    void handleAddMidiTrack();
    void handleAddMidiClip();
    void handleClipDoubleClick (te::Clip& clip);
    void handleAddPlugin (te::Track& track);
    void showPianoRoll (te::MidiClip& clip);
    void toggleMixer();
    void setupAutomationPanel (te::Track& track);

    ArrangeApplication& application;
    te::Engine& engine;
    ProjectManager& projectManager;
    std::unique_ptr<TransportController> transportController;
    std::unique_ptr<PluginScanner> pluginScanner;

    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<TimelineComponent> timeline;
    std::unique_ptr<MixerPanel> mixerPanel;
    std::unique_ptr<PluginBrowser> pluginBrowser;

    std::unique_ptr<juce::DocumentWindow> pianoRollWindow;
    juce::OwnedArray<AutomationLaneComponent> automationLanes;

    juce::TextButton automationReadButton { "Read" }, automationTouchButton { "Touch" },
        automationLatchButton { "Latch" };
    juce::Component automationPanel;
    bool mixerVisible = false;
    AutomationMode automationMode = AutomationMode::read;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (ArrangeApplication& app);
    ~MainWindow() override;

    void closeButtonPressed() override;
    void prepareForShutdown();

private:
    ArrangeApplication& application;
};

} // namespace arrange
