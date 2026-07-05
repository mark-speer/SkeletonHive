#pragma once

#include "EngineHelpers.h"
#include "ExtendedUIBehaviour.h"

namespace arrange
{

class ProjectManager
{
public:
    explicit ProjectManager (te::Engine& engine);

    te::Engine& getEngine() { return engine; }
    te::Edit* getEdit() { return edit.get(); }
    te::SelectionManager& getSelectionManager() { return selectionManager; }
    te::EditInsertPoint* getInsertPoint() { return insertPoint.get(); }

    bool createNewProject (const juce::File& editFile);
    bool loadProject (const juce::File& editFile);
    bool saveProject (bool async = true);
    juce::File getCurrentProjectFile() const { return currentProjectFile; }

    void enableAutosave (int intervalSeconds = 60);
    void prepareForShutdown();

private:
    te::Engine& engine;
    te::SelectionManager selectionManager;
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::EditInsertPoint> insertPoint;
    juce::File currentProjectFile;
    std::unique_ptr<juce::Timer> autosaveTimer;

    void setupEdit();
};

} // namespace arrange
