#include "SamplerHelpers.h"

namespace skeletonhive
{

double SamplerHelpers::getEffectiveLength (const te::SamplerPlugin& sampler, int soundIndex)
{
    const double storedLength = sampler.getSoundLength (soundIndex);

    if (storedLength > 0.0)
        return storedLength;

    const auto audioFile = sampler.getSoundFile (soundIndex);

    if (! audioFile.isValid())
        return 0.0;

    const double startTime = sampler.getSoundStartTime (soundIndex);
    return juce::jmax (0.0, audioFile.getLength() - startTime);
}

SamplerExcerpt SamplerHelpers::clampExcerpt (const te::AudioFile& audioFile, double startTime, double length)
{
    SamplerExcerpt excerpt;

    if (! audioFile.isValid())
        return excerpt;

    const double fileLength = audioFile.getLength();
    const double minLength = 32.0 / juce::jmax (1.0, audioFile.getSampleRate());

    excerpt.startTime = juce::jlimit (0.0, juce::jmax (0.0, fileLength - minLength), startTime);

    if (length <= 0.0)
    {
        excerpt.length = 0.0;
        return excerpt;
    }

    excerpt.length = juce::jlimit (minLength, juce::jmax (minLength, fileLength - excerpt.startTime), length);
    return excerpt;
}

juce::String SamplerHelpers::assignSample (te::SamplerPlugin& sampler, const juce::File& file, int keyNote)
{
    if (! file.existsAsFile())
        return "Sample file not found.";

    const auto error = sampler.addSound (file.getFullPathName(),
                                         file.getFileNameWithoutExtension(),
                                         0.0, 0.0, 0.0f);

    if (error.isNotEmpty())
        return error;

    const int soundIndex = sampler.getNumSounds() - 1;
    sampler.setSoundParams (soundIndex, keyNote, keyNote, keyNote);
    sampler.setSoundGains (soundIndex, 0.0f, 0.0f);
    return {};
}

} // namespace skeletonhive
