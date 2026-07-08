#include "CompressorHelpers.h"

namespace skeletonhive
{

float CompressorHelpers::getThreshold (const te::CompressorPlugin& comp)
{
    return comp.getThreshold();
}

float CompressorHelpers::getRatio (const te::CompressorPlugin& comp)
{
    return comp.getRatio();
}

float CompressorHelpers::getAttackMs (const te::CompressorPlugin& comp)
{
    return comp.attackMs.getCurrentValue();
}

float CompressorHelpers::getReleaseMs (const te::CompressorPlugin& comp)
{
    return comp.releaseMs.getCurrentValue();
}

float CompressorHelpers::getOutputDb (const te::CompressorPlugin& comp)
{
    return comp.outputDb.getCurrentValue();
}

float CompressorHelpers::getSidechainDb (const te::CompressorPlugin& comp)
{
    return comp.sidechainDb.getCurrentValue();
}

bool CompressorHelpers::getUseSidechain (const te::CompressorPlugin& comp)
{
    return comp.useSidechainTrigger.get();
}

void CompressorHelpers::setThreshold (te::CompressorPlugin& comp, float value, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    comp.setThreshold (value);
}

void CompressorHelpers::setRatio (te::CompressorPlugin& comp, float value, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    comp.setRatio (value);
}

void CompressorHelpers::setAttackMs (te::CompressorPlugin& comp, float ms, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    comp.attackMs.setParameter (ms, juce::sendNotification);
}

void CompressorHelpers::setReleaseMs (te::CompressorPlugin& comp, float ms, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    comp.releaseMs.setParameter (ms, juce::sendNotification);
}

void CompressorHelpers::setOutputDb (te::CompressorPlugin& comp, float db, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    comp.outputDb.setParameter (db, juce::sendNotification);
}

void CompressorHelpers::setSidechainDb (te::CompressorPlugin& comp, float db, juce::UndoManager* um)
{
    juce::ignoreUnused (um);
    comp.sidechainDb.setParameter (db, juce::sendNotification);
}

void CompressorHelpers::setUseSidechain (te::CompressorPlugin& comp, bool enabled, juce::UndoManager* um)
{
    comp.useSidechainTrigger.setValue (enabled, um);
}

} // namespace skeletonhive
