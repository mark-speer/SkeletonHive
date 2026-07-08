#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

#if JUCE_DEBUG
struct EngineBenchmarkHarness
{
    struct Results
    {
        double populateMs = 0.0;
        double freezeMs = 0.0;
        double renderMs = 0.0;
        double cacheReadMs = 0.0;
        int trackCount = 0;
    };

    static Results runFullSuite (te::Engine& engine, te::Edit& edit, int trackCount = 50);
    static void logResults (const Results& results);

    static double populateAudioStressProject (te::Edit& edit, int trackCount, int clipsPerTrack);
    static double measureFreezeMs (te::AudioTrack& track);
    static double measureRenderMs (te::Edit& edit);
    static double logAudioFileCacheStats (te::Engine& engine);
};
#endif

} // namespace skeletonhive
