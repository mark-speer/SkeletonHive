#pragma once

#include "TracktionCommon.h"

#ifndef SKELETONHIVE_HAS_ARA
 #define SKELETONHIVE_HAS_ARA 0
#endif

namespace skeletonhive
{

/** Thin wrappers around Tracktion Engine ARA clip APIs.
    When the build was configured without SKELETONHIVE_ENABLE_ARA, these are
    no-ops / false so UI can call them without per-call ifdefs. */
struct AraHelpers
{
    static constexpr bool isBuildEnabled() noexcept
    {
       #if SKELETONHIVE_HAS_ARA
        return true;
       #else
        return false;
       #endif
    }

    static bool isUsingAra (const te::AudioClipBase& clip);
    static void showAraWindow (te::AudioClipBase& clip);
    static void hideAraWindow (te::AudioClipBase& clip);
    static void convertAraToMidi (te::AudioClipBase& clip);
};

} // namespace skeletonhive
