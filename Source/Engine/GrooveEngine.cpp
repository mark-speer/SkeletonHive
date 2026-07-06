#include "GrooveEngine.h"

namespace skeletonhive
{

juce::Array<GrooveTemplate> GrooveEngine::defaultTemplates()
{
    juce::Array<GrooveTemplate> templates;

    {
        GrooveTemplate t;
        t.id = "builtin.straight";
        t.name = "Straight";
        t.isBuiltIn = true;
        templates.add (t);
    }
    {
        GrooveTemplate t;
        t.id = "builtin.mpc_swing";
        t.name = "MPC Swing";
        t.isBuiltIn = true;
        t.timing = { 0,0.12,0,0.12, 0,0.12,0,0.12, 0,0.12,0,0.12, 0,0.12,0,0.12 };
        templates.add (t);
    }
    {
        GrooveTemplate t;
        t.id = "builtin.laid_back";
        t.name = "Laid Back";
        t.isBuiltIn = true;
        t.timing = { 0,0.06,0.03,0.06, 0,0.06,0.03,0.06, 0,0.06,0.03,0.06, 0,0.06,0.03,0.06 };
        t.velocity = { 0,-8,-4,-8, 0,-8,-4,-8, 0,-8,-4,-8, 0,-8,-4,-8 };
        templates.add (t);
    }
    {
        GrooveTemplate t;
        t.id = "builtin.random";
        t.name = "Random";
        t.isBuiltIn = true;
        t.isRandom = true;
        templates.add (t);
    }

    return templates;
}

void GrooveEngine::applyGroove (const juce::Array<te::MidiNote*>& notes, const GrooveTemplate& groove,
                                double offsetBeats, juce::UndoManager* undoManager)
{
    if (groove.isRandom)
    {
        applyRandomHumanize (notes, 0.25, undoManager);
        return;
    }

    constexpr double sixteenthBeats = 0.25;

    for (auto* n : notes)
    {
        const double startBeat = n->getStartBeat().inBeats() - offsetBeats;
        const int rawStep = (int) std::floor (startBeat / sixteenthBeats);
        const size_t step = (size_t) (((rawStep % 16) + 16) % 16);

        const double newStart = juce::jmax (0.0, startBeat + groove.timing[step] * sixteenthBeats);
        n->setStartAndLength (te::BeatPosition::fromBeats (newStart + offsetBeats), n->getLengthBeats(), undoManager);

        if (groove.velocity[step] != 0)
            n->setVelocity (juce::jlimit (1, 127, n->getVelocity() + groove.velocity[step]), undoManager);
    }
}

void GrooveEngine::applyRandomHumanize (const juce::Array<te::MidiNote*>& notes, double gridIntervalBeats,
                                        juce::UndoManager* undoManager)
{
    auto& rng = juce::Random::getSystemRandom();
    const double maxTimeJitter = gridIntervalBeats * 0.1;

    for (auto* n : notes)
    {
        const double jitter = (rng.nextDouble() * 2.0 - 1.0) * maxTimeJitter;
        const double newStart = juce::jmax (0.0, n->getStartBeat().inBeats() + jitter);
        n->setStartAndLength (te::BeatPosition::fromBeats (newStart), n->getLengthBeats(), undoManager);

        const int newVelocity = juce::jlimit (1, 127, n->getVelocity() + rng.nextInt ({ -10, 11 }));
        n->setVelocity (newVelocity, undoManager);
    }
}

} // namespace skeletonhive
