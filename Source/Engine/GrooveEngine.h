#pragma once

#include "GrooveTemplate.h"

namespace skeletonhive
{

class GrooveEngine
{
public:
    static constexpr int randomGrooveIndex = 3;

    static juce::Array<GrooveTemplate> defaultTemplates();
    static void applyGroove (const juce::Array<te::MidiNote*>& notes, const GrooveTemplate& groove,
                             double offsetBeats, juce::UndoManager* undoManager);
    static void applyRandomHumanize (const juce::Array<te::MidiNote*>& notes, double gridIntervalBeats,
                                     juce::UndoManager* undoManager);
};

} // namespace skeletonhive
