#include "AraHelpers.h"

namespace skeletonhive
{

bool AraHelpers::isUsingAra (const te::AudioClipBase& clip)
{
   #if SKELETONHIVE_HAS_ARA
    return clip.isUsingARA();
   #else
    juce::ignoreUnused (clip);
    return false;
   #endif
}

void AraHelpers::showAraWindow (te::AudioClipBase& clip)
{
   #if SKELETONHIVE_HAS_ARA
    te::showARAWindow (clip);
   #else
    juce::ignoreUnused (clip);
   #endif
}

void AraHelpers::hideAraWindow (te::AudioClipBase& clip)
{
   #if SKELETONHIVE_HAS_ARA
    te::hideARAWindow (clip);
   #else
    juce::ignoreUnused (clip);
   #endif
}

void AraHelpers::convertAraToMidi (te::AudioClipBase& clip)
{
   #if SKELETONHIVE_HAS_ARA
    te::araConvertToMIDI (clip);
   #else
    juce::ignoreUnused (clip);
   #endif
}

} // namespace skeletonhive
