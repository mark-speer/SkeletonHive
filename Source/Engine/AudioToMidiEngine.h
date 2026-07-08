#pragma once

#include "AudioToMidiTypes.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

class AudioToMidiEngine
{
public:
    static te::MidiClip* convertClip (te::Edit& edit,
                                      te::AudioClipBase& sourceClip,
                                      AudioToMidiMode mode,
                                      juce::String* errorMessage = nullptr);

    static te::MidiClip* createMidiClipFromTranscription (te::Edit& edit,
                                                          te::AudioClipBase& sourceClip,
                                                          const TranscriptionResult& transcription,
                                                          AudioToMidiMode mode,
                                                          juce::String* errorMessage = nullptr);

    static TranscriptionResult analyseClip (const te::AudioClipBase& sourceClip,
                                            AudioToMidiMode mode);

    static double mapSourceSecondsToTimelineSeconds (const te::AudioClipBase& clip,
                                                     double sourceSeconds);

    static double mapTimelineSecondsToSequenceBeat (te::Edit& edit,
                                                    const te::MidiClip& midiClip,
                                                    double timelineSeconds);
};

} // namespace skeletonhive
