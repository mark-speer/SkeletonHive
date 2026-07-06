#include "ClipLibraryManager.h"

namespace skeletonhive
{

namespace
{
constexpr const char* presetRootTag = "SkeletonHiveClipPreset";
constexpr const char* clipStateTag = "CLIP";
constexpr const char* mediaFolderName = "media";

juce::File getClipLibraryRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("SkeletonHive")
               .getChildFile ("ClipLibrary");
}

juce::String relativeMediaPath (const juce::File& presetFolder, const juce::File& mediaFile)
{
    return mediaFile.getRelativePathFrom (presetFolder);
}

void walkStateReplacePath (juce::ValueTree state, const juce::String& oldPath, const juce::String& newPath)
{
    static const juce::Identifier sourceId ("source");
    static const juce::Identifier fileId ("file");

    if (state.hasProperty (sourceId) && state.getProperty (sourceId).toString() == oldPath)
        state.setProperty (sourceId, newPath, nullptr);

    if (state.hasProperty (fileId) && state.getProperty (fileId).toString() == oldPath)
        state.setProperty (fileId, newPath, nullptr);

    for (int i = 0; i < state.getNumChildren(); ++i)
        walkStateReplacePath (state.getChild (i), oldPath, newPath);
}

double readLengthFromState (const juce::ValueTree& clipState)
{
    if (clipState.hasProperty ("length"))
        return (double) clipState.getProperty ("length");

    return 0.0;
}
} // namespace

ClipLibraryManager::ClipLibraryManager (te::Engine& engine)
    : engineRef (engine)
{
    getLibraryRoot().createDirectory();
    refresh();
}

juce::File ClipLibraryManager::getLibraryRoot() const
{
    return getClipLibraryRoot();
}

void ClipLibraryManager::refresh()
{
    entries.clear();

    const auto root = getLibraryRoot();
    if (! root.isDirectory())
        return;

    for (const auto& iter : juce::RangedDirectoryIterator (root, true, "*.clip.xml", juce::File::findFiles))
    {
        const auto presetFile = iter.getFile();
        const auto preset = loadPresetTree (presetFile);

        if (! preset.isValid())
            continue;

        ClipLibraryEntry entry;
        entry.presetFile = presetFile;
        entry.name = preset.getProperty ("name", presetFile.getFileNameWithoutExtension()).toString();
        entry.category = preset.getProperty ("category", "User").toString();
        entry.clipType = preset.getProperty ("clipType", "clip").toString();
        entry.modifiedTimeMs = presetFile.getLastModificationTime().toMilliseconds();

        if (auto clipState = preset.getChildWithName (clipStateTag); clipState.isValid())
            entry.lengthSeconds = readLengthFromState (clipState);

        entries.add (std::move (entry));
    }

    struct Sorter
    {
        ClipLibrarySortMode mode;

        int compareElements (const ClipLibraryEntry& a, const ClipLibraryEntry& b) const
        {
            if (mode == ClipLibrarySortMode::dateModified)
            {
                if (a.modifiedTimeMs == b.modifiedTimeMs) return 0;
                return a.modifiedTimeMs > b.modifiedTimeMs ? -1 : 1;
            }

            return a.name.compareIgnoreCase (b.name);
        }
    };

    Sorter sorter { ClipLibrarySortMode::name };
    entries.sort (sorter);
    sendChangeMessage();
}

juce::Array<ClipLibraryEntry> ClipLibraryManager::getEntries (const juce::String& searchQuery,
                                                              ClipLibrarySortMode sort) const
{
    juce::Array<ClipLibraryEntry> filtered;
    const auto query = searchQuery.trim().toLowerCase();

    for (const auto& entry : entries)
    {
        if (query.isNotEmpty()
            && ! entry.name.toLowerCase().contains (query)
            && ! entry.category.toLowerCase().contains (query))
            continue;

        filtered.add (entry);
    }

    struct Sorter
    {
        ClipLibrarySortMode mode;

        int compareElements (const ClipLibraryEntry& a, const ClipLibraryEntry& b) const
        {
            if (mode == ClipLibrarySortMode::dateModified)
            {
                if (a.modifiedTimeMs == b.modifiedTimeMs) return 0;
                return a.modifiedTimeMs > b.modifiedTimeMs ? -1 : 1;
            }

            return a.name.compareIgnoreCase (b.name);
        }
    };

    Sorter sorter { sort };
    filtered.sort (sorter);
    return filtered;
}

juce::String ClipLibraryManager::clipTypeFor (const te::Clip& clip)
{
    if (dynamic_cast<const te::WaveAudioClip*> (&clip) != nullptr)
        return "audio";

    if (dynamic_cast<const te::MidiClip*> (&clip) != nullptr)
        return "midi";

    return "clip";
}

void ClipLibraryManager::normalizeClipStateForExport (juce::ValueTree& clipState, te::TimePosition clipStart)
{
    if (clipState.hasProperty ("start"))
        clipState.setProperty ("start", 0.0, nullptr);

    juce::ignoreUnused (clipStart);
}

void ClipLibraryManager::replacePathInState (juce::ValueTree& state, const juce::String& oldPath, const juce::String& newPath)
{
    walkStateReplacePath (state, oldPath, newPath);
}

