#pragma once



#include "SkeletonHiveApplication.h"

#include "UI/Transport/TransportBar.h"

#include "UI/Arrangement/TimelineComponent.h"

#include "UI/Mixer/MixerPanel.h"

#include "UI/Plugins/PluginTrayComponent.h"

#include "UI/Routing/SidechainMatrixPanel.h"

#include "Engine/PluginStateManager.h"

#include "Engine/SidechainRouting.h"

#include "Engine/AppCommands.h"

#include "UI/Midi/PianoRollEditor.h"

#include "UI/Automation/AutomationPanel.h"

#include "UI/Arrangement/ClipInspectorPanel.h"

#include "UI/Browser/BrowserPanel.h"

#include "UI/Detail/DetailPanelStack.h"

#include "Engine/ContentLibraryManager.h"

#include "Engine/ClipLibraryManager.h"

#include "Engine/GroovePoolManager.h"

#include "Engine/PreviewPlayer.h"

#include "Engine/EngineHelpers.h"

#include "Engine/UiTelemetryHub.h"

#include "Engine/SessionManager.h"

#include "Engine/SessionArrangementBridge.h"

#include "Engine/SessionMidiMapper.h"

#include "UI/Session/SessionViewComponent.h"

#include "UI/Session/PerformanceMacroPanel.h"



namespace skeletonhive

{



class MainContentComponent : public juce::Component,

                             public juce::DragAndDropContainer,

                             public juce::ApplicationCommandTarget,

                             private juce::MenuBarModel,

                             private juce::KeyListener,

                             private juce::Timer,

                             private juce::ChangeListener

{

public:

    MainContentComponent (SkeletonHiveApplication& app);

    ~MainContentComponent() override;



    void prepareForShutdown();

    bool confirmQuit (juce::Component* parent);



    juce::ApplicationCommandManager& getCommandManager() { return commandManager; }



private:

    void resized() override;

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    void timerCallback() override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;



    ApplicationCommandTarget* getNextCommandTarget() override;

    void getAllCommands (juce::Array<juce::CommandID>& commands) override;

    void getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result) override;

    bool perform (const InvocationInfo& info) override;



    juce::StringArray getMenuBarNames() override;

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;

    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;



    void createDefaultProject();

    void releaseEditUI();

    void rebuildEditUI();

    void updateWindowTitle();

    void updateLearnStatus();

    void handleNewProject();

    void handleOpenProject();

    void handleSaveProject();

    void handleSaveProjectAs();

    void handleCollectAllAndSave();

    void handleExportClipToLibrary (te::Clip& clip);

    void handleExport();

    void handleImportAudio();

    void handleAddAudioTrack();

    void handleAddMidiTrack();

    void handleAddMidiClip();

    void handleClipDoubleClick (te::Clip& clip);

    void handleAddPlugin (te::Track& track);

    void showPianoRoll (te::MidiClip& clip);

    void showPreferences();

    void toggleMixer();

    void toggleSidechainPanel();

    void showSidechainPanelForPlugin (te::Plugin* plugin);

    void toggleAutomationPanel();

    void toggleMidiLearn();

    void toggleBrowserPanel();

    void showPluginsBrowser();

    void syncRoamingFocus();

    void saveGroovePool();

    void toggleMainView();

    void updateMainViewVisibility();

    void togglePerformancePanel();



    te::Track* resolveFocusedTrack() const;

    PluginTrayComponent* getPluginTray() const;



    PianoRollEditor* getActivePianoRollEditor() const;

    bool isPluginTrayContext() const;



    SkeletonHiveApplication& application;

    te::Engine& engine;

    ProjectManager& projectManager;

    AppSettings& appSettings;

    MidiLearnController& midiLearnController;



    juce::ApplicationCommandManager commandManager;

    juce::MenuBarComponent menuBarComponent;

    std::unique_ptr<TransportController> transportController;

    std::unique_ptr<SessionManager> sessionManager;

    std::unique_ptr<SessionArrangementBridge> sessionArrangementBridge;

    std::unique_ptr<SessionMidiMapper> sessionMidiMapper;

    std::unique_ptr<PluginScanner> pluginScanner;

    std::unique_ptr<PluginStateManager> pluginStateManager;

    std::unique_ptr<ContentLibraryManager> contentLibraryManager;

    std::unique_ptr<ClipLibraryManager> clipLibraryManager;

    std::unique_ptr<GroovePoolManager> groovePoolManager;

    std::unique_ptr<PreviewPlayer> previewPlayer;

    std::unique_ptr<UiTelemetryHub> telemetryHub;



    std::unique_ptr<TransportBar> transportBar;

    std::unique_ptr<TimelineComponent> timeline;

    std::unique_ptr<SessionViewComponent> sessionView;

    std::unique_ptr<PerformanceMacroPanel> performanceMacroPanel;

    std::unique_ptr<MixerPanel> mixerPanel;

    std::unique_ptr<DetailPanelStack> detailPanelStack;

    std::unique_ptr<SidechainMatrixPanel> sidechainPanel;



    std::unique_ptr<juce::DocumentWindow> pianoRollWindow;

    PianoRollEditor* pianoRollEditor = nullptr;

    std::unique_ptr<AutomationPanel> automationPanel;

    std::unique_ptr<BrowserPanel> browserPanel;



    juce::Label learnStatusLabel;



    bool mixerVisible = false;

    bool sidechainVisible = false;

    bool automationVisible = false;

    bool browserVisible = true;

    bool performancePanelVisible = false;

    te::EditItemID sessionFocusedTrackId;
    int sessionFocusedSceneIndex = 0;

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

