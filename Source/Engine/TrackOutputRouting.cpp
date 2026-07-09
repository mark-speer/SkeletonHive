#include "TrackOutputRouting.h"
#include "EngineHelpers.h"

namespace skeletonhive
{

namespace
{

TrackOutputOption makeMasterOption (bool isMidi)
{
    TrackOutputOption opt;
    opt.type = TrackOutputOption::Type::master;
    opt.isMidi = isMidi;
    opt.displayName = isMidi ? "Master (MIDI)" : "Master";
    opt.available = true;
    return opt;
}

TrackOutputOption makeNoneOption()
{
    TrackOutputOption opt;
    opt.type = TrackOutputOption::Type::none;
    opt.displayName = "Master";
    return opt;
}

juce::String formatTrackDestName (int oneBasedIndex, const juce::String& trackName)
{
    return juce::String (oneBasedIndex) + ". " + trackName;
}

juce::Array<te::AudioTrack*> getTrackDestCandidates (te::Edit& edit, te::AudioTrack& dest)
{
    juce::Array<te::AudioTrack*> candidates;

    for (auto* t : te::getAudioTracks (edit))
    {
        if (auto* at = dynamic_cast<te::AudioTrack*> (t))
        {
            if (at->itemID == dest.itemID)
                continue;

            if (at->isFolderTrack())
                continue;

            candidates.add (at);
        }
    }

    return candidates;
}

bool isMidiKindOutput (const te::AudioTrack& dest)
{
    return EngineHelpers::isMidiKindTrack (dest);
}

} // namespace

bool TrackOutputOption::operator== (const TrackOutputOption& other) const noexcept
{
    if (type != other.type)
        return false;

    if (type == Type::none || type == Type::master)
        return isMidi == other.isMidi;

    if (type == Type::hardware)
        return deviceId == other.deviceId && isMidi == other.isMidi;

    if (type == Type::track || type == Type::auxReturn)
        return trackId == other.trackId;

    return true;
}

juce::Array<TrackOutputOption> TrackOutputRouting::getOutputOptions (te::Edit& edit, te::AudioTrack& dest)
{
    juce::Array<TrackOutputOption> options;
    const bool midiKind = isMidiKindOutput (dest);

    options.add (makeMasterOption (midiKind));

    juce::StringArray deviceNames, aliases;
    juce::BigInteger hasAudio, hasMidi;
    const auto trackCandidates = getTrackDestCandidates (edit, dest);

    te::TrackOutput::getPossibleOutputDeviceNames (trackCandidates, deviceNames, aliases,
                                                   hasAudio, hasMidi);

    for (int i = 0; i < deviceNames.size(); ++i)
    {
        const bool isMidiDev = hasMidi[i];
        const bool isAudioDev = hasAudio[i];

        if (midiKind && ! isMidiDev)
            continue;

        if (! midiKind && ! isAudioDev)
            continue;

        if (i <= 1)
            continue; // skip default entries already covered by master

        TrackOutputOption opt;
        opt.type = TrackOutputOption::Type::hardware;
        opt.deviceId = deviceNames[i];
        opt.displayName = aliases[i].isNotEmpty() ? aliases[i] : deviceNames[i];
        opt.isMidi = isMidiDev;
        opt.available = true;
        options.add (opt);
    }

    for (int i = 0; i < trackCandidates.size(); ++i)
    {
        auto* candidate = trackCandidates[i];

        TrackOutputOption opt;
        opt.type = EngineHelpers::isReturnTrack (*candidate)
                       ? TrackOutputOption::Type::auxReturn
                       : TrackOutputOption::Type::track;
        opt.trackId = candidate->itemID;
        opt.displayName = formatTrackDestName (i + 1, candidate->getName());
        opt.available = ! wouldCreateRoutingLoop (dest, *candidate);
        options.add (opt);
    }

    return options;
}

TrackOutputOption TrackOutputRouting::getActiveOutput (te::AudioTrack& dest)
{
    auto& output = dest.getOutput();
    const bool midiKind = isMidiKindOutput (dest);

    if (auto* destTrack = output.getDestinationTrack())
    {
        TrackOutputOption opt;
        opt.type = EngineHelpers::isReturnTrack (*destTrack)
                       ? TrackOutputOption::Type::auxReturn
                       : TrackOutputOption::Type::track;
        opt.trackId = destTrack->itemID;
        opt.displayName = destTrack->getNameAsTrackNumber();
        opt.available = true;

        const auto candidates = getTrackDestCandidates (dest.edit, dest);
        for (int i = 0; i < candidates.size(); ++i)
        {
            if (candidates[i]->itemID == opt.trackId)
            {
                opt.displayName = formatTrackDestName (i + 1, destTrack->getName());
                break;
            }
        }

        return opt;
    }

    const auto name = output.getOutputName();

    if (output.usesDefaultAudioOut() || output.usesDefaultMIDIOut()
        || name.isEmpty())
        return makeMasterOption (midiKind);

    TrackOutputOption opt;
    opt.type = TrackOutputOption::Type::hardware;
    opt.deviceId = output.getOutputDeviceID();
    opt.displayName = name;
    opt.isMidi = output.canPlayMidi() && ! output.canPlayAudio();
    opt.available = output.getOutputDevice (false) != nullptr;

    if (! opt.available)
        opt.displayName = "[Missing] " + name;

    return opt;
}

bool TrackOutputRouting::wouldCreateRoutingLoop (te::AudioTrack& source, te::AudioTrack& candidateDest)
{
    if (&source == &candidateDest)
        return true;

    if (source.getOutput().feedsInto (&candidateDest))
        return true;

    if (candidateDest.getOutput().feedsInto (&source))
        return true;

    return false;
}

bool TrackOutputRouting::setActiveOutput (te::AudioTrack& dest, const TrackOutputOption& option,
                                          juce::String& errorOut)
{
    TRACKTION_ASSERT_MESSAGE_THREAD

    if (dest.edit.getTransport().isRecording())
    {
        errorOut = "Cannot change output routing while recording";
        return false;
    }

    auto& output = dest.getOutput();
    const bool midiKind = isMidiKindOutput (dest);

    switch (option.type)
    {
        case TrackOutputOption::Type::master:
            output.setOutputToDefaultDevice (midiKind);
            return true;

        case TrackOutputOption::Type::hardware:
            if (option.deviceId.isEmpty())
            {
                errorOut = "Invalid hardware output";
                return false;
            }
            output.setOutputToDeviceID (option.deviceId);
            return true;

        case TrackOutputOption::Type::track:
        case TrackOutputOption::Type::auxReturn:
        {
            if (auto* destTrack = te::findAudioTrackForID (dest.edit, option.trackId))
            {
                if (wouldCreateRoutingLoop (dest, *destTrack))
                {
                    errorOut = "Routing would create a loop";
                    return false;
                }

                output.setOutputToTrack (destTrack);
                return true;
            }

            errorOut = "Destination track not found";
            return false;
        }

        case TrackOutputOption::Type::none:
        default:
            output.setOutputToDefaultDevice (midiKind);
            return true;
    }
}

bool TrackOutputRouting::shouldShowOutputSelector (const te::Track& track)
{
    if (track.isFolderTrack())
        return false;

    return dynamic_cast<const te::AudioTrack*> (&track) != nullptr;
}

juce::String TrackOutputRouting::getOutputTooltip (te::AudioTrack& dest)
{
    return dest.getOutput().getDescriptiveOutputName();
}

} // namespace skeletonhive
