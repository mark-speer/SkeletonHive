#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioToMidiTypes.h"
#include "TracktionCommon.h"

namespace skeletonhive
{

struct ExtractedAudio
{
    bool success = false;
    juce::String error;
    juce::AudioBuffer<float> mono;
    double sampleRate = audioToMidiAnalysisSampleRate;
    double sourceStartSeconds = 0.0;
};

/** Loads the audible source region of an audio clip into a mono float buffer. */
class AudioSampleExtractor
{
public:
    static ExtractedAudio extract (const te::AudioClipBase& clip);
};

} // namespace skeletonhive
