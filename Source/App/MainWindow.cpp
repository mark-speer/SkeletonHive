#include "MainWindow.h"
#include "TracktionCommon.h"
#include "Engine/AppSettings.h"
#include "Engine/ExportManager.h"
#include "UI/Settings/PreferencesDialog.h"
#include "UI/AppLookAndFeel.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace skeletonhive
{

namespace
{
class PianoRollWindow : public juce::DocumentWindow
{
public:
    PianoRollWindow (const juce::String& name, juce::Colour backgroundColour, std::function<void()> closeCallback)
        : DocumentWindow (name, backgroundColour, DocumentWindow::closeButton),
          onClose (std::move (closeCallback))
    {
    }

    void closeButtonPressed() override
    {
        setVisible (false);

        if (onClose != nullptr)
            juce::MessageManager::callAsync (onClose);
    }

private:
    std::function<void()> onClose;
};
} // namespace

MainContentComponent::MainContentComponent (SkeletonHiveApplication& app)
    : application (app),
      engine (app.getEngine()),
      projectManager (app.getProjectManager()),
      appSettings (app.getAppSettings()),
      midiLearnController (app.getMidiLearnController())
{
    AppCommands::registerAllCommands (commandManager);
    AppCommands::registerDefaultKeyMappings (*commandManager.getKeyMappings());
    appSettings.loadKeyMappings (commandManager);
    commandManager.setFirstCommandTarget (this);

    pluginScanner = std::make_unique<PluginScanner> (engine);
    pluginStateManager = std::make_unique<PluginStateManager>();
    appSettings.ensureDefaultSampleLibraryPaths();
    contentLibraryManager = std::make_unique<ContentLibraryManager> (engine, appSettings);
    clipLibraryManager = std::make_unique<ClipLibraryManager> (engine);
    groovePoolManager = std::make_unique<GroovePoolManager>();
    previewPlayer = std::make_unique<PreviewPlayer> (engine);
    telemetryHub = std::make_unique<UiTelemetryHub>();

    learnStatusLabel.setJustificationType (juce::Justification::centredLeft);
    learnStatusLabel.setFont (juce::FontOptions (11.0f));
    addAndMakeVisible (learnStatusLabel);

    midiLearnController.onStatusChanged = [this] { updateLearnStatus(); };
    appSettings.addChangeListener (this);

    createDefaultProject();
    rebuildEditUI();

    addKeyListener (this);
    setWantsKeyboardFocus (true);

    projectManager.enableAutosave (appSettings.getAutosaveIntervalSeconds());
    updateLearnStatus();
    startTimerHz (2);
    setSize (1200, 800);
}

MainContentComponent::~MainContentComponent()
{
    appSettings.removeChangeListener (this);
    appSettings.saveKeyMappings (commandManager);
}

void MainContentComponent::prepareForShutdown()
{
    pianoRollWindow = nullptr;
}

void MainContentComponent::createDefaultProject()
{
    auto dir = appSettings.getDefaultProjectFolder();
    dir.createDirectory();
    auto projectFile = dir.getNonexistentChildFile ("Untitled", ".tracktionedit");
    projectManager.createNewProject (projectFile, this);

    if (auto* edit = projectManager.getEdit())
    {
        edit->getTransport().looping = true;
        edit->getTransport().setLoopRange ({ 0s, te::TimeDuration::fromSeconds (30.0) });
    }
}

// Destroys every UI panel that holds references into the current Edit. Must be
// called BEFORE the Edit is replaced (new/open project) so panel destructors
// can still unregister their listeners from a live Edit.
void MainContentComponent::releaseEditUI()
{
    pianoRollWindow = nullptr;
    pianoRollEditor = nullptr;
    automationPanel = nullptr;
    detailPanelStack = nullptr;
    browserPanel = nullptr;

    sidechainPanel = nullptr;
    SidechainRouting::openMatrixForPlugin = nullptr;
    mixerPanel = nullptr;
    timeline = nullptr;
    sessionView = nullptr;
    sessionManager = nullptr;
    sessionMidiMapper = nullptr;
    sessionArrangementBridge = nullptr;
    performanceMacroPanel = nullptr;
    transportBar = nullptr;
    transportController = nullptr;
}

// Rebuilds every UI panel against the current Edit. Call after the Edit has
// been replaced so no panel keeps a stale te::Edit& into a destroyed object.
void MainContentComponent::rebuildEditUI()
{
    auto* edit = projectManager.getEdit();
    jassert (edit != nullptr);

    transportController = std::make_unique<TransportController> (*edit);
    transportBar = std::make_unique<TransportBar> (*edit, *transportController);
    timeline = std::make_unique<TimelineComponent> (*edit, projectManager.getSelectionManager(),
                                                    projectManager.getInsertPoint(),
                                                    telemetryHub.get());
    sessionManager = std::make_unique<SessionManager> (*edit, timeline->getEditViewState(), *transportController);
    sessionArrangementBridge = std::make_unique<SessionArrangementBridge> (*edit, timeline->getEditViewState(),
                                                                          *sessionManager, *transportController);
    sessionManager->setArrangementBridge (sessionArrangementBridge.get());
    sessionMidiMapper = std::make_unique<SessionMidiMapper> (*edit, timeline->getEditViewState(), *sessionManager);
    sessionView = std::make_unique<SessionViewComponent> (*sessionManager, *sessionMidiMapper,
                                                          timeline->getEditViewState(), clipLibraryManager.get());
    performanceMacroPanel = std::make_unique<PerformanceMacroPanel> (*edit, timeline->getEditViewState());
    mixerPanel = std::make_unique<MixerPanel> (*edit, telemetryHub.get());
    sidechainPanel = std::make_unique<SidechainMatrixPanel> (*edit);
    automationPanel = std::make_unique<AutomationPanel> (*edit, timeline->getEditViewState());

    auto pluginTray = std::make_unique<PluginTrayComponent> (timeline->getEditViewState(), *pluginStateManager, *pluginScanner);
    auto clipInspectorPanel = std::make_unique<ClipInspectorPanel> (*edit, projectManager.getSelectionManager());
    detailPanelStack = std::make_unique<DetailPanelStack> (std::move (pluginTray), std::move (clipInspectorPanel));

    groovePoolManager->loadForProject (projectManager.getCurrentProjectFile());
    timeline->setGroovePool (groovePoolManager.get());

    browserPanel = std::make_unique<BrowserPanel> (*contentLibraryManager, *clipLibraryManager, *groovePoolManager,
                                                   *previewPlayer, timeline->getEditViewState().waveformCache,
                                                   *pluginScanner, *pluginStateManager, engine, *edit,
                                                   projectManager.getSelectionManager());
    contentLibraryManager->setProjectFolder (projectManager.getCurrentProjectFile());
    contentLibraryManager->rescanAll();

    SidechainRouting::openMatrixForPlugin = [this] (te::Plugin* plugin)
    {
        showSidechainPanelForPlugin (plugin);
    };

    transportBar->onNewProject = [this] { handleNewProject(); };
    transportBar->onOpenProject = [this] { handleOpenProject(); };
    transportBar->onSaveProject = [this] { handleSaveProject(); };
    transportBar->onSaveProjectAs = [this] { handleSaveProjectAs(); };
    transportBar->onCollectAllAndSave = [this] { handleCollectAllAndSave(); };
    transportBar->onExport = [this] { handleExport(); };
    transportBar->onImportAudio = [this] { handleImportAudio(); };
    transportBar->onAddAudioTrack = [this] { handleAddAudioTrack(); };
    transportBar->onAddMidiTrack = [this] { handleAddMidiTrack(); };
    transportBar->onAddMidiClip = [this] { handleAddMidiClip(); };
    transportBar->onAudioSettings = [this] { showPreferences(); };
    transportBar->onShowPreferences = [this] { showPreferences(); };
    transportBar->onToggleMidiLearn = [this] { toggleMidiLearn(); };
    transportBar->onScanPlugins = [this] { showPluginsBrowser(); };
    transportBar->onToggleMixer = [this] { toggleMixer(); };
    transportBar->onToggleSidechain = [this] { toggleSidechainPanel(); };
    transportBar->onToggleAutomation = [this] { toggleAutomationPanel(); };
    transportBar->onToggleBrowser = [this] { toggleBrowserPanel(); };

    timeline->onClipDoubleClick = [this] (te::Clip& c) { handleClipDoubleClick (c); };
    timeline->onAddPlugin = [this] (te::Track& t) { handleAddPlugin (t); };
    timeline->onClipSelectionChanged = [this] { syncRoamingFocus(); };
    timeline->onShowClipProperties = [this] { syncRoamingFocus(); };
    timeline->onSampleInserted = [this] (const juce::File& file, te::Clip* clip)
    {
        juce::ignoreUnused (clip);
        contentLibraryManager->recordRecentUse (file);
    };
    timeline->onExportClipToLibrary = [this] (te::Clip& clip) { handleExportClipToLibrary (clip); };
    timeline->instantiateClipPreset = [this] (te::ClipTrack& track, te::TimePosition start, const juce::File& presetFile)
    {
        if (auto* clip = clipLibraryManager->instantiateClip (track, start, presetFile))
        {
            timeline->rebuildTracks();
            projectManager.getSelectionManager().selectOnly (clip);
            return clip;
        }

        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Clip Preset",
                                                "Could not instantiate the clip preset.");
        return (te::Clip*) nullptr;
    };
    timeline->onTrackSelected = [this] (te::Track&) { syncRoamingFocus(); };
    timeline->createPlugin = [this] (const juce::PluginDescription& desc)
    {
        if (auto* edit = projectManager.getEdit())
            return pluginScanner->createPlugin (desc, *edit);
        return te::Plugin::Ptr {};
    };

    sessionView->onTrackSelected = [this] (te::EditItemID trackId)
    {
        sessionFocusedTrackId = trackId;
        syncRoamingFocus();
    };

    sessionView->onSlotFocused = [this] (te::EditItemID trackId, int sceneIndex)
    {
        sessionFocusedTrackId = trackId;
        sessionFocusedSceneIndex = sceneIndex;
        if (performanceMacroPanel != nullptr)
            performanceMacroPanel->setFocusedTrack (trackId);
        syncRoamingFocus();
    };

    sessionView->onCommitLoopToArrangement = [this] (te::EditItemID trackId, int sceneIndex)
    {
        if (sessionArrangementBridge == nullptr)
            return;

        sessionArrangementBridge->duplicateLoopToArrangement (trackId, sceneIndex);

        if (timeline != nullptr)
            timeline->rebuildTracks();
    };

    transportBar->setSessionManager (sessionManager.get());
    transportBar->setSessionArrangementBridge (sessionArrangementBridge.get());
    transportBar->setSessionViewActive (timeline->getEditViewState().getMainView() == MainView::session);

    transportBar->onToggleRecordToArrangement = [this]
    {
        if (sessionArrangementBridge == nullptr)
            return;

        sessionArrangementBridge->setRecordToArrangementEnabled (! sessionArrangementBridge->isRecordToArrangementEnabled());
    };

    transportBar->onCaptureSession = [this]
    {
        if (sessionArrangementBridge == nullptr)
            return;

        sessionArrangementBridge->captureAndInsert();

        if (timeline != nullptr)
            timeline->rebuildTracks();
    };

    transportBar->onTogglePerformancePanel = [this] { togglePerformancePanel(); };

    sessionMidiMapper->onStatusChanged = [this] { updateLearnStatus(); };

    if (performanceMacroPanel != nullptr)
    {
        performanceMacroPanel->setVisible (false);
        addAndMakeVisible (*performanceMacroPanel);
    }

    if (auto* tray = getPluginTray())
    {
        tray->setCreatePlugin ([this] (const juce::PluginDescription& desc)
        {
            if (auto* edit = projectManager.getEdit())
                return pluginScanner->createPlugin (desc, *edit);
            return te::Plugin::Ptr {};
        });
        tray->setOnAddPlugin ([this] (te::Track& t) { handleAddPlugin (t); });
    }

    addAndMakeVisible (*transportBar);
    addAndMakeVisible (*timeline);
    addAndMakeVisible (*sessionView);
    addAndMakeVisible (*detailPanelStack);

    if (browserVisible)
        addAndMakeVisible (*browserPanel);

    transportBar->setBrowserToggleState (browserVisible);

    if (mixerVisible)
        addAndMakeVisible (*mixerPanel);

    if (sidechainVisible)
        addAndMakeVisible (*sidechainPanel);

    if (automationVisible)
        addAndMakeVisible (*automationPanel);

    updateLearnStatus();
    syncRoamingFocus();
    updateMainViewVisibility();
    resized();
    updateWindowTitle();
}

