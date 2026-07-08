#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "AudioToMidiTypes.h"

namespace skeletonhive
{

struct ChromaFrame
{
    float bins[12] {};
};

class AudioToMidiAnalysis
{
public:
    static TranscriptionResult transcribeMelody (const juce::AudioBuffer<float>& mono,
                                                 double sampleRate);

    static TranscriptionResult transcribeDrums (const juce::AudioBuffer<float>& mono,
                                                double sampleRate);

    static juce::Array<ChromaFrame> computeChromaFrames (const juce::AudioBuffer<float>& mono,
                                                           double sampleRate,
                                                           int hopSize = 512,
                                                           int fftSize = 2048);
};

} // namespace skeletonhive
