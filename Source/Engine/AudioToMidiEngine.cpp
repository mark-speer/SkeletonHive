#include "AudioToMidiEngine.h"
#include "AudioSampleExtractor.h"
#include "AudioToMidiAnalysis.h"
#include "AudioToMidiHarmony.h"
#include "EngineHelpers.h"
#include "WarpEngine.h"

namespace skeletonhive
{

namespace
{

juce::String modeLabel (AudioToMidiMode mode)
{
    switch (mode)
    {
        case AudioToMidiMode::melody:  return "Melody";
        case AudioToMidiMode::harmony: return "Harmony";
        case AudioToMidiMode::drums:   return "Drums";
    }

    return "MIDI";
}

TranscriptionResult analyseExtractedAudio (const ExtractedAudio& audio, AudioToMidiMode mode)
{
    if (! audio.success)
    {
        TranscriptionResult result;
        result.error = audio.error;
        return result;
    }

    switch (mode)
    {
        case AudioToMidiMode::melody:
            return AudioToMidiAnalysis::transcribeMelody (audio.mono, audio.sampleRate);

        case AudioToMidiMode::drums:
            return AudioToMidiAnalysis::transcribeDrums (audio.mono, audio.sampleRate);

        case AudioToMidiMode::harmony:
        {
            const auto chroma = AudioToMidiAnalysis::computeChromaFrames (audio.mono, audio.sampleRate);
            return AudioToMidiHarmony::transcribeHarmony (chroma, audio.sampleRate);
        }
    }

    return {};
}

} // namespace

TranscriptionResult AudioToMidiEngine::analyseClip (const te::AudioClipBase& sourceClip,
                                                    AudioToMidiMode mode)
{
    const auto audio = AudioSampleExtractor::extract (sourceClip);
    auto result = analyseExtractedAudio (audio, mode);

    if (! result.success && mode == AudioToMidiMode::drums)
    {
        const auto [ready, transients] = WarpEngine::getTransientTimesSeconds (sourceClip);

        if (ready && ! transients.isEmpty())
        {
            result = {};
            result.success = true;

            for (int i = 0; i < transients.size(); ++i)
            {
                const double start = transients[i];
                const double end = (i + 1 < transients.size())
                                       ? transients[i + 1]
                                       : start + 0.08;

                TranscribedNote note;
                note.sourceStartSeconds = start;
                note.sourceEndSeconds = end;
                note.pitch = 36;
                note.velocity = 100;
                result.notes.add (note);
            }
        }
    }

    if (result.success)
    {
        for (auto& note : result.notes)
        {
            note.sourceStartSeconds += audio.sourceStartSeconds;
            note.sourceEndSeconds += audio.sourceStartSeconds;
        }
    }

    return result;
}

double AudioToMidiEngine::mapSourceSecondsToTimelineSeconds (const te::AudioClipBase& clip,
                                                               double sourceSeconds)
{
    const double clipStart = clip.getPosition().getStart().inSeconds();
    const double offset = clip.getPosition().getOffset().inSeconds();

    if (WarpEngine::supportsWarp (clip) && WarpEngine::isWarpEnabled (clip))
    {
        const double warpTime = WarpEngine::sourceTimeToWarpTimeSeconds (clip, sourceSeconds);
        return clipStart + (warpTime - offset);
    }

    const double speed = juce::jmax (0.01, clip.getSpeedRatio());
    return clipStart + (sourceSeconds - offset) / speed;
}

double AudioToMidiEngine::mapTimelineSecondsToSequenceBeat (te::Edit& edit,
                                                            const te::MidiClip& midiClip,
                                                            double timelineSeconds)
{
    const auto& ts = edit.tempoSequence;
    return ts.toBeats (te::TimePosition::fromSeconds (timelineSeconds)).inBeats()
         + midiClip.getOffsetInBeats().inBeats();
}

te::MidiClip* AudioToMidiEngine::convertClip (te::Edit& edit,
                                              te::AudioClipBase& sourceClip,
                                              AudioToMidiMode mode,
                                              juce::String* errorMessage)
{
    const auto transcription = analyseClip (sourceClip, mode);
    return createMidiClipFromTranscription (edit, sourceClip, transcription, mode, errorMessage);
}

te::MidiClip* AudioToMidiEngine::createMidiClipFromTranscription (te::Edit& edit,
                                                                  te::AudioClipBase& sourceClip,
                                                                  const TranscriptionResult& transcription,
                                                                  AudioToMidiMode mode,
                                                                  juce::String* errorMessage)
{
    auto fail = [errorMessage] (const juce::String& message) -> te::MidiClip*
    {
        if (errorMessage != nullptr)
            *errorMessage = message;
        return nullptr;
    };

    if (! transcription.success)
        return fail (transcription.error.isNotEmpty() ? transcription.error
                                                       : "Audio-to-MIDI conversion failed.");

    auto* sourceTrack = sourceClip.getClipTrack();
    if (sourceTrack == nullptr)
        return fail ("Audio clip has no parent track.");

    const int sourceIndex = EngineHelpers::getArrangementTrackIndex (edit, *sourceTrack);
    auto* midiTrack = EngineHelpers::getOrInsertTrackForMidi (edit, juce::jmax (0, sourceIndex + 1));

    if (midiTrack == nullptr)
        return fail ("Could not create a MIDI track for the conversion result.");

    const auto clipRange = sourceClip.getPosition().time;
    const auto clipName = sourceClip.getName() + " (" + modeLabel (mode) + ")";

    edit.getUndoManager().beginNewTransaction ("Convert to MIDI");

    auto midiClip = EngineHelpers::createMidiClipOnTrack (*midiTrack, clipRange, clipName);

    if (midiClip == nullptr)
        return fail ("Could not create the MIDI clip.");

    const int transpose = (int) std::lround (sourceClip.getPitchChange());

    for (const auto& note : transcription.notes)
    {
        const double startTimeline = mapSourceSecondsToTimelineSeconds (sourceClip, note.sourceStartSeconds);
        const double endTimeline = mapSourceSecondsToTimelineSeconds (sourceClip, note.sourceEndSeconds);
        const double startBeat = mapTimelineSecondsToSequenceBeat (edit, *midiClip, startTimeline);
        const double endBeat = mapTimelineSecondsToSequenceBeat (edit, *midiClip, endTimeline);
        const double lengthBeats = juce::jmax (0.0625, endBeat - startBeat);
        const int pitch = juce::jlimit (0, 127, note.pitch + transpose);

        midiClip->getSequence().addNote (pitch,
                                         te::BeatPosition::fromBeats (startBeat),
                                         te::BeatDuration::fromBeats (lengthBeats),
                                         (uint8_t) juce::jlimit (1, 127, note.velocity),
                                         0,
                                         &edit.getUndoManager());
    }

    return midiClip.get();
}

} // namespace skeletonhive