void MainContentComponent::updateWindowTitle()
{
    if (auto* window = findParentComponentOfClass<MainWindow>())
        window->setName (projectManager.getWindowTitle());
}

void MainContentComponent::timerCallback()
{
    updateWindowTitle();
}

bool MainContentComponent::confirmQuit (juce::Component* parent)
{
    return projectManager.confirmDiscardOrSave (parent);
}

void MainContentComponent::handleNewProject()
{
    if (! projectManager.confirmDiscardOrSave (this))
        return;

    auto fc = std::make_shared<juce::FileChooser> ("New Project", juce::File(), "*.tracktionedit");
    fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                     [this, fc] (const juce::FileChooser&)
                     {
                         const auto f = fc->getResult();
                         if (f == juce::File())
                             return;

                         releaseEditUI();

                         if (projectManager.createNewProject (f, this) == ProjectManager::LoadResult::success)
                             rebuildEditUI();
                     });
}

void MainContentComponent::handleOpenProject()
{
    if (! projectManager.confirmDiscardOrSave (this))
        return;

    auto fc = std::make_shared<juce::FileChooser> ("Open Project", juce::File(), "*.tracktionedit");
    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                     [this, fc] (const juce::FileChooser&)
                     {
                         const auto f = fc->getResult();
                         if (! f.existsAsFile())
                             return;

                         releaseEditUI();

                         if (projectManager.loadProject (f, this) == ProjectManager::LoadResult::success)
                             rebuildEditUI();
                     });
}

