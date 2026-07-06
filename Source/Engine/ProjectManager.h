#pragma once

#include "EngineHelpers.h"
#include "ExtendedUIBehaviour.h"

namespace skeletonhive
{

class ProjectManager
{
public:
    enum class UnsavedChoice
    {
        save,
        discard,
        cancel
    };

    enum class ExternalChangeChoice
    {
        overwrite,
        reload,
        saveAs,
        cancel
    };

    enum class LoadResult
    {
        success,
        failed,
        cancelled
    };

    enum class SaveResult
    {
        success,
        unchanged,
        cancelled,
        failed,
        reloaded,
        promptSaveAs
    };

    explicit ProjectManager (te::Engine& engine);

    te::Engine& getEngine() { return engine; }
    te::Edit* getEdit() { return edit.get(); }
    te::SelectionManager& getSelectionManager() { return selectionManager; }
    te::EditInsertPoint* getInsertPoint() { return insertPoint.get(); }

    bool isDirty() const;
    juce::String getWindowTitle() const;

    LoadResult createNewProject (const juce::File& editFile, juce::Component* parentForDialogs = nullptr);
    LoadResult loadProject (const juce::File& editFile, juce::Component* parentForDialogs = nullptr);
    SaveResult saveProject (bool forceSaveEvenIfNotModified = false, juce::Component* parentForDialogs = nullptr);
    bool saveProjectAs (const juce::File& editFile, juce::Component* parentForDialogs = nullptr);

    juce::File getCurrentProjectFile() const { return currentProjectFile; }

    UnsavedChoice promptUnsavedChanges (juce::Component* parent) const;
    bool confirmDiscardOrSave (juce::Component* parent);

    void enableAutosave (int intervalSeconds = 60);
    void prepareForShutdown();

private:
    class AutosaveTimer;

    te::Engine& engine;
    te::SelectionManager selectionManager;
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<te::EditInsertPoint> insertPoint;
    juce::File currentProjectFile;
    juce::int64 recordedFileModTime = 0;
    juce::File activeLockFile;
    std::unique_ptr<juce::Timer> autosaveTimer;

    static constexpr int maxSnapshotCount = 10;

    void setupEdit();
    void recordFileModTime();
    ExternalChangeChoice checkExternalChangesBeforeSave (juce::Component* parent);
    ExternalChangeChoice promptExternalChange (juce::Component* parent) const;
    bool offerRecoveryIfNeeded (const juce::File& editFile, juce::Component* parent, bool& loadRecoveryVersion);
    bool loadEditFromPath (const juce::File& editFile);
    void createRotatingSnapshot();
    void pruneSnapshots() const;
    juce::File getAutosaveDirectory() const;
    juce::File getLockFileForProject (const juce::File& editFile) const;
    bool isLockStale (const juce::File& lockFile) const;
    bool checkProjectLock (const juce::File& editFile, juce::Component* parent);
    void releaseProjectLock();
    void acquireProjectLock();
    void autosaveTick();
};

} // namespace skeletonhive
