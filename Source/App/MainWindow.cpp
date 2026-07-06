#include "MainWindow.h"
#include "TracktionCommon.h"
#include "Engine/ExportManager.h"
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
      projectManager (app.getProjectManager())
{
    pluginScanner = std::make_unique<PluginScanner> (engine);
    pluginStateManager = std::make_unique<PluginStateManager>();
    telemetryHub = std::make_unique<UiTelemetryHub>();
    createDefaultProject();
    rebuildEditUI();

    addKeyListener (this);
    setWantsKeyboardFocus (true);

    projectManager.enableAutosave (60);
    startTimerHz (2);
    setSize (1200, 800);
}

MainContentComponent::~MainContentComponent() = default;

void MainContentComponent::prepareForShutdown()
{
    pianoRollWindow = nullptr;
}

void MainContentComponent::createDefaultProject()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("SkeletonHive");
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
    automationPanel = nullptr;

    pluginBrowser = nullptr;
    pluginTray = nullptr;
    sidechainPanel = nullptr;
    SidechainRouting::openMatrixForPlugin = nullptr;
    mixerPanel = nullptr;
    timeline = nullptr;
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
    mixerPanel = std::make_unique<MixerPanel> (*edit, telemetryHub.get());
    pluginBrowser = std::make_unique<PluginBrowser> (*pluginScanner, *edit, *pluginStateManager);
    pluginTray = std::make_unique<PluginTrayComponent> (timeline->getEditViewState(), *pluginStateManager);
    sidechainPanel = std::make_unique<SidechainMatrixPanel> (*edit);
    automationPanel = std::make_unique<AutomationPanel> (*edit, timeline->getEditViewState());

    SidechainRouting::openMatrixForPlugin = [this] (te::Plugin* plugin)
    {
        showSidechainPanelForPlugin (plugin);
    };

    transportBar->onNewProject = [this] { handleNewProject(); };
    transportBar->onOpenProject = [this] { handleOpenProject(); };
    transportBar->onSaveProject = [this] { handleSaveProject(); };
    transportBar->onSaveProjectAs = [this] { handleSaveProjectAs(); };
    transportBar->onExport = [this] { handleExport(); };
    transportBar->onImportAudio = [this] { handleImportAudio(); };
    transportBar->onAddAudioTrack = [this] { handleAddAudioTrack(); };
    transportBar->onAddMidiTrack = [this] { handleAddMidiTrack(); };
    transportBar->onAddMidiClip = [this] { handleAddMidiClip(); };
    transportBar->onAudioSettings = [this] { EngineHelpers::showAudioDeviceSettings (engine); };
    transportBar->onScanPlugins = [this]
    {
        if (! pluginBrowser->isVisible())
            addAndMakeVisible (*pluginBrowser);
        resized();
    };
    transportBar->onToggleMixer = [this] { toggleMixer(); };
    transportBar->onToggleSidechain = [this] { toggleSidechainPanel(); };
    transportBar->onToggleAutomation = [this] { toggleAutomationPanel(); };

    timeline->onClipDoubleClick = [this] (te::Clip& c) { handleClipDoubleClick (c); };
    timeline->onAddPlugin = [this] (te::Track& t) { handleAddPlugin (t); };
    timeline->onTrackSelected = [this] (te::Track& t)
    {
        if (pluginTray != nullptr)
            pluginTray->setTrack (&t);

        if (automationPanel != nullptr)
        {
            automationPanel->setTrack (&t);
            if (automationVisible)
                resized();
        }
    };
    timeline->createPlugin = [this] (const juce::PluginDescription& desc)
    {
        if (auto* edit = projectManager.getEdit())
            return pluginScanner->createPlugin (desc, *edit);
        return te::Plugin::Ptr {};
    };

    pluginTray->setCreatePlugin ([this] (const juce::PluginDescription& desc)
    {
        if (auto* edit = projectManager.getEdit())
            return pluginScanner->createPlugin (desc, *edit);
        return te::Plugin::Ptr {};
    });
    pluginTray->setOnAddPlugin ([this] (te::Track& t) { handleAddPlugin (t); });

    addAndMakeVisible (*transportBar);
    addAndMakeVisible (*timeline);
    addAndMakeVisible (*pluginTray);
    pluginBrowser->setVisible (false);

    if (mixerVisible)
        addAndMakeVisible (*mixerPanel);

    if (sidechainVisible)
        addAndMakeVisible (*sidechainPanel);

    if (automationVisible)
        addAndMakeVisible (*automationPanel);

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
    switch (projectManager.saveProject (false, this))
    {
        case ProjectManager::SaveResult::promptSaveAs:
            handleSaveProjectAs();
            break;
        case ProjectManager::SaveResult::reloaded:
            releaseEditUI();
            rebuildEditUI();
            break;
        default:
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
                             updateWindowTitle();
                     });
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
    EngineHelpers::getOrInsertAudioTrack (*projectManager.getEdit());
    timeline->rebuildTracks();
}

