#pragma once

#include "SkeletonHiveApplication.h"
#include "UI/Transport/TransportBar.h"
#include "UI/Arrangement/TimelineComponent.h"
#include "UI/Mixer/MixerPanel.h"
#include "UI/Plugins/PluginBrowser.h"
#include "UI/Plugins/PluginTrayComponent.h"
#include "UI/Routing/SidechainMatrixPanel.h"
#include "Engine/PluginStateManager.h"
#include "Engine/SidechainRouting.h"
#include "UI/Midi/PianoRollEditor.h"
#include "UI/Automation/AutomationPanel.h"
#include "Engine/EngineHelpers.h"
#include "Engine/UiTelemetryHub.h"

namespace skeletonhive
{

class MainContentComponent : public juce::Component,
                             public juce::DragAndDropContainer,
                             private juce::KeyListener,
                             private juce::Timer
{
public:
    MainContentComponent (SkeletonHiveApplication& app);
    ~MainContentComponent() override;

    void prepareForShutdown();
    bool confirmQuit (juce::Component* parent);

private:
    void resized() override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void timerCallback() override;

    void createDefaultProject();
    void releaseEditUI();
    void rebuildEditUI();
    void updateWindowTitle();
    void handleNewProject();
    void handleOpenProject();
    void handleSaveProject();
    void handleSaveProjectAs();
    void handleExport();
    void handleImportAudio();
    void handleAddAudioTrack();
    void handleAddMidiTrack();
    void handleAddMidiClip();
    void handleClipDoubleClick (te::Clip& clip);
    void handleAddPlugin (te::Track& track);
    void showPianoRoll (te::MidiClip& clip);
    void toggleMixer();
    void toggleSidechainPanel();
    void showSidechainPanelForPlugin (te::Plugin* plugin);
    void toggleAutomationPanel();

    SkeletonHiveApplication& application;
    te::Engine& engine;
    ProjectManager& projectManager;
    std::unique_ptr<TransportController> transportController;
    std::unique_ptr<PluginScanner> pluginScanner;
    std::unique_ptr<PluginStateManager> pluginStateManager;
    std::unique_ptr<UiTelemetryHub> telemetryHub;

    std::unique_ptr<TransportBar> transportBar;
    std::unique_ptr<TimelineComponent> timeline;
    std::unique_ptr<MixerPanel> mixerPanel;
    std::unique_ptr<PluginBrowser> pluginBrowser;
    std::unique_ptr<PluginTrayComponent> pluginTray;
    std::unique_ptr<SidechainMatrixPanel> sidechainPanel;

    std::unique_ptr<juce::DocumentWindow> pianoRollWindow;
    std::unique_ptr<AutomationPanel> automationPanel;

    bool mixerVisible = false;
    bool sidechainVisible = false;
    bool automationVisible = false;
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow (SkeletonHiveApplication& app);
    ~MainWindow() override;

    void closeButtonPressed() override;
    void prepareForShutdown();
    bool confirmClose();

private:
    SkeletonHiveApplication& application;
};

} // namespace skeletonhive
