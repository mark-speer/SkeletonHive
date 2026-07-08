#include "EqualiserHelpers.h"

namespace skeletonhive
{

float EqualiserHelpers::getGainAtFrequency (const te::EqualiserPlugin& eq, float frequencyHz)
{
    return const_cast<te::EqualiserPlugin&> (eq).getDBGainAtFrequency (frequencyHz);
}

float EqualiserHelpers::getLowGain (const te::EqualiserPlugin& eq) { return eq.loGainValue.get(); }
float EqualiserHelpers::getLowFreq (const te::EqualiserPlugin& eq) { return eq.loFreqValue.get(); }
float EqualiserHelpers::getLowQ (const te::EqualiserPlugin& eq) { return eq.loQValue.get(); }
float EqualiserHelpers::getMidGain1 (const te::EqualiserPlugin& eq) { return eq.midGainValue1.get(); }
float EqualiserHelpers::getMidFreq1 (const te::EqualiserPlugin& eq) { return eq.midFreqValue1.get(); }
float EqualiserHelpers::getMidQ1 (const te::EqualiserPlugin& eq) { return eq.midQValue1.get(); }
float EqualiserHelpers::getMidGain2 (const te::EqualiserPlugin& eq) { return eq.midGainValue2.get(); }
float EqualiserHelpers::getMidFreq2 (const te::EqualiserPlugin& eq) { return eq.midFreqValue2.get(); }
float EqualiserHelpers::getMidQ2 (const te::EqualiserPlugin& eq) { return eq.midQValue2.get(); }
float EqualiserHelpers::getHighGain (const te::EqualiserPlugin& eq) { return eq.hiGainValue.get(); }
float EqualiserHelpers::getHighFreq (const te::EqualiserPlugin& eq) { return eq.hiFreqValue.get(); }
float EqualiserHelpers::getHighQ (const te::EqualiserPlugin& eq) { return eq.hiQValue.get(); }

void EqualiserHelpers::setLowGain (te::EqualiserPlugin& eq, float db, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setLowGain (db);
}

void EqualiserHelpers::setLowFreq (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setLowFreq (hz);
}

void EqualiserHelpers::setLowQ (te::EqualiserPlugin& eq, float q, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setLowQ (q);
}

void EqualiserHelpers::setMidGain1 (te::EqualiserPlugin& eq, float db, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setMidGain1 (db);
}

void EqualiserHelpers::setMidFreq1 (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setMidFreq1 (hz);
}

void EqualiserHelpers::setMidQ1 (te::EqualiserPlugin& eq, float q, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setMidQ1 (q);
}

void EqualiserHelpers::setMidGain2 (te::EqualiserPlugin& eq, float db, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setMidGain2 (db);
}

void EqualiserHelpers::setMidFreq2 (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setMidFreq2 (hz);
}

void EqualiserHelpers::setMidQ2 (te::EqualiserPlugin& eq, float q, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setMidQ2 (q);
}

void EqualiserHelpers::setHighGain (te::EqualiserPlugin& eq, float db, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setHighGain (db);
}

void EqualiserHelpers::setHighFreq (te::EqualiserPlugin& eq, float hz, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setHighFreq (hz);
}

void EqualiserHelpers::setHighQ (te::EqualiserPlugin& eq, float q, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    eq.setHighQ (q);
}

} // namespace skeletonhive
