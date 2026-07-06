#include "ProjectManager.h"

#if JUCE_WINDOWS
 #include <windows.h>
#else
 #include <signal.h>
 #include <unistd.h>
#endif

namespace skeletonhive
{

namespace
{
int getCurrentPid()
{
   #if JUCE_WINDOWS
    return (int) GetCurrentProcessId();
   #else
    return (int) getpid();
   #endif
}

juce::String lockFileContents (const juce::File& projectFile)
{
    return juce::String (getCurrentPid()) + "\n"
         + juce::String (juce::Time::getCurrentTime().toMilliseconds()) + "\n"
         + projectFile.getFullPathName();
}

bool parseLockPid (const juce::File& lockFile, int& pidOut)
{
    juce::StringArray lines;
    lockFile.readLines (lines);

    if (lines.isEmpty())
        return false;

    pidOut = lines[0].getIntValue();
    return pidOut > 0;
}

bool isProcessRunning (int pid)
{
   #if JUCE_WINDOWS
    auto handle = OpenProcess (PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD) pid);
    if (handle == nullptr)
        return false;

    DWORD exitCode = 0;
    const bool running = GetExitCodeProcess (handle, &exitCode) && exitCode == STILL_ACTIVE;
    CloseHandle (handle);
    return running;
   #else
    return kill ((pid_t) pid, 0) == 0;
   #endif
}
} // namespace

class ProjectManager::AutosaveTimer : public juce::Timer
{
public:
    explicit AutosaveTimer (ProjectManager& pm) : projectManager (pm) {}

    void timerCallback() override
    {
        projectManager.autosaveTick();
    }

private:
    ProjectManager& projectManager;
};

ProjectManager::ProjectManager (te::Engine& e)
    : engine (e), selectionManager (engine)
{
}

bool ProjectManager::isDirty() const
{
    return edit != nullptr && edit->hasChangedSinceSaved();
}

juce::String ProjectManager::getWindowTitle() const
{
    juce::String title = "SkeletonHive";

    if (currentProjectFile != juce::File())
        title << " - " << currentProjectFile.getFileNameWithoutExtension();

    if (isDirty())
        title << "*";

    return title;
}

ProjectManager::LoadResult ProjectManager::createNewProject (const juce::File& editFile,
                                                             juce::Component* parentForDialogs)
{
    if (! confirmDiscardOrSave (parentForDialogs))
        return LoadResult::cancelled;

    releaseProjectLock();

    selectionManager.deselectAll();
    edit = te::createEmptyEdit (engine, editFile);
    currentProjectFile = editFile;
    setupEdit();

    if (! te::EditFileOperations (*edit).save (true, true, false))
        return LoadResult::failed;

    recordFileModTime();
    acquireProjectLock();
    return LoadResult::success;
}

ProjectManager::LoadResult ProjectManager::loadProject (const juce::File& editFile,
                                                        juce::Component* parentForDialogs)
{
    if (! editFile.existsAsFile())
        return LoadResult::failed;

    if (! confirmDiscardOrSave (parentForDialogs))
        return LoadResult::cancelled;

    if (! checkProjectLock (editFile, parentForDialogs))
        return LoadResult::cancelled;

    releaseProjectLock();

    bool loadRecoveryVersion = false;

    if (! offerRecoveryIfNeeded (editFile, parentForDialogs, loadRecoveryVersion))
        return LoadResult::cancelled;

    const auto fileToLoad = loadRecoveryVersion
                                ? te::EditFileOperations::getTempVersionOfEditFile (editFile)
                                : editFile;

    if (! loadEditFromPath (fileToLoad))
        return LoadResult::failed;

    currentProjectFile = editFile;
    recordFileModTime();
    acquireProjectLock();

    if (loadRecoveryVersion)
        edit->markAsChanged();

    return LoadResult::success;
}

bool ProjectManager::loadEditFromPath (const juce::File& editFile)
{
    selectionManager.deselectAll();
    edit = te::loadEditFromFile (engine, editFile);
    setupEdit();
    return edit != nullptr;
}

