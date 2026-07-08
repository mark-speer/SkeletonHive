#pragma once

#include <juce_core/juce_core.h>

namespace skeletonhive
{

enum class AudioToMidiMode
{
    melody,
    harmony,
    drums
};

struct TranscribedNote
{
    double sourceStartSeconds = 0.0;
    double sourceEndSeconds = 0.0;
    int pitch = 60;
    int velocity = 100;
};

struct TranscriptionResult
{
    bool success = false;
    juce::String error;
    juce::Array<TranscribedNote> notes;
};

constexpr double audioToMidiAnalysisSampleRate = 44100.0;

} // namespace skeletonhive
