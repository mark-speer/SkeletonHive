#pragma once

#include "TracktionCommon.h"
#include "Engine/ExtendedUIBehaviour.h"
#include "Engine/ExtendedEngineBehaviour.h"
#include "Engine/ProjectManager.h"
#include "Engine/TransportController.h"
#include "Engine/PluginScanner.h"

namespace arrange
{

class ArrangeApplication : public juce::JUCEApplication
{
public:
    ArrangeApplication() = default;

    const juce::String getApplicationName() override       { return "Arrange DAW"; }
    const juce::String getApplicationVersion() override    { return "0.1.0"; }
    // The out-of-process plugin scanner works by launching a second instance of
    // this executable, so that instance must be allowed to run or scans will hang.
    bool moreThanOneInstanceAllowed() override
    {
        return juce::JUCEApplicationBase::getCommandLineParameters().contains ("--PluginScan:");
    }

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;

    te::Engine& getEngine() { return *engine; }
    ProjectManager& getProjectManager() { return *projectManager; }

private:
    std::unique_ptr<te::Engine> engine;
    std::unique_ptr<ProjectManager> projectManager;
    std::unique_ptr<juce::DocumentWindow> mainWindow;
};

} // namespace arrange