ProjectManager::SaveResult ProjectManager::saveProject (bool forceSaveEvenIfNotModified,
                                                        bool collectExternalFiles,
                                                        juce::Component* parentForDialogs)
{
    if (edit == nullptr || currentProjectFile == juce::File())
        return SaveResult::failed;

    if (! forceSaveEvenIfNotModified && ! isDirty())
        return SaveResult::unchanged;

    switch (checkExternalChangesBeforeSave (parentForDialogs))
    {
        case ExternalChangeChoice::cancel:
            return SaveResult::cancelled;

        case ExternalChangeChoice::reload:
        {
            const auto path = currentProjectFile;
            releaseProjectLock();

            if (! loadEditFromPath (path))
                return SaveResult::failed;

            recordFileModTime();
            acquireProjectLock();
            return SaveResult::reloaded;
        }

        case ExternalChangeChoice::saveAs:
            return SaveResult::promptSaveAs;

        case ExternalChangeChoice::overwrite:
        default:
            break;
    }

    createRotatingSnapshot();

    const bool saved = te::EditFileOperations (*edit).save (true, forceSaveEvenIfNotModified, collectExternalFiles);

    if (saved)
    {
        te::EditFileOperations (*edit).deleteTempVersion();
        recordFileModTime();
        return SaveResult::success;
    }

    return SaveResult::failed;
}

ProjectManager::SaveResult ProjectManager::collectAllAndSave (juce::Component* parentForDialogs)
{
    return saveProject (true, true, parentForDialogs);
}

bool ProjectManager::saveProjectAs (const juce::File& editFile,
                                    bool collectExternalFiles,
                                    juce::Component* parentForDialogs)
{
    if (edit == nullptr || editFile == juce::File())
        return false;

    releaseProjectLock();

    if (! te::EditFileOperations (*edit).saveAs (editFile, collectExternalFiles))
        return false;

    currentProjectFile = editFile;
    te::EditFileOperations (*edit).deleteTempVersion();
    recordFileModTime();
    acquireProjectLock();
    juce::ignoreUnused (parentForDialogs);
    return true;
}

ProjectManager::UnsavedChoice ProjectManager::promptUnsavedChanges (juce::Component* parent) const
{
    if (! isDirty())
        return UnsavedChoice::discard;

    const int result = juce::NativeMessageBox::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                                     "Unsaved Changes",
                                                                     "Save changes before continuing?",
                                                                     parent);

    if (result == 1)
        return UnsavedChoice::save;

    if (result == 2)
        return UnsavedChoice::discard;

    return UnsavedChoice::cancel;
}

bool ProjectManager::confirmDiscardOrSave (juce::Component* parent)
{
    switch (promptUnsavedChanges (parent))
    {
        case UnsavedChoice::save:
            return saveProject (true, false, parent) == SaveResult::success;
        case UnsavedChoice::discard:
            return true;
        case UnsavedChoice::cancel:
        default:
            return false;
    }
}

void ProjectManager::enableAutosave (int intervalSeconds)
{
    autosaveTimer = std::make_unique<AutosaveTimer> (*this);
    autosaveTimer->startTimer (intervalSeconds * 1000);
}

