#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

struct SamplerExcerpt
{
    double startTime = 0.0;
    double length = 0.0;
};

/** Shared helpers for te::SamplerPlugin region and sample assignment. */
class SamplerHelpers
{
public:
    static double getEffectiveLength (const te::SamplerPlugin& sampler, int soundIndex);
    static SamplerExcerpt clampExcerpt (const te::AudioFile& audioFile, double startTime, double length);
    static juce::String assignSample (te::SamplerPlugin& sampler, const juce::File& file, int keyNote);
};

} // namespace skeletonhive