void ClipLibraryManager::copyReferencedMedia (te::Clip& clip, const juce::File& presetFolder, juce::ValueTree& clipState)
{
    if (auto* wave = dynamic_cast<te::WaveAudioClip*> (&clip))
    {
        const auto sourceFile = wave->getAudioFile().getFile();

        if (! sourceFile.existsAsFile())
            return;

        const auto mediaDir = presetFolder.getChildFile (mediaFolderName);
        mediaDir.createDirectory();

        const auto destFile = mediaDir.getNonexistentChildFile (sourceFile.getFileNameWithoutExtension(),
                                                                sourceFile.getFileExtension());

        if (sourceFile.copyFileTo (destFile))
        {
            const auto relative = relativeMediaPath (presetFolder, destFile);
            replacePathInState (clipState, sourceFile.getFullPathName(), relative);
        }
    }
}

void ClipLibraryManager::resolveMediaPaths (const juce::File& presetFolder, juce::ValueTree& clipState)
{
    const auto mediaDir = presetFolder.getChildFile (mediaFolderName);

    std::function<void (juce::ValueTree)> resolveNode = [&] (juce::ValueTree state)
    {
        static const juce::Identifier sourceId ("source");
        static const juce::Identifier fileId ("file");

        for (const auto& prop : { sourceId, fileId })
        {
            if (! state.hasProperty (prop))
                continue;

            const auto path = state.getProperty (prop).toString();

            if (path.isEmpty() || juce::File::isAbsolutePath (path))
                continue;

            const auto resolved = presetFolder.getChildFile (path);

            if (resolved.existsAsFile())
                state.setProperty (prop, resolved.getFullPathName(), nullptr);
            else if (mediaDir.getChildFile (juce::File (path).getFileName()).existsAsFile())
                state.setProperty (prop, mediaDir.getChildFile (juce::File (path).getFileName()).getFullPathName(), nullptr);
        }

        for (int i = 0; i < state.getNumChildren(); ++i)
            resolveNode (state.getChild (i));
    };

    resolveNode (clipState);
}

juce::ValueTree ClipLibraryManager::loadPresetTree (const juce::File& presetFile)
{
    if (auto xml = juce::XmlDocument::parse (presetFile))
        return juce::ValueTree::fromXml (*xml);

    return {};
}

bool ClipLibraryManager::writePresetTree (const juce::File& presetFile, const juce::ValueTree& preset)
{
    presetFile.getParentDirectory().createDirectory();

    if (auto xml = preset.createXml())
        return xml->writeTo (presetFile);

    return false;
}

juce::File ClipLibraryManager::saveClip (te::Clip& clip, const juce::String& name, const juce::String& category)
{
    const auto trimmedName = name.trim();

    if (trimmedName.isEmpty())
        return {};

    const auto legalName = juce::File::createLegalFileName (trimmedName);
    const auto presetFolder = getLibraryRoot().getChildFile (legalName);
    presetFolder.createDirectory();

    const auto presetFile = presetFolder.getChildFile ("preset.clip.xml");

    auto clipState = clip.state.createCopy();
    normalizeClipStateForExport (clipState, clip.getPosition().getStart());
    copyReferencedMedia (clip, presetFolder, clipState);

    juce::ValueTree preset (presetRootTag);
    preset.setProperty ("name", trimmedName, nullptr);
    preset.setProperty ("category", category.isNotEmpty() ? category : "User", nullptr);
    preset.setProperty ("clipType", clipTypeFor (clip), nullptr);
    preset.setProperty ("version", 1, nullptr);
    preset.addChild (clipState, -1, nullptr);

    if (! writePresetTree (presetFile, preset))
        return {};

    refresh();
    return presetFile;
}

te::Clip* ClipLibraryManager::findClipById (te::Edit& edit, te::EditItemID clipId) const
{
    for (auto track : te::getAllTracks (edit))
    {
        auto* clipTrack = dynamic_cast<te::ClipTrack*> (track);

        if (clipTrack == nullptr)
            continue;

        for (auto* clip : clipTrack->getClips())
        {
            if (clip != nullptr && clip->itemID == clipId)
                return clip;
        }
    }

    return nullptr;
}

te::Clip* ClipLibraryManager::instantiateClip (te::ClipTrack& track, te::TimePosition start, const juce::File& presetFile)
{
    const auto preset = loadPresetTree (presetFile);

    if (! preset.isValid())
        return nullptr;

    auto clipState = preset.getChildWithName (clipStateTag).createCopy();

    if (! clipState.isValid())
        return nullptr;

    resolveMediaPaths (presetFile.getParentDirectory(), clipState);

    track.edit.createNewItemID().writeID (clipState, nullptr);
    te::assignNewIDsToAutomationCurveModifiers (track.edit, clipState);

    auto* newClip = track.insertClipWithState (clipState);

    if (newClip != nullptr)
        newClip->setStart (start, false, true);

    return newClip;
}

bool ClipLibraryManager::deletePreset (const juce::File& presetFile)
{
    if (! presetFile.existsAsFile())
        return false;

    const auto folder = presetFile.getParentDirectory();
    const bool removed = presetFile.deleteFile();

    if (folder.isDirectory() && folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0)
        folder.deleteRecursively();

    refresh();
    return removed;
}

} // namespace skeletonhive