void MainContentComponent::handleSaveProject()
{
    switch (projectManager.saveProject (false, false, this))
    {
        case ProjectManager::SaveResult::promptSaveAs:
            handleSaveProjectAs();
            break;
        case ProjectManager::SaveResult::reloaded:
            releaseEditUI();
            rebuildEditUI();
            break;
        default:
            saveGroovePool();
            updateWindowTitle();
            break;
    }
}

void MainContentComponent::handleSaveProjectAs()
{
    auto fc = std::make_shared<juce::FileChooser> ("Save Project As",
                                                     projectManager.getCurrentProjectFile(),
                                                     "*.tracktionedit");
    fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                     [this, fc] (const juce::FileChooser&)
                     {
                         const auto f = fc->getResult();
                         if (f == juce::File())
                             return;

                         if (projectManager.saveProjectAs (f, this))
                         {
                             groovePoolManager->loadForProject (projectManager.getCurrentProjectFile());
                             saveGroovePool();
                             updateWindowTitle();
                         }
                     });
}

void MainContentComponent::handleCollectAllAndSave()
{
    switch (projectManager.collectAllAndSave (this))
    {
        case ProjectManager::SaveResult::promptSaveAs:
            handleSaveProjectAs();
            break;
        case ProjectManager::SaveResult::reloaded:
            releaseEditUI();
            rebuildEditUI();
            break;
        default:
            saveGroovePool();
            updateWindowTitle();
            break;
    }
}

