#include "AudioToMidiHarmony.h"

namespace skeletonhive
{

namespace
{

constexpr int numChordRoots = 12;
constexpr int medianWindow = 5;
constexpr double minChordDurationSeconds = 0.12;

struct ChordTemplate
{
    int root = 0;
    bool minor = false;
    float profile[12] {};
};

float dotProfile (const ChromaFrame& frame, const ChordTemplate& chord)
{
    float score = 0.0f;

    for (int i = 0; i < 12; ++i)
    {
        const int rotated = (i + chord.root) % 12;
        score += frame.bins[i] * chord.profile[rotated];
    }

    return score;
}

juce::Array<ChordTemplate> buildChordTemplates()
{
    juce::Array<ChordTemplate> chords;

    for (int root = 0; root < numChordRoots; ++root)
    {
        ChordTemplate major;
        major.root = root;
        major.minor = false;
        major.profile[(0 + 12) % 12] = 1.0f;
        major.profile[(4 + 12) % 12] = 0.85f;
        major.profile[(7 + 12) % 12] = 0.95f;
        chords.add (major);

        ChordTemplate minor;
        minor.root = root;
        minor.minor = true;
        minor.profile[(0 + 12) % 12] = 1.0f;
        minor.profile[(3 + 12) % 12] = 0.85f;
        minor.profile[(7 + 12) % 12] = 0.95f;
        chords.add (minor);
    }

    return chords;
}

int bestChordIndex (const ChromaFrame& frame, const juce::Array<ChordTemplate>& templates)
{
    int bestIndex = 0;
    float bestScore = -1.0f;

    for (int i = 0; i < templates.size(); ++i)
    {
        const float score = dotProfile (frame, templates.getReference (i));
        if (score > bestScore)
        {
            bestScore = score;
            bestIndex = i;
        }
    }

    return bestIndex;
}

ChromaFrame medianSmooth (const juce::Array<ChromaFrame>& frames, int centreIndex)
{
    ChromaFrame result;
    juce::Array<float> window;

    for (int pc = 0; pc < 12; ++pc)
    {
        window.clearQuick();

        for (int offset = -medianWindow; offset <= medianWindow; ++offset)
        {
            const int index = centreIndex + offset;
            if (index >= 0 && index < frames.size())
                window.add (frames.getReference (index).bins[pc]);
        }

        window.sort();
        result.bins[pc] = window.isEmpty() ? 0.0f : window.getReference (window.size() / 2);
    }

    return result;
}

void appendBlockChord (juce::Array<TranscribedNote>& notes,
                       const ChordTemplate& chord,
                       double startSeconds,
                       double endSeconds,
                       int velocity)
{
    const int rootPitch = 48 + chord.root;
    const int thirdPitch = rootPitch + (chord.minor ? 3 : 4);
    const int fifthPitch = rootPitch + 7;

    for (const int pitch : { rootPitch, thirdPitch, fifthPitch })
    {
        TranscribedNote note;
        note.sourceStartSeconds = startSeconds;
        note.sourceEndSeconds = endSeconds;
        note.pitch = juce::jlimit (0, 127, pitch);
        note.velocity = velocity;
        notes.add (note);
    }
}

} // namespace

TranscriptionResult AudioToMidiHarmony::transcribeHarmony (const juce::Array<ChromaFrame>& chromaFrames,
                                                           double sampleRate,
                                                           int hopSize)
{
    TranscriptionResult result;

    if (chromaFrames.isEmpty())
    {
        result.error = "No chroma frames were generated.";
        return result;
    }

    const auto templates = buildChordTemplates();
    const double frameDuration = (double) hopSize / sampleRate;

    juce::Array<int> chordIndices;
    chordIndices.resize (chromaFrames.size());

    for (int i = 0; i < chromaFrames.size(); ++i)
    {
        const auto smoothed = medianSmooth (chromaFrames, i);
        chordIndices.set (i, bestChordIndex (smoothed, templates));
    }

    int segmentStart = 0;
    int currentChord = chordIndices.getFirst();

    for (int i = 1; i < chordIndices.size(); ++i)
    {
        if (chordIndices[i] == currentChord)
            continue;

        const double startSeconds = (double) segmentStart * frameDuration;
        const double endSeconds = (double) i * frameDuration;

        if (endSeconds - startSeconds >= minChordDurationSeconds)
        {
            float energy = 0.0f;
            for (int frame = segmentStart; frame < i; ++frame)
                for (float bin : chromaFrames.getReference (frame).bins)
                    energy += bin;

            const int velocity = juce::jlimit (50, 110, 60 + (int) (energy * 4.0f));
            appendBlockChord (result.notes, templates.getReference (currentChord), startSeconds, endSeconds, velocity);
        }

        segmentStart = i;
        currentChord = chordIndices[i];
    }

    const double finalStart = (double) segmentStart * frameDuration;
    const double finalEnd = (double) chromaFrames.size() * frameDuration;

    if (finalEnd - finalStart >= minChordDurationSeconds)
    {
        float energy = 0.0f;
        for (int frame = segmentStart; frame < chromaFrames.size(); ++frame)
            for (float bin : chromaFrames.getReference (frame).bins)
                energy += bin;

        const int velocity = juce::jlimit (50, 110, 60 + (int) (energy * 4.0f));
        appendBlockChord (result.notes, templates.getReference (currentChord), finalStart, finalEnd, velocity);
    }

    result.success = ! result.notes.isEmpty();
    if (! result.success)
        result.error = "No harmony chords were detected.";

    return result;
}

} // namespace skeletonhive