void ProjectManager::prepareForShutdown()
{
    autosaveTimer = nullptr;
    saveProject (false, false, nullptr);
    releaseProjectLock();
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

void ProjectManager::recordFileModTime()
{
    if (currentProjectFile.existsAsFile())
        recordedFileModTime = currentProjectFile.getLastModificationTime().toMilliseconds();
    else
        recordedFileModTime = 0;
}

ProjectManager::ExternalChangeChoice ProjectManager::promptExternalChange (juce::Component* parent) const
{
    juce::ignoreUnused (parent);

    juce::AlertWindow alert ("External Changes Detected",
                             "This project file was modified outside SkeletonHive.",
                             juce::AlertWindow::WarningIcon);
    alert.addButton ("Overwrite", 1);
    alert.addButton ("Reload From Disk", 2);
    alert.addButton ("Save As...", 3);
    alert.addButton ("Cancel", 0);

    switch (alert.runModalLoop())
    {
        case 1: return ExternalChangeChoice::overwrite;
        case 2: return ExternalChangeChoice::reload;
        case 3: return ExternalChangeChoice::saveAs;
        default: return ExternalChangeChoice::cancel;
    }
}

ProjectManager::ExternalChangeChoice ProjectManager::checkExternalChangesBeforeSave (juce::Component* parent)
{
    if (currentProjectFile == juce::File() || ! currentProjectFile.existsAsFile())
        return ExternalChangeChoice::overwrite;

    if (recordedFileModTime == 0)
        return ExternalChangeChoice::overwrite;

    const auto currentModTime = currentProjectFile.getLastModificationTime().toMilliseconds();

    if (currentModTime == recordedFileModTime)
        return ExternalChangeChoice::overwrite;

    return promptExternalChange (parent);
}

bool ProjectManager::offerRecoveryIfNeeded (const juce::File& editFile,
                                            juce::Component* parent,
                                            bool& loadRecoveryVersion)
{
    loadRecoveryVersion = false;

    const auto tempFile = te::EditFileOperations::getTempVersionOfEditFile (editFile);

    if (! tempFile.existsAsFile())
        return true;

    if (! editFile.existsAsFile())
    {
        loadRecoveryVersion = true;
        return true;
    }

    if (tempFile.getLastModificationTime() <= editFile.getLastModificationTime())
        return true;

    const int result = juce::NativeMessageBox::showYesNoCancelBox (juce::MessageBoxIconType::QuestionIcon,
                                                                    "Recover Autosave",
                                                                    "A newer autosaved version of this project was found.\n\n"
                                                                    "Recover autosaved version?",
                                                                    parent,
                                                                    nullptr);

    if (result == 1)
    {
        loadRecoveryVersion = true;
        return true;
    }

    if (result == 2)
        return true;

    return false;
}

void ProjectManager::autosaveTick()
{
    if (edit == nullptr || currentProjectFile == juce::File())
        return;

    if (! isDirty())
        return;

    te::EditFileOperations (*edit).saveTempVersion (false);
}

juce::File ProjectManager::getAutosaveDirectory() const
{
    if (currentProjectFile == juce::File())
        return {};

    auto dir = currentProjectFile.getParentDirectory().getChildFile ("Autosave");
    dir.createDirectory();
    return dir;
}

void ProjectManager::createRotatingSnapshot()
{
    if (currentProjectFile == juce::File() || ! currentProjectFile.existsAsFile())
        return;

    const auto autosaveDir = getAutosaveDirectory();

    if (autosaveDir == juce::File())
        return;

    const auto timestamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    const auto snapshotName = currentProjectFile.getFileNameWithoutExtension()
                            + "-" + timestamp + currentProjectFile.getFileExtension();
    currentProjectFile.copyFileTo (autosaveDir.getChildFile (snapshotName));
    pruneSnapshots();
}

void ProjectManager::pruneSnapshots() const
{
    const auto autosaveDir = getAutosaveDirectory();

    if (autosaveDir == juce::File())
        return;

    juce::Array<juce::File> snapshots;

    for (const auto& entry : autosaveDir.findChildFiles (juce::File::findFiles, false))
        snapshots.add (entry);

    struct SnapshotSorter
    {
        static int compareElements (const juce::File& a, const juce::File& b)
        {
            if (a.getLastModificationTime() == b.getLastModificationTime())
                return 0;

            return a.getLastModificationTime() > b.getLastModificationTime() ? -1 : 1;
        }
    };

    SnapshotSorter sorter;
    snapshots.sort (sorter);

    for (int i = maxSnapshotCount; i < snapshots.size(); ++i)
        snapshots.getReference (i).deleteFile();
}

juce::File ProjectManager::getLockFileForProject (const juce::File& editFile) const
{
    return editFile.getSiblingFile (editFile.getFileNameWithoutExtension() + ".lock");
}

bool ProjectManager::isLockStale (const juce::File& lockFile) const
{
    int pid = 0;

    if (! parseLockPid (lockFile, pid))
        return true;

    return ! isProcessRunning (pid);
}

bool ProjectManager::checkProjectLock (const juce::File& editFile, juce::Component* parent)
{
    const auto lockFile = getLockFileForProject (editFile);

    if (! lockFile.existsAsFile())
        return true;

    if (isLockStale (lockFile))
    {
        lockFile.deleteFile();
        return true;
    }

    const int result = juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                                                   "Project Already Open",
                                                                   "This project appears to be open in another SkeletonHive instance.\n\n"
                                                                   "Open anyway?",
                                                                   parent,
                                                                   nullptr);

    return result != 0;
}

void ProjectManager::releaseProjectLock()
{
    if (activeLockFile != juce::File())
    {
        activeLockFile.deleteFile();
        activeLockFile = {};
    }
}

void ProjectManager::acquireProjectLock()
{
    releaseProjectLock();

    if (currentProjectFile == juce::File())
        return;

    activeLockFile = getLockFileForProject (currentProjectFile);
    activeLockFile.replaceWithText (lockFileContents (currentProjectFile));
}

} // namespace skeletonhive
