#include "MainWindow.h"
#include "TracktionCommon.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>

namespace arrange
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

MainContentComponent::MainContentComponent (ArrangeApplication& app)
    : application (app),
      engine (app.getEngine()),
      projectManager (app.getProjectManager())
{
    pluginScanner = std::make_unique<PluginScanner> (engine);
    createDefaultProject();
    rebuildEditUI();

    automationReadButton.setRadioGroupId (1);
    automationTouchButton.setRadioGroupId (1);
    automationLatchButton.setRadioGroupId (1);
    automationReadButton.setToggleState (true, juce::dontSendNotification);

    automationReadButton.onClick = [this] { automationMode = AutomationMode::read; };
    automationTouchButton.onClick = [this] { automationMode = AutomationMode::touch; };
    automationLatchButton.onClick = [this] { automationMode = AutomationMode::latch; };

    addAndMakeVisible (automationPanel);
    addAndMakeVisible (automationReadButton);
    addAndMakeVisible (automationTouchButton);
    addAndMakeVisible (automationLatchButton);

    addKeyListener (this);
    setWantsKeyboardFocus (true);

    projectManager.enableAutosave (60);
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
                   .getChildFile ("ArrangeDAW");
    dir.createDirectory();
    auto projectFile = dir.getNonexistentChildFile ("Untitled", ".tracktionedit");
    projectManager.createNewProject (projectFile);

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
    automationLanes.clear();
    automationPanel.removeAllChildren();

    pluginBrowser = nullptr;
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
                                                    projectManager.getInsertPoint());
    mixerPanel = std::make_unique<MixerPanel> (*edit);
    pluginBrowser = std::make_unique<PluginBrowser> (*pluginScanner, *edit);

    transportBar->onNewProject = [this] { handleNewProject(); };
    transportBar->onOpenProject = [this] { handleOpenProject(); };
    transportBar->onSaveProject = [this] { handleSaveProject(); };
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

    timeline->onClipDoubleClick = [this] (te::Clip& c) { handleClipDoubleClick (c); };
    timeline->onAddPlugin = [this] (te::Track& t) { handleAddPlugin (t); };
    timeline->createPlugin = [this] (const juce::PluginDescription& desc)
    {
        if (auto* edit = projectManager.getEdit())
            return pluginScanner->createPlugin (desc, *edit);
        return te::Plugin::Ptr {};
    };

    addAndMakeVisible (*transportBar);
    addAndMakeVisible (*timeline);
    pluginBrowser->setVisible (false);

    if (mixerVisible)
        addAndMakeVisible (*mixerPanel);

    resized();
}

void MainContentComponent::handleNewProject()
{
    auto fc = std::make_shared<juce::FileChooser> ("New Project", juce::File(), "*.tracktionedit");
    fc->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                     [this, fc] (const juce::FileChooser&)
                     {
                         const auto f = fc->getResult();
                         if (f != juce::File())
                         {
                             releaseEditUI();
                             projectManager.createNewProject (f);
                             rebuildEditUI();
                         }
                     });
}

void MainContentComponent::handleOpenProject()
{
    auto fc = std::make_shared<juce::FileChooser> ("Open Project", juce::File(), "*.tracktionedit");
    fc->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                     [this, fc] (const juce::FileChooser&)
                     {
                         const auto f = fc->getResult();
                         if (f.existsAsFile())
                         {
                             releaseEditUI();
                             projectManager.loadProject (f);

                             // Even if the load failed we must rebuild against
                             // whatever Edit the ProjectManager now holds.
                             if (projectManager.getEdit() != nullptr)
                                 rebuildEditUI();
                         }
                     });
}

void MainContentComponent::handleSaveProject()
{
    projectManager.saveProject (false);
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
    mixerPanel->rebuild();
}

void MainContentComponent::handleAddMidiTrack()
{
    const int idx = (int) te::getAudioTracks (*projectManager.getEdit()).size();
    EngineHelpers::getOrInsertTrackForMidi (*projectManager.getEdit(), idx);
    timeline->rebuildTracks();
    mixerPanel->rebuild();
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

void MainContentComponent::setupAutomationPanel (te::Track& track)
{
    automationLanes.clear();
    automationPanel.removeAllChildren();

    if (auto* audioTrack = dynamic_cast<te::AudioTrack*> (&track))
    {
        if (auto* vol = audioTrack->getVolumePlugin())
        {
            for (auto param : vol->getAutomatableParameters())
            {
                auto* lane = new AutomationLaneComponent (*param, *projectManager.getEdit(),
                                                          timeline->getEditViewState());
                lane->setAutomationMode (automationMode);
                automationPanel.addAndMakeVisible (lane);
                automationLanes.add (lane);
            }
        }
    }
    resized();
}

void MainContentComponent::resized()
{
    auto r = getLocalBounds();
    transportBar->setBounds (r.removeFromTop (60));

    auto automationRow = r.removeFromBottom (24);
    automationReadButton.setBounds (automationRow.removeFromLeft (60).reduced (2));
    automationTouchButton.setBounds (automationRow.removeFromLeft (60).reduced (2));
    automationLatchButton.setBounds (automationRow.removeFromLeft (60).reduced (2));

    if (mixerVisible)
        mixerPanel->setBounds (r.removeFromBottom (200));

    if (pluginBrowser != nullptr && pluginBrowser->isVisible())
    {
        auto pluginArea = r.removeFromRight (250);
        pluginBrowser->setBounds (pluginArea);
    }

    if (! automationLanes.isEmpty())
        automationPanel.setBounds (r.removeFromBottom (80));

    timeline->setBounds (r);
}

bool MainContentComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
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
    return false;
}

MainWindow::MainWindow (ArrangeApplication& app)
    : DocumentWindow ("Arrange DAW",
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

void MainWindow::closeButtonPressed()
{
    application.systemRequestedQuit();
}

} // namespace arrange
