#include "ProjectManager.h"

namespace skeletonhive
{

class AutosaveTimer : public juce::Timer
{
public:
    AutosaveTimer (ProjectManager& pm) : projectManager (pm) {}

    void timerCallback() override
    {
        projectManager.saveProject (true);
    }

private:
    ProjectManager& projectManager;
};

ProjectManager::ProjectManager (te::Engine& e)
    : engine (e), selectionManager (engine)
{
}

bool ProjectManager::createNewProject (const juce::File& editFile)
{
    selectionManager.deselectAll();
    edit = te::createEmptyEdit (engine, editFile);
    currentProjectFile = editFile;
    setupEdit();
    return te::EditFileOperations (*edit).save (true, true, false);
}

bool ProjectManager::loadProject (const juce::File& editFile)
{
    if (! editFile.existsAsFile())
        return false;

    selectionManager.deselectAll();
    edit = te::loadEditFromFile (engine, editFile);
    currentProjectFile = editFile;
    setupEdit();
    return edit != nullptr;
}

bool ProjectManager::saveProject (bool async)
{
    if (edit == nullptr || ! currentProjectFile.exists())
        return false;

    return te::EditFileOperations (*edit).save (true, async, false);
}

void ProjectManager::enableAutosave (int intervalSeconds)
{
    autosaveTimer = std::make_unique<AutosaveTimer> (*this);
    autosaveTimer->startTimer (intervalSeconds * 1000);
}

void ProjectManager::prepareForShutdown()
{
    autosaveTimer = nullptr;
    saveProject (false);
}

void ProjectManager::setupEdit()
{
    if (edit == nullptr)
        return;

    insertPoint = std::make_unique<te::EditInsertPoint> (*edit);
    selectionManager.edit = edit.get();
    selectionManager.insertPoint = insertPoint.get();

    edit->playInStopEnabled = true;
    edit->getTransport().ensureContextAllocated();
    EngineHelpers::setupDefaultTracks (*edit);
}

} // namespace skeletonhive