void MainContentComponent::handleExportClipToLibrary (te::Clip& clip)
{
    auto w = std::make_shared<juce::AlertWindow> ("Save Clip Preset",
                                                  "Enter a name for this clip preset:",
                                                  juce::AlertWindow::QuestionIcon);
    w->addTextEditor ("name", clip.getName());
    w->addTextEditor ("category", "User");
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, this, clipPtr = te::Clip::Ptr (&clip)] (int result) mutable
    {
        if (result != 1 || clipPtr == nullptr)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        const auto category = w->getTextEditorContents ("category").trim();

        if (name.isEmpty())
            return;

        if (clipLibraryManager->saveClip (*clipPtr, name, category).existsAsFile())
        {
            if (browserPanel != nullptr)
                browserPanel->refresh();
        }
        else
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Save Clip Preset",
                                                    "Could not save the clip preset.");
        }
    }));
}

void MainContentComponent::handleExport()
{
    if (auto* edit = projectManager.getEdit())
        ExportManager::showExportDialog (*edit, this);
}

void MainContentComponent::handleImportAudio()
{
    EngineHelpers::browseForAudioFile (engine, [this] (const juce::File& f)
    {
        EngineHelpers::loadAudioFileAsClip (*projectManager.getEdit(), f);
        timeline->rebuildTracks();
    });
}

void MainContentComponent::handleAddAudioTrack()
{
    if (auto* track = EngineHelpers::getOrInsertAudioTrack (*projectManager.getEdit()))
    {
        EngineHelpers::applyDefaultDeviceChain (*track,
                                                appSettings.getDefaultDeviceChain (DefaultChainKind::audioTrack),
                                                engine,
                                                false);
    }
    timeline->rebuildTracks();
}

void MainContentComponent::handleAddMidiTrack()
{
    const int idx = (int) te::getAudioTracks (*projectManager.getEdit()).size();
    if (auto* track = EngineHelpers::getOrInsertTrackForMidi (*projectManager.getEdit(), idx))
    {
        EngineHelpers::assignDefaultInputToTrack (*track, true);
        EngineHelpers::applyDefaultDeviceChain (*track,
                                                appSettings.getDefaultDeviceChain (DefaultChainKind::midiTrack),
                                                engine,
                                                true);
    }
    timeline->rebuildTracks();
}

void MainContentComponent::handleAddMidiClip()
{
    const auto pos = projectManager.getEdit()->getTransport().getPosition();
    const te::TimeRange range { pos, te::TimeDuration::fromSeconds (4.0) };
    EngineHelpers::createMidiClip (*projectManager.getEdit(), 0, range, "MIDI Clip");
    timeline->rebuildTracks();
}

void MainContentComponent::handleClipDoubleClick (te::Clip& clip)
{
    if (auto* midiClip = dynamic_cast<te::MidiClip*> (&clip))
        showPianoRoll (*midiClip);
}

void MainContentComponent::handleAddPlugin (te::Track& track)
{
    if (auto* tray = getPluginTray())
        tray->setTrack (&track);

    syncRoamingFocus();
    showPluginsBrowser();
}

void MainContentComponent::showPianoRoll (te::MidiClip& clip)
{
    pianoRollWindow = nullptr;
    pianoRollEditor = nullptr;

    auto* editor = new PianoRollEditor (clip, *projectManager.getEdit(), timeline->getEditViewState(), *groovePoolManager);
    pianoRollEditor = editor;

    auto window = std::make_unique<PianoRollWindow> ("Piano Roll - " + clip.getName(),
                                                     AppColours::panelBackground (appSettings.getTheme()),
                                                     [this] { pianoRollWindow = nullptr; pianoRollEditor = nullptr; });
    window->setContentOwned (editor, true);
    window->setResizable (true, false);
    window->centreWithSize (1000, 560);
    window->setVisible (true);
    editor->grabKeyboardFocus();
    pianoRollWindow = std::move (window);
}

