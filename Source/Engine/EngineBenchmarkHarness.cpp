#include "EngineBenchmarkHarness.h"

#if JUCE_DEBUG

#include "EngineHelpers.h"
#include "ExportManager.h"

namespace skeletonhive
{

namespace
{
juce::File createBenchmarkToneFile (te::Engine& engine, double durationSeconds = 2.0)
{
    const auto tempDir = engine.getTemporaryFileManager().getTempDirectory();
    const auto file = tempDir.getChildFile ("EngineBenchmarkTone.wav");

    if (file.existsAsFile())
        return file;

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());

    if (stream == nullptr)
        return {};

    const double sampleRate = engine.getDeviceManager().getSampleRate();
    const int numSamples = (int) (sampleRate * durationSeconds);
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(),
                                                                          sampleRate,
                                                                          1,
                                                                          16,
                                                                          {},
                                                                          0));

    if (writer == nullptr)
        return {};

    stream.release();
    juce::AudioBuffer<float> buffer (1, numSamples);

    for (int i = 0; i < numSamples; ++i)
        buffer.setSample (0, i, 0.2f * std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * (double) i / sampleRate));

    writer->writeFromAudioSampleBuffer (buffer, 0, numSamples);
    return file;
}
} // namespace

EngineBenchmarkHarness::Results EngineBenchmarkHarness::runFullSuite (te::Engine& engine,
                                                                      te::Edit& edit,
                                                                      int trackCount)
{
    Results results;
    results.trackCount = trackCount;
    results.populateMs = populateAudioStressProject (edit, trackCount, 1);
    results.cacheReadMs = logAudioFileCacheStats (engine);

    if (auto* track = EngineHelpers::getOrInsertAudioTrack (edit))
    {
        results.freezeMs = measureFreezeMs (*track);
        results.renderMs = measureRenderMs (edit);
    }

    logResults (results);
    return results;
}

void EngineBenchmarkHarness::logResults (const Results& results)
{
    DBG ("Engine benchmark: populate " << results.trackCount << " tracks "
         << juce::String (results.populateMs, 1) << " ms");
    DBG ("Engine benchmark: freeze track " << juce::String (results.freezeMs, 1) << " ms");
    DBG ("Engine benchmark: render loop " << juce::String (results.renderMs, 1) << " ms");
    DBG ("Engine benchmark: AudioFileCache lastBlockReadMs=" << juce::String (results.cacheReadMs, 2));
}

double EngineBenchmarkHarness::populateAudioStressProject (te::Edit& edit, int trackCount, int clipsPerTrack)
{
    const double startMs = juce::Time::getMillisecondCounterHiRes();
    const auto toneFile = createBenchmarkToneFile (edit.engine);

    for (int t = 0; t < trackCount; ++t)
    {
        if (auto* track = EngineHelpers::getOrInsertAudioTrackAt (edit, edit.getTrackList().size()))
        {
            track->setName ("Bench " + juce::String (t + 1));

            for (int c = 0; c < clipsPerTrack; ++c)
            {
                if (toneFile.existsAsFile())
                {
                    const auto start = edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) c * 4.0));
                    const auto end = edit.tempoSequence.toTime (te::BeatPosition::fromBeats ((double) (c + 1) * 4.0));
                    EngineHelpers::insertWaveClipFromFile (*track, toneFile, start, "Tone");
                    juce::ignoreUnused (end);
                }
            }
        }
    }

    return juce::Time::getMillisecondCounterHiRes() - startMs;
}

double EngineBenchmarkHarness::measureFreezeMs (te::AudioTrack& track)
{
    if (track.isFrozen (te::Track::individualFreeze))
        track.setFrozen (false, te::Track::individualFreeze);

    const double startMs = juce::Time::getMillisecondCounterHiRes();
    track.setFrozen (true, te::Track::individualFreeze);
    return juce::Time::getMillisecondCounterHiRes() - startMs;
}

double EngineBenchmarkHarness::measureRenderMs (te::Edit& edit)
{
    const auto tempFile = edit.engine.getTemporaryFileManager().getTempDirectory()
                              .getChildFile ("EngineBenchmarkRender.wav");

    ExportManager::RenderScope scope;
    scope.time = edit.getTransport().getLoopRange().getLength() > 0s
                     ? edit.getTransport().getLoopRange()
                     : te::TimeRange { 0s, edit.tempoSequence.toTime (te::BeatPosition::fromBeats (16.0)) };

    if (auto* track = EngineHelpers::getOrInsertAudioTrack (edit))
        scope.tracks.add (track);

    const double startMs = juce::Time::getMillisecondCounterHiRes();
    ExportManager::renderScopeToFile (edit, tempFile, scope);
    return juce::Time::getMillisecondCounterHiRes() - startMs;
}

double EngineBenchmarkHarness::logAudioFileCacheStats (te::Engine& engine)
{
    juce::ignoreUnused (engine);
    return 0.0;
}

} // namespace skeletonhive

#endif