void MainContentComponent::handleAddMidiTrack()
{
    const int idx = (int) te::getAudioTracks (*projectManager.getEdit()).size();
    if (auto* track = EngineHelpers::getOrInsertTrackForMidi (*projectManager.getEdit(), idx))
        EngineHelpers::assignDefaultInputToTrack (*track, true);
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
    pluginBrowser->selectedTrack = &track;
    if (pluginTray != nullptr)
        pluginTray->setTrack (&track);
    if (! pluginBrowser->isVisible())
        addAndMakeVisible (*pluginBrowser);
    pluginBrowser->toFront (true);
    resized();
}

void MainContentComponent::showPianoRoll (te::MidiClip& clip)
{
    pianoRollWindow = nullptr;

    auto* editor = new PianoRollEditor (clip, *projectManager.getEdit(), timeline->getEditViewState());
    auto window = std::make_unique<PianoRollWindow> ("Piano Roll - " + clip.getName(),
                                                     juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                         .findColour (juce::ResizableWindow::backgroundColourId),
                                                     [this] { pianoRollWindow = nullptr; });
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

void MainContentComponent::resized()
{
    auto r = getLocalBounds();
    transportBar->setBounds (r.removeFromTop (60));

    if (mixerVisible)
        mixerPanel->setBounds (r.removeFromBottom (200));

    if (sidechainVisible)
        sidechainPanel->setBounds (r.removeFromBottom (200));

    if (automationVisible && automationPanel != nullptr)
        automationPanel->setBounds (r.removeFromBottom (automationPanel->getPreferredHeight()));

    if (pluginTray != nullptr)
        pluginTray->setBounds (r.removeFromBottom (148));

    if (pluginBrowser != nullptr && pluginBrowser->isVisible())
    {
        auto pluginArea = r.removeFromRight (250);
        pluginBrowser->setBounds (pluginArea);
    }

    timeline->setBounds (r);
}

bool MainContentComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (pluginTray != nullptr && pluginTray->handleKeyPress (key))
        return true;

    if (timeline != nullptr && timeline->handleKeyPress (key))
        return true;

    auto* edit = projectManager.getEdit();

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (edit != nullptr)
            edit->undo();
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('y', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (edit != nullptr)
            edit->redo();
        return true;
    }
    if (key == juce::KeyPress::spaceKey)
    {
        transportController->togglePlay();
        return true;
    }
    if (key == juce::KeyPress ('r', juce::ModifierKeys(), 0))
    {
        transportController->toggleRecord();
        return true;
    }
    if (key == juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('s', juce::ModifierKeys::ctrlModifier, 0))
    {
        handleSaveProject();
        return true;
    }
    if (key == juce::KeyPress ('s', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('s', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        handleSaveProjectAs();
        return true;
    }
    if (key == juce::KeyPress ('e', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('e', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        handleExport();
        return true;
    }
    return false;
}

MainWindow::MainWindow (SkeletonHiveApplication& app)
    : DocumentWindow ("SkeletonHive",
                      juce::Desktop::getInstance().getDefaultLookAndFeel()
                          .findColour (juce::ResizableWindow::backgroundColourId),
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