void MainContentComponent::toggleMixer()
{
    mixerVisible = ! mixerVisible;
    if (mixerVisible)
        addAndMakeVisible (*mixerPanel);
    else
        removeChildComponent (mixerPanel.get());
    resized();
}

void MainContentComponent::toggleSidechainPanel()
{
    sidechainVisible = ! sidechainVisible;
    if (sidechainVisible)
        addAndMakeVisible (*sidechainPanel);
    else
        removeChildComponent (sidechainPanel.get());
    resized();
}

void MainContentComponent::showSidechainPanelForPlugin (te::Plugin* plugin)
{
    if (sidechainPanel == nullptr)
        return;

    if (! sidechainVisible)
    {
        sidechainVisible = true;
        addAndMakeVisible (*sidechainPanel);
    }

    if (plugin != nullptr)
        sidechainPanel->focusPlugin (plugin->itemID);

    resized();
}

void MainContentComponent::toggleAutomationPanel()
{
    automationVisible = ! automationVisible;

    if (automationPanel == nullptr)
        return;

    if (automationVisible)
    {
        // Follow the current track selection when opening
        if (automationPanel->getTrack() == nullptr)
            if (auto* selected = projectManager.getSelectionManager().getFirstItemOfType<te::Track>())
                automationPanel->setTrack (selected);

        addAndMakeVisible (*automationPanel);
    }
    else
    {
        removeChildComponent (automationPanel.get());
    }

    resized();
}

void MainContentComponent::syncRoamingFocus()
{
    if (auto* tray = getPluginTray())
    {
        if (auto* focused = resolveFocusedTrack())
            tray->setTrack (focused);
    }

    if (detailPanelStack != nullptr)
    {
        detailPanelStack->updateClipSelection (
            projectManager.getSelectionManager().getItemsOfType<te::Clip>());
    }

    if (automationPanel != nullptr)
    {
        if (auto* focused = resolveFocusedTrack())
            automationPanel->setTrack (focused);
    }

    if (browserPanel != nullptr)
    {
        if (auto* pluginBrowser = browserPanel->getPluginBrowser())
            pluginBrowser->selectedTrack = resolveFocusedTrack();
    }

    resized();
}

void MainContentComponent::saveGroovePool()
{
    if (groovePoolManager != nullptr)
        groovePoolManager->saveForProject();
}

te::Track* MainContentComponent::resolveFocusedTrack() const
{
    if (timeline != nullptr && timeline->getEditViewState().getMainView() == MainView::session
        && sessionFocusedTrackId != te::EditItemID())
    {
        if (auto* edit = projectManager.getEdit())
        {
            for (auto track : te::getAllTracks (*edit))
            {
                if (track->itemID == sessionFocusedTrackId)
                    return track;
            }
        }
    }

    auto& selection = projectManager.getSelectionManager();

    if (auto* selected = selection.getFirstItemOfType<te::Track>())
        return selected;

    if (auto* clip = selection.getFirstItemOfType<te::Clip>())
        return clip->getTrack();

    if (auto* tray = getPluginTray())
        if (auto* t = tray->getTrack())
            return t;

    if (auto* edit = projectManager.getEdit())
    {
        for (auto track : te::getAllTracks (*edit))
        {
            if (auto* clipTrack = dynamic_cast<te::ClipTrack*> (track))
                return clipTrack;
        }
    }

    return nullptr;
}

PluginTrayComponent* MainContentComponent::getPluginTray() const
{
    return detailPanelStack != nullptr ? detailPanelStack->getPluginTray() : nullptr;
}

void MainContentComponent::showPluginsBrowser()
{
    if (! browserVisible)
        toggleBrowserPanel();

    if (browserPanel != nullptr)
        browserPanel->showPluginsTab();
}

void MainContentComponent::resized()
{
    auto r = getLocalBounds();
    transportBar->setBounds (r.removeFromTop (60));

    if (midiLearnController.isActive())
        learnStatusLabel.setBounds (r.removeFromTop (18).reduced (8, 0));

    if (mixerVisible)
        mixerPanel->setBounds (r.removeFromBottom (200));

    if (sidechainVisible)
        sidechainPanel->setBounds (r.removeFromBottom (200));

    if (automationVisible && automationPanel != nullptr)
        automationPanel->setBounds (r.removeFromBottom (automationPanel->getPreferredHeight()));

    if (detailPanelStack != nullptr)
        detailPanelStack->setBounds (r.removeFromBottom (detailPanelStack->getPreferredHeight()));

    if (browserPanel != nullptr && browserVisible)
    {
        auto browserArea = r.removeFromLeft (BrowserPanel::preferredWidth);
        browserPanel->setBounds (browserArea);
    }

    auto* edit = projectManager.getEdit();
    const bool sessionMode = edit != nullptr
        && timeline != nullptr
        && timeline->getEditViewState().getMainView() == MainView::session;

    if (sessionMode)
    {
        timeline->setVisible (false);

        if (performancePanelVisible && performanceMacroPanel != nullptr)
            performanceMacroPanel->setBounds (r.removeFromBottom (PerformanceMacroPanel::preferredHeight));

        if (sessionView != nullptr)
        {
            sessionView->setVisible (true);
            sessionView->setBounds (r);
        }
    }
    else
    {
        if (sessionView != nullptr)
            sessionView->setVisible (false);

        if (performanceMacroPanel != nullptr)
            performanceMacroPanel->setVisible (false);

        timeline->setVisible (true);
        timeline->setBounds (r);
    }
}

