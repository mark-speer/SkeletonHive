#pragma once

#include "AudioToMidiAnalysis.h"
#include "AudioToMidiTypes.h"

namespace skeletonhive
{

class AudioToMidiHarmony
{
public:
    static TranscriptionResult transcribeHarmony (const juce::Array<ChromaFrame>& chromaFrames,
                                                  double sampleRate,
                                                  int hopSize = 512);
};

} // namespace skeletonhive
