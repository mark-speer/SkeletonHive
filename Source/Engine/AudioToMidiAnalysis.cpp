#include "AudioToMidiAnalysis.h"

extern "C"
{
#include <aubio.h>
}

namespace skeletonhive
{

namespace
{

constexpr uint_t defaultWinSize = 2048;
constexpr uint_t defaultHopSize = 512;
constexpr double minNoteDurationSeconds = 0.05;
constexpr double minDrumGapSeconds = 0.04;

float computeSpectralCentroid (const fvec_t* spectrum, float sampleRate, uint_t fftSize)
{
    double weighted = 0.0;
    double total = 0.0;

    for (uint_t i = 1; i < spectrum->length / 2; ++i)
    {
        const double magnitude = spectrum->data[i];
        const double frequency = (double) i * sampleRate / (double) fftSize;
        weighted += frequency * magnitude;
        total += magnitude;
    }

    if (total <= 0.0)
        return 0.0f;

    return (float) (weighted / total);
}

int mapDrumPitchFromCentroid (float centroidHz)
{
    if (centroidHz < 180.0f)
        return 36; // kick

    if (centroidHz < 2500.0f)
        return 38; // snare

    return 42; // hi-hat
}

void copyCvecNormToSpectrum (const cvec_t* fftGrain, fvec_t* spectrum)
{
    const smpl_t* magnitudes = cvec_norm_get_data (fftGrain);

    for (uint_t i = 0; i < spectrum->length; ++i)
        spectrum->data[i] = magnitudes[i];
}

} // namespace

TranscriptionResult AudioToMidiAnalysis::transcribeMelody (const juce::AudioBuffer<float>& mono,
                                                         double sampleRate)
{
    TranscriptionResult result;

    if (mono.getNumSamples() <= 0)
    {
        result.error = "No audio samples to analyse.";
        return result;
    }

    const uint_t hopSize = defaultHopSize;
    const uint_t winSize = defaultWinSize;
    const smpl_t samplerate = (smpl_t) sampleRate;

    aubio_notes_t* notesDetector = new_aubio_notes ("default", winSize, hopSize, samplerate);

    if (notesDetector == nullptr)
    {
        result.error = "Could not initialise melody analysis.";
        return result;
    }

    aubio_notes_set_silence (notesDetector, -70.0f);

    fvec_t* input = new_fvec (hopSize);
    fvec_t* notesOut = new_fvec (3);

    const auto* samples = mono.getReadPointer (0);
    const int totalSamples = mono.getNumSamples();

    double pendingStart = -1.0;
    int pendingPitch = 60;
    int pendingVelocity = 100;

    for (int frame = 0; frame + (int) hopSize <= totalSamples; ++frame)
    {
        for (uint_t i = 0; i < hopSize; ++i)
            input->data[i] = samples[frame + (int) i];

        aubio_notes_do (notesDetector, input, notesOut);

        const double frameTime = (double) frame / sampleRate;

        if (notesOut->data[0] > 0.0f)
        {
            if (pendingStart >= 0.0)
            {
                TranscribedNote note;
                note.sourceStartSeconds = pendingStart;
                note.sourceEndSeconds = frameTime;
                note.pitch = pendingPitch;
                note.velocity = pendingVelocity;

                if (note.sourceEndSeconds - note.sourceStartSeconds >= minNoteDurationSeconds)
                    result.notes.add (note);
            }

            pendingStart = frameTime;
            pendingPitch = juce::jlimit (0, 127, (int) std::lround (notesOut->data[1]));
            pendingVelocity = juce::jlimit (1, 127, (int) std::lround (notesOut->data[2] * 127.0f));
            if (pendingVelocity <= 0)
                pendingVelocity = 100;
        }
        else if (notesOut->data[0] < 0.0f && pendingStart >= 0.0)
        {
            TranscribedNote note;
            note.sourceStartSeconds = pendingStart;
            note.sourceEndSeconds = frameTime;
            note.pitch = pendingPitch;
            note.velocity = pendingVelocity;

            if (note.sourceEndSeconds - note.sourceStartSeconds >= minNoteDurationSeconds)
                result.notes.add (note);

            pendingStart = -1.0;
        }
    }

    if (pendingStart >= 0.0)
    {
        const double endTime = (double) totalSamples / sampleRate;
        TranscribedNote note;
        note.sourceStartSeconds = pendingStart;
        note.sourceEndSeconds = endTime;
        note.pitch = pendingPitch;
        note.velocity = pendingVelocity;

        if (note.sourceEndSeconds - note.sourceStartSeconds >= minNoteDurationSeconds)
            result.notes.add (note);
    }

    del_aubio_notes (notesDetector);
    del_fvec (input);
    del_fvec (notesOut);

    result.success = ! result.notes.isEmpty();
    if (! result.success)
        result.error = "No melody notes were detected.";

    return result;
}

TranscriptionResult AudioToMidiAnalysis::transcribeDrums (const juce::AudioBuffer<float>& mono,
                                                          double sampleRate)
{
    TranscriptionResult result;

    if (mono.getNumSamples() <= 0)
    {
        result.error = "No audio samples to analyse.";
        return result;
    }

    const uint_t hopSize = defaultHopSize;
    const uint_t winSize = defaultWinSize;
    const smpl_t samplerate = (smpl_t) sampleRate;

    aubio_onset_t* onsetDetector = new_aubio_onset ("default", winSize, hopSize, samplerate);
    aubio_fft_t* fft = new_aubio_fft (winSize);
    fvec_t* input = new_fvec (hopSize);
    fvec_t* onsetOut = new_fvec (1);
    cvec_t* fftGrain = new_cvec (winSize);
    fvec_t* spectrum = new_fvec (winSize / 2 + 1);

    if (onsetDetector == nullptr || fft == nullptr)
    {
        result.error = "Could not initialise drum analysis.";
        del_aubio_onset (onsetDetector);
        del_aubio_fft (fft);
        del_fvec (input);
        del_fvec (onsetOut);
        del_cvec (fftGrain);
        del_fvec (spectrum);
        return result;
    }

    aubio_onset_set_silence (onsetDetector, -70.0f);

    const auto* samples = mono.getReadPointer (0);
    const int totalSamples = mono.getNumSamples();
    double lastHitTime = -10.0;

    for (int frame = 0; frame + (int) hopSize <= totalSamples; frame += (int) hopSize)
    {
        for (uint_t i = 0; i < hopSize; ++i)
            input->data[i] = samples[frame + (int) i];

        aubio_onset_do (onsetDetector, input, onsetOut);

        if (onsetOut->data[0] <= 0.0f)
            continue;

        const double hitTime = (double) frame / sampleRate;

        if (hitTime - lastHitTime < minDrumGapSeconds)
            continue;

        lastHitTime = hitTime;

        fvec_t* window = new_fvec (winSize);
        for (uint_t i = 0; i < winSize; ++i)
        {
            const int index = frame + (int) i;
            window->data[i] = (index >= 0 && index < totalSamples) ? samples[index] : 0.0f;
        }

        aubio_fft_do (fft, window, fftGrain);
        copyCvecNormToSpectrum (fftGrain, spectrum);
        del_fvec (window);

        const float centroid = computeSpectralCentroid (spectrum, (float) sampleRate, winSize);

        TranscribedNote note;
        note.sourceStartSeconds = hitTime;
        note.sourceEndSeconds = hitTime + 0.08;
        note.pitch = mapDrumPitchFromCentroid (centroid);
        note.velocity = juce::jlimit (60, 127, 70 + (int) (centroid / 80.0f));
        result.notes.add (note);
    }

    del_aubio_onset (onsetDetector);
    del_aubio_fft (fft);
    del_fvec (input);
    del_fvec (onsetOut);
    del_cvec (fftGrain);
    del_fvec (spectrum);

    result.success = ! result.notes.isEmpty();
    if (! result.success)
        result.error = "No drum hits were detected.";

    return result;
}

juce::Array<ChromaFrame> AudioToMidiAnalysis::computeChromaFrames (const juce::AudioBuffer<float>& mono,
                                                                   double sampleRate,
                                                                   int hopSize,
                                                                   int fftSize)
{
    juce::Array<ChromaFrame> frames;

    if (mono.getNumSamples() <= 0)
        return frames;

    aubio_fft_t* fft = new_aubio_fft ((uint_t) fftSize);
    fvec_t* input = new_fvec ((uint_t) fftSize);
    cvec_t* fftGrain = new_cvec ((uint_t) fftSize);
    fvec_t* spectrum = new_fvec ((uint_t) (fftSize / 2 + 1));

    const auto* samples = mono.getReadPointer (0);
    const int totalSamples = mono.getNumSamples();

    for (int frame = 0; frame + fftSize <= totalSamples; frame += hopSize)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            const int index = frame + i;
            input->data[(uint_t) i] = index < totalSamples ? samples[index] : 0.0f;
        }

        aubio_fft_do (fft, input, fftGrain);
        copyCvecNormToSpectrum (fftGrain, spectrum);

        ChromaFrame chroma;

        for (uint_t bin = 1; bin < spectrum->length; ++bin)
        {
            const float magnitude = spectrum->data[bin];
            if (magnitude <= 0.0f)
                continue;

            const float frequency = (float) bin * (float) sampleRate / (float) fftSize;
            if (frequency < 50.0f || frequency > 5000.0f)
                continue;

            const float midi = 69.0f + 12.0f * std::log2 (frequency / 440.0f);
            const int pitchClass = ((int) std::lround (midi) % 12 + 12) % 12;
            chroma.bins[pitchClass] += magnitude;
        }

        float maxBin = 0.0f;
        for (float bin : chroma.bins)
            maxBin = juce::jmax (maxBin, bin);

        if (maxBin > 0.0f)
        {
            for (float& bin : chroma.bins)
                bin /= maxBin;
        }

        frames.add (chroma);
    }

    del_aubio_fft (fft);
    del_fvec (input);
    del_cvec (fftGrain);
    del_fvec (spectrum);
    return frames;
}

} // namespace skeletonhive
