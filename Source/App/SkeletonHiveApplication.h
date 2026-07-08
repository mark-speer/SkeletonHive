#pragma once

#include "TracktionCommon.h"
#include "Engine/ExtendedUIBehaviour.h"
#include "Engine/ExtendedEngineBehaviour.h"
#include "Engine/ProjectManager.h"
#include "Engine/TransportController.h"
#include "Engine/PluginScanner.h"
#include "Engine/AppSettings.h"
#include "Engine/MidiLearnController.h"
#include "UI/AppLookAndFeel.h"

namespace skeletonhive
{

class SkeletonHiveApplication : public juce::JUCEApplication
{
public:
    SkeletonHiveApplication() = default;

    const juce::String getApplicationName() override       { return "SkeletonHive"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    // The out-of-process plugin scanner works by launching a second instance of
    // this executable, so that instance must be allowed to run or scans will hang.
    bool moreThanOneInstanceAllowed() override
    {
        const auto params = juce::JUCEApplicationBase::getCommandLineParameters();
        return params.contains ("--PluginScan:") || params.contains ("SkeletonHivePluginHost");
    }

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;

    te::Engine& getEngine() { return *engine; }
    ProjectManager& getProjectManager() { return *projectManager; }
    AppSettings& getAppSettings() { return appSettings; }
    AppLookAndFeel& getAppLookAndFeel() { return appLookAndFeel; }
    MidiLearnController& getMidiLearnController() { return *midiLearnController; }

private:
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<ProjectManager> projectManager;
    std::unique_ptr<juce::DocumentWindow> mainWindow;
    AppSettings appSettings;
    AppLookAndFeel appLookAndFeel;
    std::unique_ptr<MidiLearnController> midiLearnController;
};

} // namespace skeletonhive
