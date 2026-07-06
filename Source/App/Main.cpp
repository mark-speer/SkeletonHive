#include "SkeletonHiveApplication.h"
#include "MainWindow.h"

namespace skeletonhive
{

void SkeletonHiveApplication::initialise (const juce::String& commandLine)
{
    if (te::PluginManager::startChildProcessPluginScan (commandLine))
        return;

    auto uiBehaviour = std::make_unique<ExtendedUIBehaviour>();
    auto engineBehaviour = std::make_unique<ExtendedEngineBehaviour>();
    engine = std::make_unique<te::Engine> ("SkeletonHive", std::move (uiBehaviour), std::move (engineBehaviour));
    projectManager = std::make_unique<ProjectManager> (*engine);

    mainWindow = std::make_unique<MainWindow> (*this);
}

void SkeletonHiveApplication::shutdown()
{
    if (auto* window = dynamic_cast<MainWindow*> (mainWindow.get()))
        window->prepareForShutdown();

    if (engine != nullptr && projectManager != nullptr)
    {
        EngineHelpers::prepareEngineForShutdown (*engine, projectManager->getEdit());
        projectManager->prepareForShutdown();
    }

    mainWindow = nullptr;
    projectManager = nullptr;

    if (engine != nullptr)
        EngineHelpers::releaseAudioDevices (*engine);

    engine = nullptr;
}

} // namespace skeletonhive

START_JUCE_APPLICATION (skeletonhive::SkeletonHiveApplication)
