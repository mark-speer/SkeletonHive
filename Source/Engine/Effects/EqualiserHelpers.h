#pragma once

#include "TracktionCommon.h"

namespace skeletonhive
{

/** Thin helpers over te::EqualiserPlugin (4-band EQ). */
class EqualiserHelpers
{
public:
    static float getGainAtFrequency (const te::EqualiserPlugin& eq, float frequencyHz);

    static float getLowGain (const te::EqualiserPlugin& eq);
    static float getLowFreq (const te::EqualiserPlugin& eq);
    static float getLowQ (const te::EqualiserPlugin& eq);
    static float getMidGain1 (const te::EqualiserPlugin& eq);
    static float getMidFreq1 (const te::EqualiserPlugin& eq);
    static float getMidQ1 (const te::EqualiserPlugin& eq);
    static float getMidGain2 (const te::EqualiserPlugin& eq);
    static float getMidFreq2 (const te::EqualiserPlugin& eq);
    static float getMidQ2 (const te::EqualiserPlugin& eq);
    static float getHighGain (const te::EqualiserPlugin& eq);
    static float getHighFreq (const te::EqualiserPlugin& eq);
    static float getHighQ (const te::EqualiserPlugin& eq);

    static void setLowGain (te::EqualiserPlugin& eq, float db, juce::UndoManager* um);
    static void setLowFreq (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um);
    static void setLowQ (te::EqualiserPlugin& eq, float q, juce::UndoManager* um);
    static void setMidGain1 (te::EqualiserPlugin& eq, float db, juce::UndoManager* um);
    static void setMidFreq1 (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um);
    static void setMidQ1 (te::EqualiserPlugin& eq, float q, juce::UndoManager* um);
    static void setMidGain2 (te::EqualiserPlugin& eq, float db, juce::UndoManager* um);
    static void setMidFreq2 (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um);
    static void setMidQ2 (te::EqualiserPlugin& eq, float q, juce::UndoManager* um);
    static void setHighGain (te::EqualiserPlugin& eq, float db, juce::UndoManager* um);
    static void setHighFreq (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um);
    static void setHighQ (te::EqualiserPlugin& eq, float q, juce::UndoManager* um);
};

} // namespace skeletonhive