void MainContentComponent::toggleMainView()
{
    if (timeline == nullptr)
        return;

    auto& viewState = timeline->getEditViewState();
    const auto next = viewState.getMainView() == MainView::session ? MainView::arrangement : MainView::session;

    if (next == MainView::session && sessionArrangementBridge != nullptr && transportController != nullptr)
        sessionArrangementBridge->syncWritePositionFromTransport();

    viewState.setMainView (next);

    if (transportBar != nullptr)
        transportBar->setSessionViewActive (next == MainView::session);

    updateMainViewVisibility();
    resized();
}

void MainContentComponent::updateMainViewVisibility()
{
    if (timeline == nullptr)
        return;

    const bool sessionMode = timeline->getEditViewState().getMainView() == MainView::session;

    if (transportBar != nullptr)
        transportBar->setSessionViewActive (sessionMode);

    if (sessionView != nullptr)
        sessionView->setVisible (sessionMode);

    if (! sessionMode)
    {
        performancePanelVisible = false;

        if (performanceMacroPanel != nullptr)
            performanceMacroPanel->setVisible (false);

        if (transportBar != nullptr)
            transportBar->setPerformancePanelVisible (false);
    }

    if (timeline != nullptr)
        timeline->setVisible (! sessionMode);
}

void MainContentComponent::togglePerformancePanel()
{
    if (timeline == nullptr || timeline->getEditViewState().getMainView() != MainView::session)
        return;

    performancePanelVisible = ! performancePanelVisible;

    if (performanceMacroPanel != nullptr)
        performanceMacroPanel->setVisible (performancePanelVisible);

    if (transportBar != nullptr)
        transportBar->setPerformancePanelVisible (performancePanelVisible);

    resized();
}

bool MainContentComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (auto* mappings = commandManager.getKeyMappings())
        return mappings->keyPressed (key, this);

    return false;
}

void MainContentComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &appSettings)
    {
        application.getAppLookAndFeel().setTheme (appSettings.getTheme());

        if (contentLibraryManager != nullptr)
            contentLibraryManager->rescanAll();
    }
}

ApplicationCommandTarget* MainContentComponent::getNextCommandTarget()
{
    return nullptr;
}

void MainContentComponent::getAllCommands (juce::Array<juce::CommandID>& commands)
{
    const juce::CommandID ids[] {
        AppCommandIDs::play,
        AppCommandIDs::stop,
        AppCommandIDs::record,
        AppCommandIDs::undo,
        AppCommandIDs::redo,
        AppCommandIDs::saveProject,
        AppCommandIDs::saveProjectAs,
        AppCommandIDs::exportProject,
        AppCommandIDs::showPreferences,
        AppCommandIDs::toggleMidiLearn,
        AppCommandIDs::toggleGrid,
        AppCommandIDs::duplicateClips,
        AppCommandIDs::groupClips,
        AppCommandIDs::ungroupClips,
        AppCommandIDs::toggleRipple,
        AppCommandIDs::deleteTimelineSelection,
        AppCommandIDs::addMarker,
        AppCommandIDs::prevMarker,
        AppCommandIDs::nextMarker,
        AppCommandIDs::toggleTakeLanes,
        AppCommandIDs::consolidateClips,
        AppCommandIDs::applyGrooveToClips,
        AppCommandIDs::toggleDetailDevices,
        AppCommandIDs::toggleDetailClip,
        AppCommandIDs::toggleMainView,
        AppCommandIDs::toggleRecordToArrangement,
        AppCommandIDs::captureSessionToArrangement,
        AppCommandIDs::duplicateSessionLoopToArrangement,
        AppCommandIDs::togglePerformancePanel,
        AppCommandIDs::pluginCopy,
        AppCommandIDs::pluginPaste,
        AppCommandIDs::pluginDuplicate,
        AppCommandIDs::pluginDelete,
        AppCommandIDs::pianoDeleteNotes,
        AppCommandIDs::pianoSelectAll,
        AppCommandIDs::pianoDuplicateNotes,
        AppCommandIDs::pianoQuantize,
        AppCommandIDs::pianoHumanize,
        AppCommandIDs::pianoToggleFold,
        AppCommandIDs::pianoToggleScaleSnap,
        AppCommandIDs::pianoToggleDrawTool,
        AppCommandIDs::pianoToggleStepTool,
        AppCommandIDs::pianoEscape,
        AppCommandIDs::pianoNudgeLeft,
        AppCommandIDs::pianoNudgeRight,
        AppCommandIDs::pianoNudgeUp,
        AppCommandIDs::pianoNudgeDown,
        AppCommandIDs::pianoStepRest,
    };

    commands.addArray (ids, (int) std::size (ids));
}

