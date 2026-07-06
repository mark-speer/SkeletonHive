#include "SessionClipPlaybackResolver.h"
#include "EngineHelpers.h"
#include "MidiScale.h"

#include <algorithm>
#include <map>
#include <vector>

namespace skeletonhive
{

namespace
{
struct NoteMuteSnapshot
{
    te::EditItemID clipId;
    juce::Array<juce::ValueTree> noteStates;
    juce::Array<bool> originalMute;
};

struct IterationGroupEntry
{
    te::MidiNote* note = nullptr;
    double startBeat = 0.0;
    int pitch = 0;
};

std::map<te::EditItemID, NoteMuteSnapshot>& playbackSnapshots()
{
    static std::map<te::EditItemID, NoteMuteSnapshot> snapshots;
    return snapshots;
}

void ensureSnapshot (te::MidiClip& clip)
{
    auto& snapshots = playbackSnapshots();
    const auto id = clip.itemID;

    if (snapshots.find (id) != snapshots.end())
        return;

    NoteMuteSnapshot snapshot;
    snapshot.clipId = id;

    for (auto* note : clip.getSequence().getNotes())
    {
        snapshot.noteStates.add (note->state);
        snapshot.originalMute.add (note->isMute());
    }

    snapshots.emplace (id, std::move (snapshot));
}

void applyMasksToClip (te::MidiClip& clip, int loopCycleIndex, juce::Random& rng)
{
    if (! EngineHelpers::isSessionClip (clip))
        return;

    ensureSnapshot (clip);

    const int root = EngineHelpers::getClipScaleRoot (clip);
    const auto mode = EngineHelpers::getClipScaleMode (clip);
    const bool scaleLock = EngineHelpers::getClipScaleLock (clip);

    std::map<int, std::vector<IterationGroupEntry>> iterationGroups;

    for (auto* note : clip.getSequence().getNotes())
    {
        const int iteration = EngineHelpers::getNoteIteration (note->state);
        if (iteration > 0)
        {
            IterationGroupEntry entry;
            entry.note = note;
            entry.startBeat = note->getStartBeat().inBeats();
            entry.pitch = note->getNoteNumber();
            iterationGroups[iteration].push_back (entry);
        }
    }

    for (auto& pair : iterationGroups)
    {
        auto& group = pair.second;
        std::sort (group.begin(), group.end(), [] (const IterationGroupEntry& a, const IterationGroupEntry& b)
        {
            if (a.startBeat != b.startBeat)
                return a.startBeat < b.startBeat;
            return a.pitch < b.pitch;
        });
    }

    for (auto* note : clip.getSequence().getNotes())
    {
        bool mute = false;

        if (scaleLock && mode != ScaleMode::none
            && ! MidiScale::isPitchInScale (note->getNoteNumber(), root, mode))
        {
            mute = true;
        }

        if (! mute)
        {
            const int iteration = EngineHelpers::getNoteIteration (note->state);
            if (iteration > 0)
            {
                const auto it = iterationGroups.find (iteration);
                if (it != iterationGroups.end() && ! it->second.empty())
                {
                    const auto& group = it->second;
                    const int activeIndex = loopCycleIndex % (int) group.size();
                    mute = group[(size_t) activeIndex].note != note;
                }
            }
        }

        if (! mute)
        {
            const int probability = EngineHelpers::getNoteProbability (note->state);
            if (probability < 100 && rng.nextInt (100) >= probability)
                mute = true;
        }

        note->setMute (mute, nullptr);
    }
}
} // namespace

void SessionClipPlaybackResolver::applyPlaybackMasks (te::MidiClip& clip, int loopCycleIndex, juce::Random& rng)
{
    applyMasksToClip (clip, loopCycleIndex, rng);
}

void SessionClipPlaybackResolver::restorePlaybackMasks (te::MidiClip& clip)
{
    auto& snapshots = playbackSnapshots();
    const auto it = snapshots.find (clip.itemID);
    if (it == snapshots.end())
        return;

    const auto& snapshot = it->second;

    for (auto* note : clip.getSequence().getNotes())
    {
        for (int i = 0; i < snapshot.noteStates.size(); ++i)
        {
            if (note->state == snapshot.noteStates.getReference (i))
            {
                note->setMute (snapshot.originalMute[i], nullptr);
                break;
            }
        }
    }

    snapshots.erase (it);
}

bool SessionClipPlaybackResolver::hasActiveMasks (const te::MidiClip& clip)
{
    return playbackSnapshots().find (clip.itemID) != playbackSnapshots().end();
}

} // namespace skeletonhive
