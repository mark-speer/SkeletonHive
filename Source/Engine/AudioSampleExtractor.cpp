#include "AudioSampleExtractor.h"
#include "WarpEngine.h"

namespace skeletonhive
{

namespace
{

juce::AudioBuffer<float> resampleToMono (const juce::AudioBuffer<float>& input,
                                         double inputSampleRate,
                                         double outputSampleRate)
{
    if (input.getNumSamples() == 0)
        return {};

    const double ratio = outputSampleRate / inputSampleRate;
    const int outputSamples = juce::jmax (1, (int) std::ceil (input.getNumSamples() * ratio));
    juce::AudioBuffer<float> output (1, outputSamples);
    output.clear();

    juce::LagrangeInterpolator interpolator;
    interpolator.reset();
    interpolator.process (ratio,
                          input.getReadPointer (0),
                          output.getWritePointer (0),
                          outputSamples);

    return output;
}

} // namespace

ExtractedAudio AudioSampleExtractor::extract (const te::AudioClipBase& clip)
{
    ExtractedAudio result;

    const auto audioFile = WarpEngine::getSourceFile (clip);
    const auto file = audioFile.getFile();

    if (! file.existsAsFile())
    {
        result.error = "Audio clip has no valid source file.";
        return result;
    }

    auto& readManager = clip.edit.engine.getAudioFileFormatManager().readFormatManager;
    std::unique_ptr<juce::AudioFormatReader> reader (readManager.createReaderFor (file));

    if (reader == nullptr)
    {
        result.error = "Could not read the audio file.";
        return result;
    }

    const double fileSampleRate = reader->sampleRate;
    const double sourceOffset = clip.getPosition().getOffset().inSeconds();
    const double clipTimelineSeconds = clip.getPosition().getLength().inSeconds();
    const double speed = juce::jmax (0.01, clip.getSpeedRatio());
    const double availableSourceSeconds = juce::jmax (0.0, reader->lengthInSamples / fileSampleRate - sourceOffset);
    const double sourceDuration = juce::jmin (availableSourceSeconds, clipTimelineSeconds * speed);

    if (sourceDuration <= 0.01)
    {
        result.error = "Audio clip region is too short to analyse.";
        return result;
    }

    const int startSample = (int) std::llround (sourceOffset * fileSampleRate);
    const int numSamples = (int) std::llround (sourceDuration * fileSampleRate);

    if (numSamples <= 0)
    {
        result.error = "Audio clip region is too short to analyse.";
        return result;
    }

    juce::AudioBuffer<float> fileBuffer ((int) reader->numChannels, numSamples);
    reader->read (&fileBuffer, 0, numSamples, startSample, true, true);

    juce::AudioBuffer<float> mono (1, numSamples);
    mono.clear();

    for (int ch = 0; ch < fileBuffer.getNumChannels(); ++ch)
        mono.addFrom (0, 0, fileBuffer, ch, 0, numSamples, 1.0f / (float) fileBuffer.getNumChannels());

    result.mono = resampleToMono (mono, fileSampleRate, audioToMidiAnalysisSampleRate);
    result.sampleRate = audioToMidiAnalysisSampleRate;
    result.sourceStartSeconds = sourceOffset;
    result.success = result.mono.getNumSamples() > 0;
    return result;
}

} // namespace skeletonhive