void MainContentComponent::getCommandInfo (juce::CommandID commandID, juce::ApplicationCommandInfo& result)
{
    result.setInfo (AppCommands::getCommandName (commandID), AppCommands::getCommandName (commandID), "SkeletonHive", 0);
}

bool MainContentComponent::isPluginTrayContext() const
{
    if (getPluginTray() == nullptr || getActivePianoRollEditor() != nullptr)
        return false;

    if (getPluginTray()->hasKeyboardFocus (true))
        return true;

    return ! projectManager.getSelectionManager().getItemsOfType<te::Plugin>().isEmpty();
}

bool MainContentComponent::perform (const InvocationInfo& info)
{
    const auto mods = juce::ModifierKeys::currentModifiers;

    if (auto* piano = getActivePianoRollEditor())
    {
        switch (info.commandID)
        {
            case AppCommandIDs::pianoDeleteNotes:
            case AppCommandIDs::pianoSelectAll:
            case AppCommandIDs::pianoDuplicateNotes:
            case AppCommandIDs::pianoQuantize:
            case AppCommandIDs::pianoHumanize:
            case AppCommandIDs::pianoToggleFold:
            case AppCommandIDs::pianoToggleScaleSnap:
            case AppCommandIDs::pianoToggleDrawTool:
            case AppCommandIDs::pianoToggleStepTool:
            case AppCommandIDs::pianoEscape:
            case AppCommandIDs::pianoNudgeLeft:
            case AppCommandIDs::pianoNudgeRight:
            case AppCommandIDs::pianoNudgeUp:
            case AppCommandIDs::pianoNudgeDown:
            case AppCommandIDs::pianoStepRest:
                return piano->performCommand (info.commandID, mods);
            default:
                break;
        }
    }

    if (isPluginTrayContext())
    {
        switch (info.commandID)
        {
            case AppCommandIDs::pluginCopy:
            case AppCommandIDs::pluginPaste:
            case AppCommandIDs::pluginDuplicate:
            case AppCommandIDs::pluginDelete:
                if (auto* tray = getPluginTray())
                    if (tray->performCommand (info.commandID))
                        return true;
                break;
            default:
                break;
        }
    }

    auto* edit = projectManager.getEdit();

    switch (info.commandID)
    {
        case AppCommandIDs::play:
            transportController->togglePlay();
            return true;
        case AppCommandIDs::stop:
            transportController->stop();
            return true;
        case AppCommandIDs::record:
            if (mods.isAnyModifierKeyDown())
                break;
            transportController->toggleRecord();
            return true;
        case AppCommandIDs::undo:
            if (edit != nullptr) edit->undo();
            return true;
        case AppCommandIDs::redo:
            if (edit != nullptr) edit->redo();
            return true;
        case AppCommandIDs::saveProject:
            handleSaveProject();
            return true;
        case AppCommandIDs::saveProjectAs:
            handleSaveProjectAs();
            return true;
        case AppCommandIDs::exportProject:
            handleExport();
            return true;
        case AppCommandIDs::showPreferences:
            showPreferences();
            return true;
        case AppCommandIDs::toggleMidiLearn:
            toggleMidiLearn();
            return true;
        case AppCommandIDs::toggleGrid:
        case AppCommandIDs::duplicateClips:
        case AppCommandIDs::groupClips:
        case AppCommandIDs::ungroupClips:
        case AppCommandIDs::toggleRipple:
        case AppCommandIDs::deleteTimelineSelection:
        case AppCommandIDs::addMarker:
        case AppCommandIDs::prevMarker:
        case AppCommandIDs::nextMarker:
        case AppCommandIDs::toggleTakeLanes:
        case AppCommandIDs::consolidateClips:
            if (timeline != nullptr)
                return timeline->performCommand (info.commandID);
            break;
        case AppCommandIDs::applyGrooveToClips:
        {
            if (groovePoolManager == nullptr)
                break;

            if (const auto* groove = groovePoolManager->getSelectedGroove())
            {
                juce::String error;
                if (auto* currentEdit = projectManager.getEdit())
                {
                    EngineHelpers::applyGrooveToSelection (*currentEdit, projectManager.getSelectionManager(),
                                                           *groove, &error);

                    if (error.isNotEmpty())
                        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, "Apply Groove", error);
                }

                return true;
            }

            break;
        }
        case AppCommandIDs::toggleDetailDevices:
            if (detailPanelStack != nullptr)
            {
                detailPanelStack->setActiveView (DetailView::devices);
                resized();
                return true;
            }
            break;
        case AppCommandIDs::toggleDetailClip:
            if (detailPanelStack != nullptr)
            {
                detailPanelStack->setActiveView (DetailView::clip);
                resized();
                return true;
            }
            break;
        case AppCommandIDs::toggleMainView:
            toggleMainView();
            return true;
        case AppCommandIDs::toggleRecordToArrangement:
        case AppCommandIDs::captureSessionToArrangement:
        case AppCommandIDs::duplicateSessionLoopToArrangement:
        {
            if (timeline == nullptr || timeline->getEditViewState().getMainView() != MainView::session)
                break;

            if (sessionArrangementBridge == nullptr)
                break;

            switch (info.commandID)
            {
                case AppCommandIDs::toggleRecordToArrangement:
                    sessionArrangementBridge->setRecordToArrangementEnabled (
                        ! sessionArrangementBridge->isRecordToArrangementEnabled());
                    return true;
                case AppCommandIDs::captureSessionToArrangement:
                    sessionArrangementBridge->captureAndInsert();
                    timeline->rebuildTracks();
                    return true;
                case AppCommandIDs::duplicateSessionLoopToArrangement:
                    if (sessionFocusedTrackId != te::EditItemID())
                    {
                        sessionArrangementBridge->duplicateLoopToArrangement (sessionFocusedTrackId,
                                                                              sessionFocusedSceneIndex);
                        timeline->rebuildTracks();
                    }
                    return true;
                default: break;
            }
            break;
        }
        case AppCommandIDs::togglePerformancePanel:
            togglePerformancePanel();
            return true;
        default:
            break;
    }

    return false;
}

PianoRollEditor* MainContentComponent::getActivePianoRollEditor() const
{
    if (pianoRollEditor == nullptr || pianoRollWindow == nullptr || ! pianoRollWindow->isVisible())
        return nullptr;

    if (! pianoRollEditor->hasKeyboardFocus (true))
        return nullptr;

    return pianoRollEditor;
}

void MainContentComponent::showPreferences()
{
    PreferencesDialog::show (this,
                             appSettings,
                             application.getAppLookAndFeel(),
                             engine,
                             *pluginScanner,
                             *pluginStateManager,
                             commandManager,
                             [this] (int seconds)
                             {
                                 projectManager.enableAutosave (seconds);
                             },
                             [this]
                             {
                                 if (contentLibraryManager != nullptr)
                                     contentLibraryManager->rescanAll();
                             });
}

void MainContentComponent::toggleBrowserPanel()
{
    browserVisible = ! browserVisible;

    if (browserPanel != nullptr)
    {
        if (browserVisible)
        {
            addAndMakeVisible (*browserPanel);
        }
        else
        {
            browserPanel->stopPreview();
            removeChildComponent (browserPanel.get());
        }
    }

    if (transportBar != nullptr)
        transportBar->setBrowserToggleState (browserVisible);

    resized();
}

void MainContentComponent::toggleMidiLearn()
{
    if (sessionMidiMapper != nullptr)
        sessionMidiMapper->cancelLearn();

    midiLearnController.setActive (! midiLearnController.isActive());
    updateLearnStatus();
}

void MainContentComponent::updateLearnStatus()
{
    if (transportBar != nullptr)
        transportBar->setLearnModeActive (midiLearnController.isActive());

    juce::String text;

    if (sessionMidiMapper != nullptr)
        text = sessionMidiMapper->getStatusText();

    if (text.isEmpty())
    {
        if (auto* edit = projectManager.getEdit())
            text = midiLearnController.getStatusText (*edit);
    }

    learnStatusLabel.setText (text, juce::dontSendNotification);
    learnStatusLabel.setVisible (text.isNotEmpty());
    resized();
}

MainWindow::MainWindow (SkeletonHiveApplication& app)
    : DocumentWindow ("SkeletonHive",
                      AppColours::panelBackground (app.getAppSettings().getTheme()),
                      DocumentWindow::allButtons),
      application (app)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new MainContentComponent (app), true);
    setResizable (true, true);
    centreWithSize (1200, 800);
    setVisible (true);
}

MainWindow::~MainWindow() = default;

void MainWindow::prepareForShutdown()
{
    if (auto* content = dynamic_cast<MainContentComponent*> (getContentComponent()))
        content->prepareForShutdown();
}

bool MainWindow::confirmClose()
{
    if (auto* content = dynamic_cast<MainContentComponent*> (getContentComponent()))
        return content->confirmQuit (this);

    return true;
}

void MainWindow::closeButtonPressed()
{
    if (confirmClose())
        application.systemRequestedQuit();
}

} // namespace skeletonhive
